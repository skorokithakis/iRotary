//#define LCD
#define GSM

#define PIN_RING 10
#define PIN_DIALING 4
#define PIN_PULSE 5
#define PIN_BUTTON_HALF 6
#define PIN_BUTTON_FULL 7
#define PIN_HOOK 11
#define NUMBER_LENGTH 10

#if defined(GSM)
#include "SIM900.h"
#include <SoftwareSerial.h>
#include "call.h"

CallGSM call;
unsigned long statusCheckTime = 0;
byte lastStatus = CALL_NONE;
#endif

#if defined(LCD)
#include <LiquidCrystal.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
#endif

enum states {
    IDLE,
    RINGING,
    RECEIVER_UP,
    IN_CALL
};

int pulses = 0;
char edge = 0;
char buttonFullEdge = 0;
char buttonHalfEdge = 0;
char hookEdge = 0;
char dialing = 1;
states state = IDLE;
String lastNumber = "";
String number = "";


void setup() {
    pinMode(PIN_DIALING, INPUT_PULLUP);
    pinMode(PIN_PULSE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_HALF, INPUT_PULLUP);
    pinMode(PIN_BUTTON_FULL, INPUT_PULLUP);
    pinMode(PIN_HOOK, INPUT_PULLUP);
    pinMode(PIN_RING, OUTPUT);
    pinMode(13, OUTPUT);

    digitalWrite(PIN_RING, HIGH);

    Serial.begin(9600);
#if defined(GSM)
    if (gsm.begin(2400)) {
        Serial.println("\nstatus=READY");
    } else {
        Serial.println("\nstatus=IDLE");
    }
#endif

    Serial.println("Ready.");
#if defined(LCD)
    lcd.begin(16, 2);
#endif

    digitalWrite(PIN_RING, LOW);
}


///////////////////////////////////////////////////////////////////////
// Events
//
void buttonFullPressed() {
    if (lastNumber == "") {
        Serial.println("No number to dial.");
    } else {
        Serial.print("Redialing last number: ");
        Serial.println(lastNumber);
        dialNumber(lastNumber);
    }
}

void buttonFullReleased() {
}

void buttonHalfPressed() {
}

void buttonHalfReleased() {
}

void hookPressed() {
#if defined(GSM)
    call.HangUp();
#endif
    Serial.println("Hook pressed.");
    state = IDLE;
}

void hookReleased() {
    number = "";
    pulses = 0;
    Serial.println("Hook released.");

    if (state == RINGING) {
#if defined(GSM)
        call.PickUp();
#endif
        state = IN_CALL;
    } else {
        state = RECEIVER_UP;
    }
}

void ringingStarted() {
    Serial.println("Ringing started.");
    digitalWrite(PIN_RING, HIGH);
    state = RINGING;
}

void ringingStopped() {
    Serial.println("Ringing stopped.");
    digitalWrite(PIN_RING, LOW);
}


///////////////////////////////////////////////////////////////////////
// Helpers
//
void checkButtonHalfPressed() {
    if (digitalRead(PIN_BUTTON_HALF) == LOW) {
        if (buttonHalfEdge == 0) {
            buttonHalfPressed();
        }
        buttonHalfEdge = 1;
    } else {
        if (buttonHalfEdge == 1) {
            buttonHalfReleased();
        }
        buttonHalfEdge = 0;
    }
}

void checkButtonFullPressed() {
    if (digitalRead(PIN_BUTTON_FULL) == LOW) {
        if (buttonFullEdge == 0) {
            buttonFullPressed();
        }
        buttonFullEdge = 1;
    } else {
        if (buttonFullEdge == 1) {
            buttonFullReleased();
        }
        buttonFullEdge = 0;
    }
}

bool checkHookChanged() {
    if (digitalRead(PIN_HOOK) == LOW) {
        if (hookEdge == 0) {
            hookReleased();
        }
        hookEdge = 1;
    } else {
        if (hookEdge == 1) {
            hookPressed();
        }
        hookEdge = 0;
    }
}

char getDigit() {
    int digit;

    if (pulses == 0) {
        return -1;
    }
    digit = pulses % 10;
    pulses = 0;
    return digit;
}

void showNumber(String number) {
#if defined(LCD)
    lcd.clear();
    lcd.print(number);
#endif

    Serial.print(number);
    Serial.print("\r");
}

void dialNumber(String number) {
    char numArray[NUMBER_LENGTH+1];
    if (state != RECEIVER_UP) {
        Serial.println("Invalid state for dialing, returning...");
        return;
    }

#if defined(LCD)
    lcd.setCursor(0, 1);
    lcd.print("Dialing...");
#endif

    Serial.print("\r\n");
    Serial.print("Dialing...");
    Serial.print("\r\n");
    number.toCharArray(numArray, NUMBER_LENGTH+1);
#if defined(GSM)
    call.Call(numArray);
#endif
    lastNumber = number;
    state = IN_CALL;
}


///////////////////////////////////////////////////////////////////////
// Loop functions
//
void checkStatus() {
#if defined(GSM)
    byte status;

    if (millis() - 1000 > statusCheckTime) {
        statusCheckTime = millis();
        Serial.println("Checking status...");

        status = call.CallStatus();
        if (lastStatus != status && status == CALL_INCOM_VOICE) {
            ringingStarted();
        }
        if (lastStatus != status && lastStatus == CALL_INCOM_VOICE) {
            ringingStopped();
        }
        lastStatus = status;
    }

#endif
}

void readPulses() {
    char pinPulse = digitalRead(PIN_PULSE);

    if (dialing == 1) {
        pulses = 0;
        return;
    }

    if (pinPulse == HIGH && edge == 0) {
        pulses++;
        edge = 1;
    } else if (pinPulse == LOW && edge == 1) {
        edge = 0;
    }
}

void readDialing() {
    char digit;
    bool finalDigit = false;
    char pinDialing = digitalRead(PIN_DIALING);

    if (pinDialing == 1 && dialing == 0) {
        digit = getDigit();
        if (digit != -1) {
            number += (int)digit;
            showNumber(number);

            if (number.length() == NUMBER_LENGTH) {
                dialNumber(number);
                number = "";
            }
        }
    }
    dialing = pinDialing;
}

void loop() {
    if (state == RECEIVER_UP) {
        readPulses();
        readDialing();

        // Hax. This increases the accuracy, I have no idea why.
        delay(3);
    } else if (state == IDLE) {
        checkStatus();
    }

    checkButtonFullPressed();
    checkButtonHalfPressed();
    checkHookChanged();
}

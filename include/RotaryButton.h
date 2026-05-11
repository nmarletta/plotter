// RotaryButton.h

#ifndef ROTARY_BUTTON_H
#define ROTARY_BUTTON_H

#include <Arduino.h>
#include "globals.h"

class RotaryButton {
public:
    RotaryButton(uint8_t pinA, uint8_t pinB, uint8_t pushButton);

    void begin();
    void encoderUpdate();
    bool turned();
    void buttonUpdate();
    bool pressed();
    bool isDown();
    int getPosition() const;
    void setPosition(int newPosition);
    void constrainPosition(int min, int max);

private:
    static RotaryButton* instance;
    static void encoderISR();
    static void buttonISR();
    bool debounceCheck();

    static const int8_t enc_states[16];
    uint8_t PIN_A, PIN_B, PUSH_BUTTON;
    unsigned long lastIncReadTime, lastDecReadTime, lastButtonPress;
    int pauseLength = 25000;
    int fastIncrement = 1;
    bool buttonDown = false;
    bool buttonRead = false;
    const unsigned long debounceDelay = 150;
    volatile int encoderPosition = 0;
    int lastEncoderPosition = 0;
};

#endif // ROTARY_BUTTON_H


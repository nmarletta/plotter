#include "RotaryButton.h"

RotaryButton* RotaryButton::instance = nullptr;

const int8_t RotaryButton::enc_states[] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

RotaryButton::RotaryButton(uint8_t pinA, uint8_t pinB, uint8_t pushButton)
  : PIN_A(pinA), PIN_B(pinB), PUSH_BUTTON(pushButton) {
  instance = this;  // Set the instance pointer to this object
}

void RotaryButton::begin() {
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PUSH_BUTTON, INPUT_PULLUP);

  // Encoder is polled in turned() — no ISRs needed.
  // Using INPUT_PULLUP; encoder common wire must connect to GND.
  lastIncReadTime = micros();
  lastDecReadTime = micros();
}

void RotaryButton::encoderUpdate() {
  static uint8_t old_AB = 3;
  static int8_t encval = 0;

  old_AB <<= 2;
  uint8_t a = digitalRead(PIN_A);
  uint8_t b = digitalRead(PIN_B);
  if (a) old_AB |= 0x02;
  if (b) old_AB |= 0x01;

  encval += enc_states[(old_AB & 0x0f)];

  if (encval > 3) {
    int changeValue = 1;
    if ((micros() - lastIncReadTime) < pauseLength) {
      changeValue = fastIncrement * changeValue;
    }
    lastIncReadTime = micros();
    encoderPosition += changeValue;
    encval = 0;
  } else if (encval < -3) {
    int changeValue = -1;
    if ((micros() - lastDecReadTime) < pauseLength) {
      changeValue = fastIncrement * changeValue;
    }
    lastDecReadTime = micros();
    encoderPosition += changeValue;
    encval = 0;
  }
}

bool RotaryButton::turned() {
  encoderUpdate(); // poll on every call — no ISR needed
  if (encoderPosition != lastEncoderPosition) {
    lastEncoderPosition = encoderPosition;
    return true;
  }
  return false;
}

void RotaryButton::buttonUpdate() {
  if (debounceCheck()) {
    bool nowDown = (digitalRead(PUSH_BUTTON) == LOW);
    if (nowDown && !buttonDown) {
      // Leading edge: button just pressed
      buttonRead = false;
    }
    buttonDown = nowDown;
  }
}

bool RotaryButton::pressed() {
  buttonUpdate();
  if (buttonDown && !buttonRead) {
    buttonRead = true;
    return true;
  }
  return false;
}

bool RotaryButton::isDown() {
  buttonUpdate();
  return buttonDown;
}

int RotaryButton::getPosition() const {
  return encoderPosition;
}

void RotaryButton::setPosition(int newPosition) {
  encoderPosition = newPosition;
}

void RotaryButton::constrainPosition(int min, int max) {
  if (encoderPosition < min) {
    encoderPosition = min;
  }
  if (encoderPosition > max) {
    encoderPosition = max;
  }
}

bool RotaryButton::debounceCheck() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonPress > debounceDelay) {
    lastButtonPress = currentMillis;
    return true;
  }
  return false;
}

void RotaryButton::encoderISR() {
  if (instance) {
    instance->encoderUpdate();
  }
}

void RotaryButton::buttonISR() {
  if (instance) {
    instance->buttonUpdate();
  }
}

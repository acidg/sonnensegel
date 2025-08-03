#include "button_handler.h"

ButtonHandler::ButtonHandler(uint8_t buttonPin) 
    : pin(buttonPin), lastState(HIGH), currentState(HIGH), 
      lastDebounceTime(0), pressStartTime(0), longPressHandled(false), shortPressHandled(false) {
}

void ButtonHandler::begin() {
    pinMode(pin, INPUT_PULLUP);
}

bool ButtonHandler::isDebounced() const {
    return (millis() - lastDebounceTime) > BUTTON_DEBOUNCE_MS;
}

void ButtonHandler::updatePressTime() {
    if (currentState == LOW) {
        pressStartTime = millis();
        longPressHandled = false;
        shortPressHandled = false;
    }
}

ButtonAction ButtonHandler::checkPressType() {
    if (currentState == HIGH && !longPressHandled && !shortPressHandled) {
        unsigned long pressDuration = millis() - pressStartTime;
        if (pressDuration < BUTTON_LONG_PRESS_MS) {
            shortPressHandled = true;
            return BUTTON_SHORT_PRESS;
        }
    }
    
    if (currentState == LOW && !longPressHandled) {
        unsigned long pressDuration = millis() - pressStartTime;
        if (pressDuration >= BUTTON_LONG_PRESS_MS) {
            longPressHandled = true;
            return BUTTON_LONG_PRESS;
        }
    }
    
    return BUTTON_NONE;
}

ButtonAction ButtonHandler::update() {
    bool reading = digitalRead(pin);
    
    if (reading != lastState) {
        lastDebounceTime = millis();
    }
    
    if (!isDebounced()) {
        lastState = reading;
        return BUTTON_NONE;
    }
    
    if (reading == currentState) {
        lastState = reading;
        return checkPressType();
    }
    
    currentState = reading;
    if (currentState == LOW) {
        updatePressTime();
    }
    
    lastState = reading;
    return checkPressType();
}
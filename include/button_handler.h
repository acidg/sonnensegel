#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "constants.h"

enum ButtonAction {
    BUTTON_NONE,
    BUTTON_SHORT_PRESS,
    BUTTON_LONG_PRESS
};

class ButtonHandler {
private:
    uint8_t pin;
    bool lastState;
    bool currentState;
    unsigned long lastDebounceTime;
    unsigned long pressStartTime;
    bool longPressHandled;
    
    bool isDebounced() const;
    void updatePressTime();
    ButtonAction checkPressType();
    
public:
    ButtonHandler(uint8_t buttonPin);
    void begin();
    ButtonAction update();
};

#endif // BUTTON_HANDLER_H
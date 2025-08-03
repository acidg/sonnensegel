#ifndef WIND_SENSOR_H
#define WIND_SENSOR_H

#include <Arduino.h>
#include "pins.h"
#include "constants.h"

class WindSensor {
private:
    volatile unsigned long* pulseCountPtr;
    unsigned long lastPulseCount;
    unsigned long lastCheckTime;
    unsigned long pulsesPerMinute;
    unsigned long pulseThreshold;
    bool safetyTriggered;
    
public:
    WindSensor(volatile unsigned long* pulseCounter);
    void begin();
    void update();
    void setThreshold(unsigned long threshold);
    
    unsigned long getPulsesPerMinute() const { return pulsesPerMinute; }
    unsigned long getThreshold() const { return pulseThreshold; }
    bool isSafetyTriggered() const { return safetyTriggered; }
    void resetSafetyTrigger() { safetyTriggered = false; }
};

#endif // WIND_SENSOR_H
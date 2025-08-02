#ifndef WIND_SENSOR_H
#define WIND_SENSOR_H

#include <Arduino.h>
#include "pins.h"
#include "constants.h"

class WindSensor {
private:
    volatile unsigned long* pulseCountPtr;
    unsigned long lastPulseCount;
    unsigned long lastMeasurementTime;
    unsigned long lastSafetyCheckTime;
    float currentSpeed;
    float speedThreshold;
    float conversionFactor;
    bool safetyTriggered;
    
    float calculateSpeed(unsigned long pulseDelta, float timeDelta) const;
    bool isTimeToMeasure() const;
    bool isTimeForSafetyCheck() const;
    
public:
    WindSensor(volatile unsigned long* pulseCounter);
    void begin();
    void update();
    void setThreshold(float threshold);
    void setConversionFactor(float factor);
    
    float getCurrentSpeed() const { return currentSpeed; }
    float getThreshold() const { return speedThreshold; }
    float getConversionFactor() const { return conversionFactor; }
    bool isSafetyTriggered() const { return safetyTriggered; }
    void resetSafetyTrigger() { safetyTriggered = false; }
};

#endif // WIND_SENSOR_H
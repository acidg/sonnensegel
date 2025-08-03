#include "wind_sensor.h"

WindSensor::WindSensor(volatile unsigned long* pulseCounter)
    : pulseCountPtr(pulseCounter), lastPulseCount(0), 
      lastCheckTime(0), pulsesPerMinute(0),
      pulseThreshold(DEFAULT_WIND_PULSE_THRESHOLD), safetyTriggered(false) {
}

void WindSensor::begin() {
    pinMode(WIND_SENSOR_PIN, INPUT_PULLUP);
    lastCheckTime = millis();
}

void WindSensor::update() {
    unsigned long now = millis();
    
    // Check every minute (60000ms)
    if (now - lastCheckTime >= 60000) {
        unsigned long currentPulseCount = *pulseCountPtr;
        pulsesPerMinute = currentPulseCount - lastPulseCount;
        
        // Check if pulses exceed threshold
        if (pulseThreshold > 0 && pulsesPerMinute > pulseThreshold) {
            if (!safetyTriggered) {
                safetyTriggered = true;
                Serial.print("Wind safety triggered! Pulses: ");
                Serial.print(pulsesPerMinute);
                Serial.print(" > Threshold: ");
                Serial.println(pulseThreshold);
            }
        } else {
            safetyTriggered = false;
        }
        
        lastPulseCount = currentPulseCount;
        lastCheckTime = now;
    }
}

void WindSensor::setThreshold(unsigned long threshold) {
    pulseThreshold = constrain(threshold, MIN_WIND_PULSE_THRESHOLD, MAX_WIND_PULSE_THRESHOLD);
}
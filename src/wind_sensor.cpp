#include "wind_sensor.h"

WindSensor::WindSensor(volatile unsigned long* pulseCounter)
    : pulseCountPtr(pulseCounter), lastPulseCount(0), 
      lastMeasurementTime(0), lastSafetyCheckTime(0),
      currentSpeed(0.0), speedThreshold(DEFAULT_WIND_THRESHOLD),
      conversionFactor(DEFAULT_WIND_FACTOR), safetyTriggered(false) {
}

void WindSensor::begin() {
    pinMode(WIND_SENSOR_PIN, INPUT_PULLUP);
}

float WindSensor::calculateSpeed(unsigned long pulseDelta, float timeDelta) const {
    float pulsesPerSecond = pulseDelta / timeDelta;
    return pulsesPerSecond * conversionFactor;
}

bool WindSensor::isTimeToMeasure() const {
    return (millis() - lastMeasurementTime) >= WIND_MEASUREMENT_INTERVAL_MS;
}

bool WindSensor::isTimeForSafetyCheck() const {
    return (millis() - lastSafetyCheckTime) >= WIND_SAFETY_CHECK_INTERVAL_MS;
}

void WindSensor::update() {
    unsigned long now = millis();
    
    if (isTimeToMeasure()) {
        unsigned long currentPulseCount = *pulseCountPtr;
        unsigned long pulseDelta = currentPulseCount - lastPulseCount;
        float timeDelta = (now - lastMeasurementTime) / 1000.0;
        
        currentSpeed = calculateSpeed(pulseDelta, timeDelta);
        
        lastPulseCount = currentPulseCount;
        lastMeasurementTime = now;
    }
    
    if (isTimeForSafetyCheck()) {
        lastSafetyCheckTime = now;
        
        if (speedThreshold > 0 && currentSpeed > speedThreshold) {
            if (!safetyTriggered) {
                safetyTriggered = true;
                Serial.print("Wind safety triggered! Speed: ");
                Serial.print(currentSpeed);
                Serial.print(" > Threshold: ");
                Serial.println(speedThreshold);
            }
        } else {
            safetyTriggered = false;
        }
    }
}

void WindSensor::setThreshold(float threshold) {
    speedThreshold = constrain(threshold, MIN_WIND_THRESHOLD, MAX_WIND_THRESHOLD);
}

void WindSensor::setConversionFactor(float factor) {
    conversionFactor = constrain(factor, MIN_WIND_FACTOR, MAX_WIND_FACTOR);
}
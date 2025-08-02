#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "constants.h"

struct StorageData {
    float position;
    unsigned long travelTime;
    float windThreshold;
    float windFactor;
};

class Storage {
private:
    static const int POSITION_ADDR = 0;
    static const int TRAVEL_TIME_ADDR = 4;
    static const int WIND_THRESHOLD_ADDR = 8;
    static const int WIND_FACTOR_ADDR = 12;
    static const int MAGIC_ADDR = 16;
    
    bool isValidData() const;
    void writeDefaults();
    
public:
    void begin();
    StorageData load();
    void save(const StorageData& data);
};

#endif // STORAGE_H
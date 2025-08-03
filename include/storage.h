#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>
#include "constants.h"

struct StorageData {
    float position;
    unsigned long travelTime;
    unsigned long windThreshold;
};

class Storage {
private:
    static const int POSITION_ADDR = 0;
    static const int TRAVEL_TIME_ADDR = 4;
    static const int WIND_THRESHOLD_ADDR = 8;
    static const int MAGIC_ADDR = 12;
    
    bool isValidData() const;
    void writeDefaults();
    
public:
    void begin();
    StorageData load();
    void save(const StorageData& data);
};

#endif // STORAGE_H
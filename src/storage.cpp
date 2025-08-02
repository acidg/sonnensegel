#include "storage.h"
#include "config.h"

void Storage::begin() {
    EEPROM.begin(EEPROM_SIZE);
}

bool Storage::isValidData() const {
    uint32_t magic;
    EEPROM.get(MAGIC_ADDR, magic);
    return magic == EEPROM_MAGIC_VALUE;
}

void Storage::writeDefaults() {
    StorageData defaults = {
        .position = 0.0,
        .travelTime = DEFAULT_TRAVEL_TIME_MS,
        .windThreshold = DEFAULT_WIND_THRESHOLD,
        .windFactor = DEFAULT_WIND_FACTOR
    };
    save(defaults);
}

StorageData Storage::load() {
    StorageData data;
    
    if (!isValidData()) {
        writeDefaults();
    }
    
    EEPROM.get(POSITION_ADDR, data.position);
    EEPROM.get(TRAVEL_TIME_ADDR, data.travelTime);
    EEPROM.get(WIND_THRESHOLD_ADDR, data.windThreshold);
    EEPROM.get(WIND_FACTOR_ADDR, data.windFactor);
    
    // Validate loaded data
    data.position = constrain(data.position, MIN_POSITION, MAX_POSITION);
    data.travelTime = constrain(data.travelTime, MIN_TRAVEL_TIME_MS, MAX_TRAVEL_TIME_MS);
    data.windThreshold = constrain(data.windThreshold, MIN_WIND_THRESHOLD, MAX_WIND_THRESHOLD);
    data.windFactor = constrain(data.windFactor, MIN_WIND_FACTOR, MAX_WIND_FACTOR);
    
    return data;
}

void Storage::save(const StorageData& data) {
    EEPROM.put(POSITION_ADDR, data.position);
    EEPROM.put(TRAVEL_TIME_ADDR, data.travelTime);
    EEPROM.put(WIND_THRESHOLD_ADDR, data.windThreshold);
    EEPROM.put(WIND_FACTOR_ADDR, data.windFactor);
    EEPROM.put(MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.commit();
}
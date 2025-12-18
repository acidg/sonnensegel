# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266-based awning controller with MQTT/Home Assistant integration, local button control, wind safety features, and time-based position tracking. Built for LC Technology ESP8266 ESP-12F WiFi relay module.

## Build & Development Commands

```bash
# Build the project
pio run

# Upload to ESP8266
pio run --target upload

# Monitor serial output (115200 baud, includes exception decoder)
pio device monitor

# Build and upload in one command
pio run --target upload && pio device monitor
```

## Architecture

### Core Control Flow (main.cpp)

The main loop follows this priority order:
1. WiFi management (handles connection, AP fallback, captive portal)
2. Network service initialization (mDNS, web interface, MQTT when WiFi connected)
3. **Button inputs** (highest priority - can interrupt motor operations)
4. Motor control updates (only if no button was just pressed)
5. Wind safety checks
6. Network services (MDNS, web interface, MQTT if enabled)

### Component Architecture

**Motor Control System:**
- `MotorController`: Sends relay pulses to motor controller. Supports direction changes without stop pulse (`startWithoutStop`) and normal starts with stop pulse.
- `PositionTracker`: Maintains current/target positions (0-100%), calculates position changes based on travel time. Uses time-based estimation, not sensors.
- Direction changes handled by stopping motor with `sendStopPulse=false`, then starting in new direction.

**Configuration & Storage:**
- `ConfigManager`: Manages all settings (WiFi, MQTT, awning params) in EEPROM with checksum validation.
- Settings auto-saved when motor stops, especially at end positions.
- All config structs defined in config_manager.h: `WiFiConfig`, `MQTTConfig`, `AwningConfig`, `SystemConfig`.

**Network Stack:**
- `WiFiManager`: Handles WiFi connection with automatic AP fallback after timeout. Runs captive portal for configuration when in AP mode.
- `WebInterface`: Serves control interface and configuration pages. Always available when WiFi connected.
- `MqttHandler`: Home Assistant integration with autodiscovery. Can be enabled/disabled independently. Uses callback pattern for commands.

**Safety & Input:**
- `WindSensor`: Monitors anemometer pulses (ISR-based). Triggers automatic retraction when threshold exceeded.
- `ButtonHandler`: Debounced button with short press (stop) and long press (full movement) detection.

### Key Behavioral Patterns

**Position Tracking:**
- Purely time-based calculation using travel time calibration
- Position updated every 100ms while motor running
- Current position persisted to EEPROM on every stop

**Motor Control:**
- Relay interlock prevents both relays active simultaneously
- Start pulse: 1000ms, Stop pulse: 100ms
- Direction changes skip stop pulse for smoother transitions
- Motor controller handles actual stop pulse generation

**Network Service Lifecycle:**
- Services (mDNS, web, MQTT) only initialize when WiFi connected
- MQTT can be independently enabled/disabled via config
- mDNS announces device as `{hostname}.local` with HTTP service

**Button Priority:**
- Buttons checked before motor control updates
- Short press immediately stops and sets target to current position
- Long press sets target to 0% (retract) or 100% (extend)
- `buttonPressed` flag prevents motor control conflicts same iteration

## Pin Definitions

See `include/pins.h` for GPIO assignments:
- GPIO14: Relay 1 (Extend)
- GPIO12: Relay 2 (Retract)
- GPIO2: Extend button (active low, internal pullup)
- GPIO4: Retract button (active low, internal pullup)
- GPIO5: Wind sensor (pulse input with interrupt)

## Configuration Files

- `platformio.ini`: Build settings, optimized for ESP8266 memory constraints
- EEPROM magic value: `0xDEADBEEF` for config validation
- Default travel time: 15 seconds
- Default wind threshold: 100 pulses/minute

## Important Constants (constants.h)

- Motor pulse timings: START=1000ms, STOP=100ms
- Button debounce: 20ms, long press threshold: 1000ms
- Position update interval: 100ms
- MQTT reconnect: 5s interval, with exponential backoff after 5 failed attempts
- Position tolerance: 1.0% for target matching

## Code Organization

- `include/`: All header files with class definitions
- `src/`: Implementation files matching headers
- Single-responsibility classes following embedded C++ patterns
- No Arduino String class for memory efficiency - char arrays used throughout

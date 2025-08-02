# ESP8266 Awning Controller

A complete ESP8266-based awning controller with wind sensor support, MQTT integration for Home Assistant, and local button control.

## Features

- **Motor Control**: Interfaces with awning motor controller via relay pulses
- **Local Control**: Physical buttons for extend/retract with short press (stop) and long press (full movement)
- **Position Tracking**: Time-based position calculation with EEPROM persistence
- **Wind Protection**: Automatic retraction when wind speed exceeds threshold
- **MQTT Integration**: Full Home Assistant compatibility
- **Safety Features**: Relay interlocks and automatic end-stop detection
- **Calibration**: Adjustable travel time and wind sensor parameters

## Hardware Requirements

- LC Technology ESP8266 ESP-12F WiFi 4-channel relay module
- Two momentary push buttons for local control
- Wind sensor (anemometer with pulse output)
- 5V power supply

## Wiring Diagram

```
ESP8266 GPIO Connections:
┌─────────────────────────────────────┐
│ ESP8266 ESP-12F Module              │
│                                     │
│ GPIO14 ─────> Relay 1 (Extend)      │
│ GPIO12 ─────> Relay 2 (Retract)     │
│                                     │
│ GPIO2  <───── Extend Button ──┐     │
│                              ╧ GND  │
│                                     │
│ GPIO4  <───── Retract Button ─┐     │
│                              ╧ GND  │
│                                     │
│ GPIO5  <───── Wind Sensor Pulse     │
│ GND    <───── Wind Sensor GND       │
│ 3.3V   ────> Wind Sensor VCC        │
│                                     │
│ VCC    <───── 5V Power Supply       │
│ GND    <───── Power Supply GND      │
└─────────────────────────────────────┘

Motor Controller Connections:
┌─────────────────────────────────────┐
│ Awning Motor Controller             │
│                                     │
│ Extend Input  <───── Relay 1 (NO)   │
│ Retract Input <───── Relay 2 (NO)   │
│ Common        <───── Relay COM      │
│                                     │
└─────────────────────────────────────┘
```

## Build Instructions

### Using PlatformIO (Recommended)

1. Install [PlatformIO](https://platformio.org/install)
2. Clone this repository
3. Update WiFi and MQTT settings in `include/config.h`
4. Build and upload:
   ```bash
   # Build the project
   pio run
   
   # Upload to ESP8266
   pio run --target upload
   
   # Monitor serial output
   pio device monitor
   ```

### Using Arduino IDE

1. Install Arduino IDE and ESP8266 board support
2. Install required libraries:
   - PubSubClient
   - ArduinoJson
3. Copy all files to Arduino project folder
4. Update WiFi and MQTT settings in `config.h`
5. Select board: "NodeMCU 1.0 (ESP-12E Module)"
6. Upload to device

## Configuration

### WiFi and MQTT Settings

Edit `include/config.h`:

```cpp
// WiFi Configuration
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT Configuration
#define MQTT_SERVER "your_mqtt_server"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_CLIENT_ID "awning_controller"
#define MQTT_BASE_TOPIC "home/awning"
```

### Default Parameters

```cpp
#define DEFAULT_TRAVEL_TIME_MS 15000  // 15 seconds
#define DEFAULT_WIND_THRESHOLD 25.0   // 25 km/h
#define DEFAULT_WIND_FACTOR 2.5       // pulses/sec to km/h
```

## Calibration Procedure

### 1. Travel Time Calibration

The travel time is the duration needed for the awning to move from fully retracted (0%) to fully extended (100%).

**Method 1: Via MQTT**
```bash
# Set travel time to 20 seconds (20000 ms)
mosquitto_pub -h your_mqtt_server -t "home/awning/calibrate" -m "20000"
```

**Method 2: Via Home Assistant**
Use the input_number slider in the UI (see homeassistant.yaml)

**Calibration Steps:**
1. Fully retract the awning
2. Time how long it takes to fully extend
3. Set this value as the travel time

### 2. Wind Sensor Calibration

The wind sensor outputs pulses that need to be converted to wind speed.

**Calculate Conversion Factor:**
1. Check your anemometer specifications for pulses per rotation
2. Measure the cup radius (distance from center to cup center)
3. Calculate: `Factor = (2 * π * radius * 3.6) / pulses_per_rotation`

**Set via MQTT:**
```bash
# Set wind conversion factor
mosquitto_pub -h your_mqtt_server -t "home/awning/set_wind_factor" -m "2.5"

# Set wind safety threshold (km/h)
mosquitto_pub -h your_mqtt_server -t "home/awning/set_wind_threshold" -m "30"
```

## MQTT Topics

### Command Topics (Subscribe)
- `home/awning/set` - Commands: OPEN, CLOSE, STOP
- `home/awning/set_position` - Target position (0-100)
- `home/awning/calibrate` - Set travel time in milliseconds
- `home/awning/set_wind_threshold` - Set wind speed threshold (km/h)
- `home/awning/set_wind_factor` - Set wind conversion factor

### Status Topics (Publish)
- `home/awning/state` - Current state: opening, closing, stopped
- `home/awning/position` - Current position (0-100)
- `home/awning/availability` - online/offline
- `home/awning/wind_speed` - Current wind speed (km/h)
- `home/awning/wind_threshold` - Current threshold setting
- `home/awning/wind_factor` - Current conversion factor

## Home Assistant Integration

1. Copy the contents of `homeassistant.yaml` to your HA configuration
2. Restart Home Assistant
3. The awning will appear as a cover entity with wind sensor data

## Operation

### Button Controls
- **Short Press** (< 1 second): Stop movement
- **Long Press** (≥ 1 second): Start movement in button direction

### Wind Protection
When wind speed exceeds the threshold:
- Awning automatically retracts to 0%
- Manual control temporarily disabled
- Resumes normal operation when wind drops

### Safety Features
- Relay interlock prevents simultaneous activation
- Automatic stop at end positions
- Position saved to EEPROM every stop
- Continues operation without WiFi/MQTT

## Troubleshooting

### No WiFi Connection
- Check credentials in config.h
- Verify router settings (2.4GHz network)
- Buttons continue to work offline

### Position Drift
- Recalibrate travel time
- Check for mechanical slippage
- Ensure consistent motor speed

### Wind Sensor Issues
- Verify pulse signal with oscilloscope
- Check pull-up resistor on GPIO5
- Adjust conversion factor for accuracy

### MQTT Connection Failed
- Verify server address and port
- Check username/password
- Ensure unique client ID
- Check MQTT broker logs

## LED Status Indicators (Optional)

Add status LEDs to unused GPIO pins:
- **Blue LED**: WiFi connected
- **Green LED**: MQTT connected
- **Red LED**: Wind safety activated

## Safety Notes

1. Never operate during storms or high winds
2. Ensure proper weatherproofing of electronics
3. Use appropriate fuses/circuit breakers
4. Test emergency stop functionality regularly
5. Mount wind sensor in unobstructed location

## License

This project is open source. Feel free to modify and distribute.
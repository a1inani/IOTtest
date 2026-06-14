# Sensor Logger – ESP32-C3 Super Mini

A Wi-Fi sensor dashboard for the **ESP32-C3 Super Mini**.  
Reads **temperature, humidity, pressure, and altitude** (BME280), **water/liquid level** (SEN0193), **soil pH** (analog sensor), and **rain/moisture** (Grove water sensor).  
Stores history locally on internal flash and serves a captive-portal web dashboard over its own Wi-Fi access point – no router or internet connection required.

---

## Features

| Feature | Detail |
|---------|--------|
| **BME280** | Temperature (°C & °F), Humidity (%), Pressure (hPa), Altitude (m, derived) |
| **SEN0193** | Analog water/liquid level – raw ADC value and mapped 0–100 % |
| **Soil pH** | Raw ADC reading and an **estimated** pH value (linear calibration; requires calibration for accuracy – see below) |
| **Grove rain sensor** | Raw ADC value + rain detected boolean |
| **Pump relay** | On/off control via web API; safety auto-off timer |
| **RAM ring buffer** | Last 120 readings (~20 min at 10 s interval) instantly available |
| **Flash persistence** | Readings appended to a CSV on LittleFS, rotated at ~50 KB |
| **Wi-Fi AP** | Broadcasts `SensorLogger` – phone or laptop connects directly |
| **Captive portal** | DNS wildcard resolves all hostnames to the device; connecting clients are automatically redirected to the dashboard on most platforms |

### HTTP Endpoints

| Path | Method | Description |
|------|--------|-------------|
| `/` | GET | HTML dashboard – auto-refreshes every 5 s |
| `/api/current` | GET | JSON – most recent reading + pump state |
| `/api/history` | GET | JSON – last 50 RAM-buffer readings |
| `/api/pump` | GET | JSON – current pump relay state |
| `/api/pump/on` | POST | Energise pump relay; returns pump-state JSON |
| `/api/pump/off` | POST | De-energise pump relay; returns pump-state JSON |
| `/api/pump/on` | GET | Backward-compatible route; redirects to `/` |
| `/api/pump/off` | GET | Backward-compatible route; redirects to `/` |
| `/api/log` | GET | Full persistent CSV log (direct download) |
| `/api/log/clear` | GET | Erase the persistent CSV log, redirect to `/` |

---

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| **ESP32-C3 Super Mini** | Main microcontroller (Wi-Fi, ADC, I²C, GPIO) |
| **BME280** | I²C environmental sensor: temperature, humidity, barometric pressure |
| **SEN0193** | DFRobot analog capacitive liquid/water level sensor |
| **Analog soil pH sensor** | Generic analog pH module (e.g. DFRobot SEN0169 or similar) |
| **Grove water/rain sensor** | Seeed Grove moisture/rain detection board (analog output) |
| **Relay module** | Single-channel relay for submersible pump switching (active-LOW or active-HIGH) |
| **Submersible pump** | DC or AC pump controlled through the relay |

### Pin Assignment

| GPIO | Role | Notes |
|------|------|-------|
| **0** | SEN0193 signal (ADC1\_CH0) | Analog water/liquid level |
| **1** | Pump relay control (digital out) | Active-LOW by default (`PUMP_RELAY_ACTIVE_LEVEL`) |
| **2** | Grove rain sensor (ADC1\_CH2) | Also a strapping pin – see §GPIO Notes |
| **3** | Soil pH sensor signal (ADC1\_CH3) | See §Pin Assignment Notes |
| **5** | BME280 SDA | Configurable – change `BME280_SDA_PIN` in `config.h` |
| **6** | BME280 SCL | Configurable – change `BME280_SCL_PIN` in `config.h` |

### Wiring

```
BME280          →   ESP32-C3 Super Mini
─────────────────────────────────────────────
VCC              3.3 V
GND              GND
SDA              GPIO 5   ← BME280_SDA_PIN (configurable)
SCL              GPIO 6   ← BME280_SCL_PIN (configurable)

SEN0193 (water level sensor)
─────────────────────────────────────────────
VCC              3.3 V (or 5 V if module requires)
GND              GND
SIGNAL           GPIO 0   ← WATER_LEVEL_PIN

Soil pH sensor
─────────────────────────────────────────────
VCC              3.3 V (or 5 V if module requires)
GND              GND
SIGNAL           GPIO 3   ← SOIL_PH_PIN  (see Pin Assignment Notes)

Grove water/rain sensor
─────────────────────────────────────────────
VCC              3.3 V (or 5 V)
GND              GND
SIGNAL (A0)      GPIO 2   ← RAIN_SENSOR_PIN

Relay module
─────────────────────────────────────────────
VCC              3.3 V (or 5 V – check module spec)
GND              GND
IN               GPIO 1   ← PUMP_RELAY_PIN
COM              Pump supply wire
NO (or NC)       Pump supply wire (use NO for normally-open; pump runs when relay energised)
```

> **Tip:** Use a flyback diode across the relay coil if driving it directly from GPIO (most relay modules already include one).

### Pin Assignment Notes

The **ESP32-C3 SoC supports ADC only on GPIO 0–4** (ADC1).  
GPIO 7 and GPIO 8 are digital-only GPIO pins on this chip and **cannot be used as analog inputs**.  
For this reason the soil pH sensor signal is connected to **GPIO 3** (ADC1\_CH3), which is the next available ADC pin after GPIO 0 (SEN0193) and GPIO 2 (rain sensor).

If your pH module includes a second analog output for temperature compensation, connect it to **GPIO 4** (ADC1\_CH4) and read it with `analogRead(4)` as needed.

### GPIO Notes

- **GPIO 2** is a strapping pin on the ESP32-C3.  It must be **low or floating during boot**.  If your rain sensor drives it high at power-on, the ESP32 may fail to boot.  Most Grove rain sensor modules do not drive the output high at startup, but verify your specific module.  If you encounter boot failures, add a **10 kΩ pull-down resistor** between GPIO 2 and GND to hold the pin low during startup, or temporarily disconnect the sensor signal wire during the first power-on test.
- **GPIO 9** is the BOOT/FLASH button strapping pin.  It is not used in this project.

---

## Software Requirements

### Arduino IDE

[Arduino IDE 2.x](https://www.arduino.cc/en/software) (or Arduino CLI ≥ 0.35).

### Board Package – ESP32-C3 Super Mini

1. Open **File → Preferences**
2. Add the following URL to *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for **esp32**, and install **esp32 by Espressif Systems** (≥ 3.0.0).
4. Select board: **Tools → Board → esp32 → ESP32C3 Dev Module**  
   *(The "Super Mini" is a form-factor variant of the standard ESP32-C3 dev board.)*

> **USB serial on ESP32-C3 Super Mini**: the Super Mini uses the ESP32-C3's built-in USB Serial/JTAG peripheral.  
> In Arduino IDE, set **Tools → USB CDC On Boot → Enabled** to see Serial Monitor output without a separate USB-UART adapter.

### Libraries

Install via **Sketch → Include Library → Manage Libraries…** or with Arduino CLI:

```bash
arduino-cli lib install "Adafruit BME280 Library"
arduino-cli lib install "Adafruit Unified Sensor"
```

| Library | Author | Min version |
|---------|--------|-------------|
| Adafruit BME280 Library | Adafruit | 2.2.2 |
| Adafruit Unified Sensor | Adafruit | 1.1.9 |

All other dependencies (`WiFi`, `WebServer`, `DNSServer`, `Wire`, `LittleFS`) are included in the ESP32 Arduino core.

---

## Build & Flash

### Arduino IDE

1. Open `sensor_logger/sensor_logger.ino`
2. **Tools → Board → esp32 → ESP32C3 Dev Module**
3. **Tools → USB CDC On Boot → Enabled**
4. **Tools → Port** → select your device's COM / tty port
5. Click **Upload** (Ctrl+U)

On first boot LittleFS is automatically formatted – no separate filesystem upload step is needed.

### Arduino CLI

```bash
# Compile
arduino-cli compile \
  --fqbn esp32:esp32:esp32c3 \
  --build-property "build.extra_flags=-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1" \
  sensor_logger/

# Upload  (replace /dev/ttyUSB0 with your port)
arduino-cli upload \
  --fqbn esp32:esp32:esp32c3 \
  --port /dev/ttyUSB0 \
  sensor_logger/
```

---

## Usage

1. After flashing, open the Serial Monitor at **115200 baud** to confirm startup output.
2. On your phone or laptop, connect to Wi-Fi network **`SensorLogger`** (password: `SL-sensor-1!`).
3. A captive-portal popup should appear automatically – tap it to open the dashboard.
4. If no popup appears, open a browser and go to **`http://192.168.4.1/`**.

---

## Configuration

All tuneable constants live in `sensor_logger/config.h`:

| Constant | Default | Description |
|----------|---------|-------------|
| `AP_SSID` | `"SensorLogger"` | Wi-Fi network name |
| `AP_PASSWORD` | `"SL-sensor-1!"` | Wi-Fi password (≥ 8 chars; `""` = open) |
| `BME280_SDA_PIN` | `5` | I²C SDA GPIO for BME280 |
| `BME280_SCL_PIN` | `6` | I²C SCL GPIO for BME280 |
| `BME280_I2C_ADDR` | `0x76` | BME280 I²C address (0x77 if SDO→VCC) |
| `SEA_LEVEL_PRESSURE_HPA` | `1013.25` | Reference pressure for altitude derivation |
| `WATER_LEVEL_PIN` | `0` | GPIO for SEN0193 signal (ADC1\_CH0) |
| `SOIL_PH_PIN` | `3` | GPIO for pH sensor signal (ADC1\_CH3) |
| `PH_SLOPE` | `-5.70` | pH calibration slope (pH/V) |
| `PH_INTERCEPT` | `21.34` | pH calibration intercept |
| `RAIN_SENSOR_PIN` | `2` | GPIO for Grove rain sensor (ADC1\_CH2) |
| `RAIN_THRESHOLD` | `500` | ADC threshold for rain detection (0–4095) |
| `PUMP_RELAY_PIN` | `1` | GPIO for relay control (digital output) |
| `PUMP_RELAY_ACTIVE_LEVEL` | `LOW` | `LOW` = active-LOW relay; `HIGH` = active-HIGH |
| `PUMP_MAX_ON_MS` | `60000` | Pump auto-off safety timer (ms); 0 = disabled |
| `SAMPLE_INTERVAL_MS` | `10000` | Milliseconds between sensor readings |
| `RAM_BUFFER_SIZE` | `120` | Readings kept in RAM (~20 min at 10 s) |
| `PERSIST_EVERY_N` | `6` | Flush to flash every N readings (~1 min at 10 s) |
| `LOG_FILE_PATH` | `"/sensor_log.csv"` | LittleFS path for the persistent log |
| `LOG_MAX_BYTES` | `51200` | Log file size before rotation (~50 KB) |
| `HTTP_PORT` | `80` | HTTP server port |

---

## Project Structure

```
sensor_logger/
├── sensor_logger.ino   Main sketch – setup() and loop()
├── config.h            All tuneable constants and pin assignments
├── SampleStore.h       Sensor + storage public API declarations
├── SampleStore.cpp     Sensor polling, RAM ring buffer, LittleFS CSV log, pump control
├── WebUI.h             Web server public API declarations
└── WebUI.cpp           Wi-Fi AP, DNS captive portal, HTTP routes, embedded HTML dashboard
README.md
```

---

## CSV Log Format

The persistent log (`/api/log`) is a plain CSV with a header row:

```
uptime_ms,temperature_c,temperature_f,humidity,pressure_hpa,altitude_m(derived),water_level_raw,water_level_pct,ph_raw,ph_value(estimated),rain_raw,rain_detected
12345,25.3,77.5,60.1,1013.2,12.4,2048,50,1890,6.83,320,0
```

- `altitude_m` is derived from barometric pressure and the configured sea-level reference.
- `ph_value` is an **estimated** value based on the linear calibration model; calibrate before trusting it.
- BME280 columns are **empty** (not zero) when the sensor returned an error.

---

## Notes

### pH Sensor Calibration

The default `PH_SLOPE` and `PH_INTERCEPT` values are approximations suitable for generic Arduino-compatible pH modules at ~25 °C.  **They will not be accurate for your specific sensor without calibration.**

**Two-point calibration procedure:**

1. Obtain pH 4.0 and pH 7.0 buffer solutions.
2. Rinse and dry the probe between measurements.
3. Immerse the probe in pH 7.0 buffer solution.  Note the ADC raw value from the Serial Monitor or `/api/current`.  Convert to voltage: `V7 = raw × 3.3 / 4095`.
4. Immerse the probe in pH 4.0 buffer solution.  Note the raw value.  Convert: `V4 = raw × 3.3 / 4095`.
5. Compute new coefficients:
   ```
   PH_SLOPE      = (7.0 - 4.0) / (V7 - V4)
   PH_INTERCEPT  = 7.0 - PH_SLOPE × V7
   ```
6. Update `PH_SLOPE` and `PH_INTERCEPT` in `config.h` and reflash.

> Temperature significantly affects pH electrode output.  For best accuracy, calibrate at the temperature at which you will be measuring, or use a pH module with built-in temperature compensation.

### SEN0193 Water Level

The SEN0193 sensor outputs an analog voltage proportional to immersion depth.  The firmware reads the raw ADC value (0–4095) and maps it linearly to 0–100 %.  This is an approximation; actual behaviour depends on the liquid type and sensor placement.  Calibrate `water_level_pct` against a known reference if precise measurements are needed.

### Altitude Derivation

The altitude value is **derived** from the BME280 barometric pressure measurement using the standard hypsometric formula.  Accuracy depends on the `SEA_LEVEL_PRESSURE_HPA` reference in `config.h`.  Update this value with the current local QNH (available from weather services) for accurate results.  Expect errors of several tens of metres with the default value.

### Pump Safety

- The relay is driven to the **OFF state immediately** at startup, before any other initialisation, to prevent the pump from running during a reboot.
- The `PUMP_MAX_ON_MS` safety timer (default 60 s) automatically de-energises the relay even if the device loses connectivity or the firmware hangs after a turn-on command.
- **Do not leave the pump running unattended for extended periods** without appropriate flow sensors or overflow protection in your physical installation.
- Use an appropriately rated relay for your pump's voltage and current.

### Pump Troubleshooting

If pump behaviour does not match the dashboard, use this checklist:

1. **Pump turns on at boot while dashboard says OFF**
   - This is usually a relay polarity mismatch.
   - In `sensor_logger/config.h`, switch `PUMP_RELAY_ACTIVE_LEVEL` (`LOW` ↔ `HIGH`), reflash, and retest.
2. **ON/OFF controls seem inverted**
   - Your relay board is likely the opposite polarity of the current configuration.
   - Flip `PUMP_RELAY_ACTIVE_LEVEL`, reflash, and verify again.
3. **Dashboard button appears ineffective**
   - The dashboard uses `fetch()` calls to `POST /api/pump/on` and `POST /api/pump/off`.
   - Check Serial Monitor logs for pump command lines that print active/off levels and the exact GPIO level written.
   - Check the dashboard pump diagnostics line (`active`, `off`, and `current pin`) for polarity mismatch clues.

> **Safety first:** test relay switching **without the pump attached** before live pump testing.

### Flash Wear

Readings are flushed to internal flash every `PERSIST_EVERY_N` samples (default: one write per minute).  The log is rotated when it exceeds `LOG_MAX_BYTES`.  This keeps flash write cycles low and well within the ESP32's endurance specification.

### Security Note

The `AP_PASSWORD` in `config.h` is a default intended only for initial testing.  
**Change it to a unique, strong password before any real-world deployment.**

### Captive Portal Behaviour

Captive portal behaviour varies between operating systems.  The device runs a DNS server that answers every query with `192.168.4.1` and handles the well-known OS-specific detection URLs.  If the automatic popup does not appear, open a browser and navigate to `http://192.168.4.1/`.

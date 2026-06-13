# Sensor Logger – XIAO ESP32-C6 + DHT11

A Wi-Fi sensor dashboard for the [Seeed Studio XIAO ESP32-C6](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/).  
Reads **temperature**, **humidity**, and a computed **heat index** from a DHT11 sensor, stores history locally, and serves a captive-portal web dashboard over its own Wi-Fi access point – no router or internet connection required.

---

## Features

| Feature | Detail |
|---------|--------|
| **Sensor readings** | Temperature (°C & °F), Relative Humidity (%), Heat Index (°C & °F) |
| **Heat index** | *Derived* – calculated from temperature + humidity (Steadman formula); clearly labelled throughout the UI and in every data format |
| **RAM ring buffer** | Last 120 readings (~20 min at 10 s interval) instantly available |
| **Flash persistence** | Readings appended to a CSV on LittleFS, rotated at ~50 KB |
| **Wi-Fi AP** | Broadcasts `SensorLogger` – phone or laptop connects directly |
| **Captive portal** | DNS wildcard resolves all hostnames to the device; connecting clients are automatically redirected to the dashboard on most platforms |
| **Web endpoints** | See table below |

### HTTP Endpoints

| Path | Method | Description |
|------|--------|-------------|
| `/` | GET | HTML dashboard – auto-refreshes every 5 s |
| `/api/current` | GET | JSON – most recent reading |
| `/api/history` | GET | JSON – last 50 RAM-buffer readings |
| `/api/log` | GET | Full persistent CSV log (direct download) |
| `/api/log/clear` | GET | Erase the persistent CSV log, redirect to `/` |

---

## Hardware

| Component | Detail |
|-----------|--------|
| Board | Seeed Studio XIAO ESP32-C6 |
| Sensor | DHT11 (temperature + humidity) |

### Wiring

```
DHT11 pin     →    XIAO ESP32-C6 pin
──────────────────────────────────────────────
VCC  (pin 1)       3.3 V  (3V3 pad)
DATA (pin 2)       D2     (GPIO 4)   ← configurable in config.h
GND  (pin 4)       GND
```

> **Tip:** A 10 kΩ pull-up resistor between DATA and VCC is recommended.  
> Most DHT11 breakout boards include this resistor.

---

## Software Requirements

### Arduino IDE

[Arduino IDE 2.x](https://www.arduino.cc/en/software) (or Arduino CLI ≥ 0.35).

### Board Package

Install the **esp32 by Espressif Systems** board package (≥ 3.0.0):

1. Open **File → Preferences**
2. Add the following URL to *Additional boards manager URLs*:  
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search for **esp32**, and install.

### Libraries

Install both libraries via **Sketch → Include Library → Manage Libraries…** or with the Arduino CLI:

```bash
arduino-cli lib install "DHT sensor library"
arduino-cli lib install "Adafruit Unified Sensor"
```

| Library | Author | Min version |
|---------|--------|-------------|
| DHT sensor library | Adafruit | 1.4.4 |
| Adafruit Unified Sensor | Adafruit | 1.1.9 |

No other third-party libraries are needed – `WiFi`, `WebServer`, `DNSServer`, and `LittleFS` are all part of the ESP32 Arduino core.

---

## Build & Flash

### Arduino IDE

1. Open `sensor_logger/sensor_logger.ino`
2. **Tools → Board → esp32 → XIAO_ESP32C6**
3. **Tools → Port** → select your device's COM / tty port
4. Click **Upload** (Ctrl+U)

On first boot LittleFS is automatically formatted – no separate filesystem upload step is needed.

### Arduino CLI

```bash
# Compile
arduino-cli compile \
  --fqbn esp32:esp32:XIAO_ESP32C6 \
  sensor_logger/

# Upload  (replace /dev/ttyUSB0 with your port)
arduino-cli upload \
  --fqbn esp32:esp32:XIAO_ESP32C6 \
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
| `AP_PASSWORD` | `"SL-sensor-1!"` | Wi-Fi password (≥ 8 chars; `""` = open network) |
| `DHT_PIN` | `4` | GPIO number for DHT11 data line (D2 on XIAO ESP32-C6) |
| `DHT_TYPE` | `DHT11` | Sensor type passed to the Adafruit library |
| `SAMPLE_INTERVAL_MS` | `10000` | Milliseconds between readings (DHT11 minimum: 2000) |
| `RAM_BUFFER_SIZE` | `120` | Readings kept in RAM (~20 min at 10 s) |
| `PERSIST_EVERY_N` | `6` | Flush to flash every N valid readings (~1 min at 10 s) |
| `LOG_FILE_PATH` | `"/sensor_log.csv"` | LittleFS path for the persistent log |
| `LOG_MAX_BYTES` | `51200` | Log file size before rotation (~50 KB) |
| `HTTP_PORT` | `80` | HTTP server port |

---

## Project Structure

```
sensor_logger/
├── sensor_logger.ino   Main sketch – setup() and loop()
├── config.h            All tuneable constants
├── SampleStore.h       Sensor + storage public API declarations
├── SampleStore.cpp     DHT11 polling, RAM ring buffer, LittleFS CSV log
├── WebUI.h             Web server public API declarations
└── WebUI.cpp           Wi-Fi AP, DNS captive portal, HTTP routes, embedded HTML
README.md
```

---

## CSV Log Format

The persistent log (`/api/log`) is a plain CSV file with a header row:

```
uptime_ms,temperature_c,temperature_f,humidity,heat_index_c(derived),heat_index_f(derived)
12345,24.0,75.2,55.0,23.5,74.3
```

The `(derived)` suffix in the column names is a reminder that those values are computed, not directly measured.

---

## Notes

### Heat Index

The heat index is a *derived* value – it is **not** directly measured by the DHT11.  
It is computed using the Steadman formula as implemented in the Adafruit DHT library.  
The formula is most meaningful at temperatures ≥ 27 °C (80 °F) and humidity ≥ 40 %.  
At lower values the number is still computed and displayed; treat it as informational.

### Flash Wear

Readings are only flushed to internal flash every `PERSIST_EVERY_N` samples (default: one write per minute at 10 s interval).  
The log is rotated (the old file is deleted and a new one started) when it exceeds `LOG_MAX_BYTES`.  
This keeps write cycles low and well within the ESP32's flash endurance specification.

### Security Note

The `AP_PASSWORD` in `config.h` is a default intended only for initial testing.
**Change it to a unique, strong password before any real-world deployment.**


### Captive Portal Behaviour

Captive portal behaviour varies between operating systems and browser versions.  
The device runs a DNS server that answers every query with `192.168.4.1` and handles the well-known OS-specific detection URLs (Android `/generate_204`, iOS `/hotspot-detect.html`, Windows `/connecttest.txt`, Firefox `/canonical.html`).  
If the automatic popup does not appear, simply open a browser and navigate to `http://192.168.4.1/`.

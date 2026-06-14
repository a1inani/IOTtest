#pragma once

// ─── Wi-Fi Access Point ───────────────────────────────────────────────────────
// IMPORTANT: Change AP_PASSWORD before deploying.
#define AP_SSID      "SensorLogger"
#define AP_PASSWORD  "SL-sensor-1!"  // must be ≥8 characters for WPA2; use "" for open

// ─── BME280 – I²C temperature / humidity / pressure ───────────────────────────
// Default I²C pins for the ESP32-C3 Super Mini.
// Change BME280_SDA_PIN / BME280_SCL_PIN to match your wiring if needed.
#define BME280_SDA_PIN  10
#define BME280_SCL_PIN  9
// I²C address: 0x76 when SDO is tied to GND (most breakout boards); 0x77 when SDO → VCC.
#define BME280_I2C_ADDR  0x76
// Reference sea-level pressure used for the derived altitude calculation (hPa / mbar).
// Adjust to your local QNH for accurate altitude readings.
#define SEA_LEVEL_PRESSURE_HPA  1013.25f

// ─── SEN0193 – Analog capacitive water / liquid level sensor ──────────────────
// GPIO 0 = ADC1_CH0 on the ESP32-C3 Super Mini.
#define WATER_LEVEL_PIN  0

// ─── Analog soil pH sensor ────────────────────────────────────────────────────
// Per user request, use GPIO 7 / GPIO 8 for the soil pH sensor interface.
// Primary pH analog signal is read from GPIO 7.
// If your module exposes a second analog channel (e.g. compensation/reference),
// GPIO 8 is reserved for that secondary channel.
#define SOIL_PH_PIN       7
#define SOIL_PH_AUX_PIN   8

// pH calibration – linear model: pH = PH_SLOPE × voltage + PH_INTERCEPT
// These are approximate defaults for a generic analog pH sensor at 25 °C.
// *** Readings are ESTIMATES until you calibrate with pH 4.0 and pH 7.0 buffers. ***
// See README.md §"pH Sensor Calibration" for the calibration procedure.
#define PH_SLOPE      (-5.70f)   // pH units per volt
#define PH_INTERCEPT  (21.34f)   // intercept

// ─── Grove water / rain sensor ────────────────────────────────────────────────
// GPIO 2 = ADC1_CH2. The sensor's analog output is read; rain is detected when
// the raw ADC value exceeds RAIN_THRESHOLD.
// NOTE: GPIO 2 is a strapping pin on the ESP32-C3 – see README §"GPIO Notes".
#define RAIN_SENSOR_PIN  2
#define RAIN_THRESHOLD   1500   // ADC counts (0–4095); tune for your environment

// ─── Submersible pump – relay control ─────────────────────────────────────────
// GPIO 1 drives the relay coil that switches the pump.
// Set PUMP_RELAY_ACTIVE_LEVEL to LOW  for active-LOW relay modules (most common).
// Set PUMP_RELAY_ACTIVE_LEVEL to HIGH for active-HIGH relay modules.
// If the pump turns ON at boot while the dashboard says OFF, this polarity is
// likely wrong for your relay board: switch LOW <-> HIGH, reflash, and retest.
// Always test relay behaviour without the pump attached first.
#define PUMP_RELAY_PIN           1
#define PUMP_RELAY_ACTIVE_LEVEL  HIGH

// Safety: the firmware automatically de-energises the relay after this many
// milliseconds even if no explicit OFF command is received.  This protects against
// the pump running indefinitely due to a network issue or firmware hang.
// 60 seconds is a conservative default suitable for small-volume pumping tasks
// (e.g. brief watering runs).  Increase this value for larger tanks or slower pumps,
// and always verify your pump's max continuous-run rating.
// Set to 0 to disable (not recommended for unattended operation).
#define PUMP_MAX_ON_MS  60000UL   // 60 seconds

// ─── Sampling ─────────────────────────────────────────────────────────────────
#define SAMPLE_INTERVAL_MS  10000UL   // 10 seconds

// ─── RAM ring buffer ──────────────────────────────────────────────────────────
// 120 readings × 10 s ≈ 20 minutes of recent history instantly available.
#define RAM_BUFFER_SIZE  120

// ─── Flash persistence ────────────────────────────────────────────────────────
// Flush to LittleFS every N readings to limit flash wear.
// At 10 s/sample + PERSIST_EVERY_N = 6  →  one write per minute.
#define PERSIST_EVERY_N  6
#define LOG_FILE_PATH    "/sensor_log.csv"
#define LOG_MAX_BYTES    (50 * 1024)   // rotate log after ~50 KB

// ─── HTTP server ──────────────────────────────────────────────────────────────
#define HTTP_PORT  80

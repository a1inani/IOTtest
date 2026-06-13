#pragma once

// ─── Wi-Fi Access Point ────────────────────────────────────────────────────────
// IMPORTANT: Change AP_PASSWORD before deploying. The default is intentionally
// non-trivial but you should set a unique password for any real-world use.
#define AP_SSID      "SensorLogger"
#define AP_PASSWORD  "SL-sensor-1!"  // must be ≥8 characters for WPA2; use "" for open

// ─── DHT11 sensor ─────────────────────────────────────────────────────────────
// XIAO ESP32-C6 pin mapping:  D2 = GPIO 4
#define DHT_PIN   4
#define DHT_TYPE  DHT11

// ─── Sampling ─────────────────────────────────────────────────────────────────
// DHT11 minimum reliable interval: 1 000 ms; 2 000 ms+ recommended.
// 10 000 ms is the default – good balance between freshness and flash wear.
#define SAMPLE_INTERVAL_MS  10000UL   // 10 seconds

// ─── RAM ring buffer ──────────────────────────────────────────────────────────
// Number of readings kept in memory (120 × 10 s ≈ 20 minutes of recent history)
#define RAM_BUFFER_SIZE  120

// ─── Flash persistence ────────────────────────────────────────────────────────
// Write to LittleFS every N valid readings to reduce flash wear.
// At 10 s/sample + PERSIST_EVERY_N = 6  →  one flash write per minute.
#define PERSIST_EVERY_N   6
#define LOG_FILE_PATH     "/sensor_log.csv"
#define LOG_MAX_BYTES     (50 * 1024)   // rotate log after ~50 KB

// ─── HTTP server ──────────────────────────────────────────────────────────────
#define HTTP_PORT  80

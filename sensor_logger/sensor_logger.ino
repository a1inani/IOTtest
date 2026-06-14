/*
 * sensor_logger.ino – Sensor Logger for ESP32-C3 Super Mini
 *
 * Reads data from:
 *   BME280         (I²C, SDA=GPIO 5, SCL=GPIO 6) – temperature, humidity, pressure, altitude
 *   SEN0193        (GPIO 0, ADC)                  – analog water/liquid level
 *   Soil pH sensor (GPIO 3, ADC)                  – estimated soil pH (requires calibration)
 *   Grove rain     (GPIO 2, ADC)                  – water/rain presence
 *   Pump relay     (GPIO 1, digital output)        – submersible pump control
 *
 * Recent readings are stored in a RAM ring buffer; older readings are persisted
 * to LittleFS (internal flash) as a rolling CSV log.
 *
 * A Wi-Fi access point is broadcast and a captive-portal-style web dashboard is
 * served at http://192.168.4.1/ – no router or internet connection required.
 *
 * Endpoints
 *   GET /               HTML dashboard (auto-refreshes every 5 s)
 *   GET /api/current    JSON – most recent reading + pump state
 *   GET /api/history    JSON – last 50 RAM-buffer readings
 *   GET /api/pump       JSON – current pump state
 *   GET /api/pump/on    Turn pump ON (relay energised); redirects to /
 *   GET /api/pump/off   Turn pump OFF (relay de-energised); redirects to /
 *   GET /api/log        CSV  – full persistent history (download)
 *   GET /api/log/clear  Erase persistent CSV log, then redirect to /
 *
 * See README.md for wiring, library dependencies, calibration, and build instructions.
 * All tuneable constants are in config.h.
 */

#include "config.h"
#include "SampleStore.h"
#include "WebUI.h"

void setup() {
  Serial.begin(115200);
  delay(1500);  // allow USB serial to enumerate
  Serial.println(F("\n=== Sensor Logger – ESP32-C3 Super Mini ==="));

  SampleStore::begin();
  WebUI::begin();

  // Take an initial reading right away so the dashboard is populated on first load
  SampleStore::takeSample();

  Serial.println(F("\nSetup complete."));
  Serial.printf("Connect to Wi-Fi  SSID: \"%s\"  password: \"%s\"\n",
                AP_SSID, AP_PASSWORD);
  Serial.println(F("Open your browser and go to: http://192.168.4.1/\n"));
}

void loop() {
  static unsigned long lastSample = 0;

  unsigned long now = millis();
  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    SampleStore::takeSample();
  }

  // Enforce pump safety timer (auto-off after PUMP_MAX_ON_MS)
  SampleStore::updatePumpTimer();

  WebUI::handle();
}

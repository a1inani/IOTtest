/*
 * sensor_logger.ino – Sensor Logger for XIAO ESP32-C6 + DHT11
 *
 * Reads temperature, humidity, and a derived heat-index from a DHT11 sensor.
 * Recent readings are stored in a RAM ring buffer; older readings are persisted
 * to LittleFS (internal flash) as a rolling CSV log.
 *
 * A Wi-Fi access point is broadcast and a captive-portal-style web dashboard is
 * served at http://192.168.4.1/ – no router or internet connection required.
 *
 * Endpoints
 *   GET /               HTML dashboard (auto-refreshes every 5 s)
 *   GET /api/current    JSON – most recent reading
 *   GET /api/history    JSON – last 50 RAM-buffer readings
 *   GET /api/log        CSV  – full persistent history (download)
 *   GET /api/log/clear  Erase persistent CSV log, then redirect to /
 *
 * See README.md for wiring, library dependencies, and build instructions.
 * All tuneable constants are in config.h.
 */

#include "config.h"
#include "SampleStore.h"
#include "WebUI.h"

void setup() {
  Serial.begin(115200);
  delay(1500);  // allow USB serial to enumerate
  Serial.println(F("\n=== Sensor Logger – XIAO ESP32-C6 ==="));

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

  WebUI::handle();
}

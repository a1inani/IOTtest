#include "SampleStore.h"
#include "config.h"

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <LittleFS.h>

// ─── module-private state ─────────────────────────────────────────────────────

static Adafruit_BME280 bme;
static bool _bmeAvailable = false;

static SensorReading _buf[RAM_BUFFER_SIZE];
static int  _head  = 0;
static int  _count = 0;
static int  _persistCounter = 0;

// Keep startup default explicitly in sync with relayOffLevel() logic.
static int           _pumpRelayLevel =
    (PUMP_RELAY_ACTIVE_LEVEL == LOW) ? HIGH : LOW;
static unsigned long _pumpOnTime = 0;

// ─── forward declaration ──────────────────────────────────────────────────────

static void persistReading(const SensorReading& r);

// ─── helpers ──────────────────────────────────────────────────────────────────

/** Relay OFF level is the logical inverse of the active (ON) level. */
static inline int relayOffLevel() {
  return (PUMP_RELAY_ACTIVE_LEVEL == LOW) ? HIGH : LOW;
}

static inline const char* levelName(int level) {
  return (level == HIGH) ? "HIGH" : "LOW";
}

// ─── public API ───────────────────────────────────────────────────────────────

void SampleStore::begin() {
  // ── Pump relay: drive to safe OFF state immediately ────────────────────────
  // Set the output latch first, then enable output mode to reduce startup glitches.
  digitalWrite(PUMP_RELAY_PIN, relayOffLevel());
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, relayOffLevel());
  _pumpRelayLevel = relayOffLevel();
  Serial.printf("[SampleStore] Pump relay init GPIO%d: active=%s, off=%s, pin=%s (safe OFF).\n",
                PUMP_RELAY_PIN,
                levelName(PUMP_RELAY_ACTIVE_LEVEL),
                levelName(relayOffLevel()),
                levelName(_pumpRelayLevel));

  // ── BME280 via I²C ─────────────────────────────────────────────────────────
  Wire.begin(BME280_SDA_PIN, BME280_SCL_PIN);
  if (!bme.begin(BME280_I2C_ADDR, &Wire)) {
    Serial.printf("[SampleStore] BME280 not found at address 0x%02X "
                  "(SDA=GPIO%d, SCL=GPIO%d).\n"
                  "              Check wiring. Try 0x77 if SDO is tied HIGH.\n",
                  BME280_I2C_ADDR, BME280_SDA_PIN, BME280_SCL_PIN);
    _bmeAvailable = false;
  } else {
    Serial.printf("[SampleStore] BME280 ready (SDA=GPIO%d, SCL=GPIO%d, addr=0x%02X).\n",
                  BME280_SDA_PIN, BME280_SCL_PIN, BME280_I2C_ADDR);
    _bmeAvailable = true;
  }

  // ── ADC – explicit 12-bit resolution (0–4095) ──────────────────────────────
  analogReadResolution(12);
  Serial.printf("[SampleStore] ADC 12-bit: water level GPIO%d, pH GPIO%d, rain GPIO%d.\n",
                WATER_LEVEL_PIN, SOIL_PH_PIN, RAIN_SENSOR_PIN);

  // ── LittleFS ───────────────────────────────────────────────────────────────
  if (!LittleFS.begin(true)) {   // 'true' = format on first use
    Serial.println("[SampleStore] LittleFS mount failed – readings will not persist.");
  } else {
    Serial.println("[SampleStore] LittleFS mounted.");
    if (LittleFS.exists(LOG_FILE_PATH)) {
      File f = LittleFS.open(LOG_FILE_PATH, "r");
      if (f) {
        Serial.printf("[SampleStore] Existing log: %u bytes.\n", (unsigned)f.size());
        f.close();
      }
    }
  }
}

void SampleStore::takeSample() {
  SensorReading r{};
  r.uptime_ms = millis();

  // ── BME280 ─────────────────────────────────────────────────────────────────
  if (_bmeAvailable) {
    r.temperature_c = bme.readTemperature();
    r.humidity      = bme.readHumidity();
    r.pressure_hpa  = bme.readPressure() / 100.0f;
    r.altitude_m    = bme.readAltitude(SEA_LEVEL_PRESSURE_HPA);
    r.temperature_f = r.temperature_c * 9.0f / 5.0f + 32.0f;

    if (isnan(r.temperature_c) || isnan(r.humidity) || isnan(r.pressure_hpa)) {
      Serial.println("[SampleStore] BME280 returned NaN – check sensor connection.");
      r.bme_valid = false;
    } else {
      r.bme_valid = true;
      Serial.printf("[SampleStore] BME: T=%.1f°C  H=%.1f%%  P=%.1fhPa  Alt=%.1fm\n",
                    r.temperature_c, r.humidity, r.pressure_hpa, r.altitude_m);
    }
  } else {
    r.bme_valid    = false;
    r.temperature_c = NAN;
    r.temperature_f = NAN;
    r.humidity      = NAN;
    r.pressure_hpa  = NAN;
    r.altitude_m    = NAN;
  }

  // ── SEN0193 water level ────────────────────────────────────────────────────
  r.water_level_raw = analogRead(WATER_LEVEL_PIN);
  r.water_level_pct = (int)((r.water_level_raw / 4095.0f) * 100.0f + 0.5f);
  Serial.printf("[SampleStore] WaterLevel: raw=%d  pct=%d%%\n",
                r.water_level_raw, r.water_level_pct);

  // ── Soil pH (estimated) ────────────────────────────────────────────────────
  r.ph_raw = analogRead(SOIL_PH_PIN);
  float phVoltage = r.ph_raw * 3.3f / 4095.0f;
  r.ph_value  = PH_SLOPE * phVoltage + PH_INTERCEPT;
  // Clamp to valid pH range using Arduino's constrain() helper
  r.ph_value  = constrain(r.ph_value, 0.0f, 14.0f);
  Serial.printf("[SampleStore] SoilPH: raw=%d  %.3fV  pH≈%.2f (estimated, uncalibrated)\n",
                r.ph_raw, phVoltage, r.ph_value);

  // ── Grove water/rain sensor ────────────────────────────────────────────────
  r.rain_raw      = analogRead(RAIN_SENSOR_PIN);
  r.rain_detected = (r.rain_raw > RAIN_THRESHOLD);
  Serial.printf("[SampleStore] Rain: raw=%d  detected=%s\n",
                r.rain_raw, r.rain_detected ? "YES" : "no");

  r.valid = true;

  // ── store in RAM ring buffer ────────────────────────────────────────────────
  _buf[_head] = r;
  _head = (_head + 1) % RAM_BUFFER_SIZE;
  if (_count < RAM_BUFFER_SIZE) _count++;

  // ── flush to LittleFS periodically ─────────────────────────────────────────
  if (++_persistCounter >= PERSIST_EVERY_N) {
    _persistCounter = 0;
    persistReading(r);
  }
}

void SampleStore::setPump(bool on) {
  int targetLevel = on ? PUMP_RELAY_ACTIVE_LEVEL : relayOffLevel();
  if (targetLevel == _pumpRelayLevel) {
    if (on) {
      _pumpOnTime = millis();
      Serial.printf("[SampleStore] Pump ON repeated: GPIO%d already %s, safety timer reset to %lus.\n",
                    PUMP_RELAY_PIN, levelName(_pumpRelayLevel), PUMP_MAX_ON_MS / 1000UL);
    } else {
      Serial.printf("[SampleStore] Pump command unchanged: requested OFF, pin already %s (GPIO%d).\n",
                    levelName(_pumpRelayLevel), PUMP_RELAY_PIN);
    }
    return;
  }
  bool wasOn = (_pumpRelayLevel == PUMP_RELAY_ACTIVE_LEVEL);

  digitalWrite(PUMP_RELAY_PIN, targetLevel);
  _pumpRelayLevel = targetLevel;

  if (on && !wasOn) {
    _pumpOnTime = millis();
  }

  Serial.printf("[SampleStore] Pump %s: wrote GPIO%d=%s (active=%s, off=%s, timer=%lus).\n",
                on ? "ON" : "OFF",
                PUMP_RELAY_PIN,
                levelName(_pumpRelayLevel),
                levelName(PUMP_RELAY_ACTIVE_LEVEL),
                levelName(relayOffLevel()),
                PUMP_MAX_ON_MS / 1000UL);
}

bool SampleStore::getPumpState() {
  return (_pumpRelayLevel == PUMP_RELAY_ACTIVE_LEVEL);
}

int SampleStore::getPumpRelayLevel() {
  return _pumpRelayLevel;
}

int SampleStore::getPumpRelayActiveLevel() {
  return PUMP_RELAY_ACTIVE_LEVEL;
}

int SampleStore::getPumpRelayOffLevel() {
  return relayOffLevel();
}

void SampleStore::updatePumpTimer() {
  // Unsigned subtraction wraps correctly on millis() rollover (~49 days),
  // so this comparison remains valid throughout continuous operation.
  if (PUMP_MAX_ON_MS > 0 && getPumpState() &&
      (millis() - _pumpOnTime >= PUMP_MAX_ON_MS)) {
    Serial.println("[SampleStore] Pump auto-off: safety timer expired.");
    setPump(false);
  }
}

SensorReading SampleStore::getLatest() {
  if (_count == 0) {
    SensorReading empty{};
    empty.valid = false;
    return empty;
  }
  return _buf[(_head - 1 + RAM_BUFFER_SIZE) % RAM_BUFFER_SIZE];
}

String SampleStore::historyJson(int maxEntries) {
  int n     = (_count < maxEntries) ? _count : maxEntries;
  int start = (_head - n + RAM_BUFFER_SIZE * 2) % RAM_BUFFER_SIZE;

  String json;
  json.reserve(512 + n * 220);
  json = F("{\"readings\":[");

  bool first = true;
  for (int i = 0; i < n; i++) {
    const SensorReading& r = _buf[(start + i) % RAM_BUFFER_SIZE];
    if (!r.valid) continue;
    if (!first) json += ',';
    first = false;

    // Format BME280 floats: output "null" for NaN to produce valid JSON.
    char tC[10], tF[10], hum[10], pHpa[10], alt[10];
    if (r.bme_valid) {
      snprintf(tC,   sizeof(tC),   "%.1f", r.temperature_c);
      snprintf(tF,   sizeof(tF),   "%.1f", r.temperature_f);
      snprintf(hum,  sizeof(hum),  "%.1f", r.humidity);
      snprintf(pHpa, sizeof(pHpa), "%.1f", r.pressure_hpa);
      snprintf(alt,  sizeof(alt),  "%.1f", r.altitude_m);
    } else {
      strcpy(tC, "null"); strcpy(tF, "null"); strcpy(hum, "null");
      strcpy(pHpa, "null"); strcpy(alt, "null");
    }

    char entry[256];
    snprintf(entry, sizeof(entry),
             "{\"uptime_ms\":%lu,\"bme_valid\":%s,"
             "\"temperature_c\":%s,\"temperature_f\":%s,"
             "\"humidity\":%s,\"pressure_hpa\":%s,\"altitude_m\":%s,"
             "\"water_level_raw\":%d,\"water_level_pct\":%d,"
             "\"ph_raw\":%d,\"ph_value\":%.2f,"
             "\"rain_raw\":%d,\"rain_detected\":%s}",
             r.uptime_ms,
             r.bme_valid ? "true" : "false",
             tC, tF, hum, pHpa, alt,
             r.water_level_raw, r.water_level_pct,
             r.ph_raw, r.ph_value,
             r.rain_raw, r.rain_detected ? "true" : "false");
    json += entry;
  }
  json += F("]}");
  return json;
}

String SampleStore::persistentLog() {
  if (!LittleFS.exists(LOG_FILE_PATH)) return String();
  File f = LittleFS.open(LOG_FILE_PATH, "r");
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

void SampleStore::clearLog() {
  if (LittleFS.exists(LOG_FILE_PATH)) {
    LittleFS.remove(LOG_FILE_PATH);
    Serial.println("[SampleStore] Persistent log cleared.");
  }
}

// ─── private ──────────────────────────────────────────────────────────────────

static void persistReading(const SensorReading& r) {
  // Rotate log if it has grown beyond the configured limit
  if (LittleFS.exists(LOG_FILE_PATH)) {
    File tmp = LittleFS.open(LOG_FILE_PATH, "r");
    if (tmp) {
      bool tooLarge = (tmp.size() > LOG_MAX_BYTES);
      tmp.close();
      if (tooLarge) {
        LittleFS.remove(LOG_FILE_PATH);
        Serial.println("[SampleStore] Log rotated (size limit reached).");
      }
    }
  }

  bool needHeader = !LittleFS.exists(LOG_FILE_PATH);
  File f = LittleFS.open(LOG_FILE_PATH, "a");
  if (!f) {
    Serial.println("[SampleStore] Failed to open log file for writing.");
    return;
  }

  if (needHeader) {
    // Column names: mark derived and estimated values explicitly
    f.println(F("uptime_ms,temperature_c,temperature_f,humidity,"
                "pressure_hpa,altitude_m(derived),"
                "water_level_raw,water_level_pct,"
                "ph_raw,ph_value(estimated),rain_raw,rain_detected"));
  }

  // Format NaN as empty field so the CSV remains parseable
  char tC[8], tF[8], hum[8], pHpa[8], alt[8];
  if (r.bme_valid) {
    snprintf(tC,   sizeof(tC),   "%.1f", r.temperature_c);
    snprintf(tF,   sizeof(tF),   "%.1f", r.temperature_f);
    snprintf(hum,  sizeof(hum),  "%.1f", r.humidity);
    snprintf(pHpa, sizeof(pHpa), "%.1f", r.pressure_hpa);
    snprintf(alt,  sizeof(alt),  "%.1f", r.altitude_m);
  } else {
    tC[0] = tF[0] = hum[0] = pHpa[0] = alt[0] = '\0';
  }

  char line[192];
  snprintf(line, sizeof(line),
           "%lu,%s,%s,%s,%s,%s,%d,%d,%d,%.2f,%d,%d",
           r.uptime_ms,
           tC, tF, hum, pHpa, alt,
           r.water_level_raw, r.water_level_pct,
           r.ph_raw, r.ph_value,
           r.rain_raw, (int)r.rain_detected);
  f.println(line);
  f.close();
}

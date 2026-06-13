#include "SampleStore.h"
#include "config.h"

#include <DHT.h>
#include <LittleFS.h>

// ─── module-private state ─────────────────────────────────────────────────────

static DHT dht(DHT_PIN, DHT_TYPE);

static SensorReading _buf[RAM_BUFFER_SIZE];
static int  _head  = 0;   // index of the NEXT write slot
static int  _count = 0;   // number of valid entries currently stored

static int  _persistCounter = 0;

// ─── forward declaration ──────────────────────────────────────────────────────

static void persistReading(const SensorReading& r);

// ─── public API ───────────────────────────────────────────────────────────────

void SampleStore::begin() {
  dht.begin();
  Serial.printf("[SampleStore] DHT sensor (type id=%d) initialised on GPIO %d\n",
               (int)DHT_TYPE, DHT_PIN);

  if (!LittleFS.begin(true)) {  // 'true' = format filesystem on first use
    Serial.println("[SampleStore] LittleFS mount failed – readings will not persist across reboots.");
  } else {
    Serial.println("[SampleStore] LittleFS mounted.");
    if (LittleFS.exists(LOG_FILE_PATH)) {
      File f = LittleFS.open(LOG_FILE_PATH, "r");
      if (f) {
        Serial.printf("[SampleStore] Existing log: %u bytes\n", (unsigned)f.size());
        f.close();
      }
    }
  }
}

void SampleStore::takeSample() {
  SensorReading r;
  r.uptime_ms     = millis();
  r.humidity      = dht.readHumidity();
  r.temperature_c = dht.readTemperature(false);  // Celsius
  r.temperature_f = dht.readTemperature(true);   // Fahrenheit

  if (isnan(r.humidity) || isnan(r.temperature_c)) {
    Serial.println("[SampleStore] DHT11 read failed (NaN) – check wiring and pull-up resistor.");
    r.valid       = false;
    r.heat_index_c = NAN;
    r.heat_index_f = NAN;
  } else {
    r.valid = true;
    // heat_index is a DERIVED value computed from temperature + humidity
    r.heat_index_c = dht.computeHeatIndex(r.temperature_c, r.humidity, false);
    r.heat_index_f = dht.computeHeatIndex(r.temperature_f, r.humidity, true);

    Serial.printf("[SampleStore] T=%.1f°C / %.1f°F  H=%.0f%%  HI=%.1f°C (derived)\n",
                  r.temperature_c, r.temperature_f, r.humidity, r.heat_index_c);
  }

  // ── store in RAM ring buffer ────────────────────────────────────────────────
  _buf[_head] = r;
  _head = (_head + 1) % RAM_BUFFER_SIZE;
  if (_count < RAM_BUFFER_SIZE) _count++;

  // ── flush to LittleFS periodically ─────────────────────────────────────────
  if (r.valid && (++_persistCounter >= PERSIST_EVERY_N)) {
    _persistCounter = 0;
    persistReading(r);
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
  json.reserve(128 + n * 110);  // pre-allocate to reduce heap fragmentation
  json = F("{\"readings\":[");

  bool first = true;
  for (int i = 0; i < n; i++) {
    const SensorReading& r = _buf[(start + i) % RAM_BUFFER_SIZE];
    if (!r.valid) continue;
    if (!first) json += ',';
    first = false;

    char entry[128];
    snprintf(entry, sizeof(entry),
             "{\"uptime_ms\":%lu,\"temperature_c\":%.1f,\"temperature_f\":%.1f,"
             "\"humidity\":%.1f,\"heat_index_c\":%.1f,\"heat_index_f\":%.1f}",
             r.uptime_ms,
             r.temperature_c, r.temperature_f,
             r.humidity,
             r.heat_index_c, r.heat_index_f);
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
    f.println(F("uptime_ms,temperature_c,temperature_f,humidity,"
                "heat_index_c(derived),heat_index_f(derived)"));
  }

  char line[96];
  snprintf(line, sizeof(line), "%lu,%.1f,%.1f,%.1f,%.1f,%.1f",
           r.uptime_ms,
           r.temperature_c, r.temperature_f,
           r.humidity,
           r.heat_index_c, r.heat_index_f);
  f.println(line);
  f.close();
}

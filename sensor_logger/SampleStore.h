#pragma once

#include <Arduino.h>

/**
 * SensorReading – one timestamped snapshot from the DHT11.
 *
 * temperature_c / temperature_f  – measured directly by the sensor.
 * humidity                       – measured directly by the sensor.
 * heat_index_c / heat_index_f    – DERIVED: computed from temperature + humidity
 *                                  using the Steadman formula (Adafruit DHT library).
 */
struct SensorReading {
  unsigned long uptime_ms;     ///< Device uptime at time of reading (ms since boot)
  float temperature_c;         ///< Temperature in Celsius         (DHT11 direct)
  float temperature_f;         ///< Temperature in Fahrenheit      (DHT11 direct)
  float humidity;              ///< Relative humidity in %         (DHT11 direct)
  float heat_index_c;          ///< Heat index in Celsius          – DERIVED value
  float heat_index_f;          ///< Heat index in Fahrenheit       – DERIVED value
  bool  valid;                 ///< false when the sensor returned an error
};

namespace SampleStore {
  /** Initialise DHT11 and mount LittleFS (formats on first use). */
  void begin();

  /** Read DHT11, push into RAM ring buffer, and flush to flash periodically. */
  void takeSample();

  /** Return the most recent reading (valid = false if no readings yet). */
  SensorReading getLatest();

  /**
   * Serialise up to \p maxEntries recent ring-buffer readings as a JSON string.
   * Format: {"readings":[…]}  oldest → newest order.
   */
  String historyJson(int maxEntries = 50);

  /** Return the full persisted CSV from LittleFS (empty string if none). */
  String persistentLog();

  /** Delete the persistent CSV log from LittleFS. */
  void clearLog();
}

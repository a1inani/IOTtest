#pragma once

#include <Arduino.h>

/**
 * SensorReading – one timestamped snapshot from all sensors attached to the
 * ESP32-C3 Super Mini.
 *
 * BME280  (I²C, GPIO 5/6):
 *   temperature_c / temperature_f – directly measured
 *   humidity                      – directly measured
 *   pressure_hpa                  – directly measured
 *   altitude_m                    – DERIVED from pressure + SEA_LEVEL_PRESSURE_HPA
 *
 * SEN0193 (GPIO 0, ADC1_CH0):
 *   water_level_raw  – raw ADC value (0–4095)
 *   water_level_pct  – mapped to 0–100 %
 *
 * Soil pH sensor (GPIO 3, ADC1_CH3):
 *   ph_raw    – raw ADC value (0–4095)
 *   ph_value  – ESTIMATED pH using linear calibration (PH_SLOPE / PH_INTERCEPT).
 *               Values are approximate until the sensor is calibrated with
 *               pH 4.0 and pH 7.0 buffer solutions – see README.md.
 *
 * Grove water/rain sensor (GPIO 2, ADC1_CH2):
 *   rain_raw      – raw ADC value (0–4095)
 *   rain_detected – true when rain_raw > RAIN_THRESHOLD
 *
 * Pump relay (GPIO 1):
 *   Controlled separately via SampleStore::setPump(); not stored in the reading.
 */
struct SensorReading {
  unsigned long uptime_ms;

  // ── BME280 ──────────────────────────────────────────────────────────────────
  float temperature_c;   ///< Temperature in °C
  float temperature_f;   ///< Temperature in °F
  float humidity;        ///< Relative humidity in %
  float pressure_hpa;    ///< Barometric pressure in hPa
  float altitude_m;      ///< Altitude in m – DERIVED (requires SEA_LEVEL_PRESSURE_HPA)

  // ── SEN0193 water level ──────────────────────────────────────────────────────
  int water_level_raw;   ///< Raw ADC value (0–4095)
  int water_level_pct;   ///< Approx. fill level 0–100 %

  // ── Soil pH sensor ───────────────────────────────────────────────────────────
  int   ph_raw;    ///< Raw ADC value (0–4095)
  float ph_value;  ///< Estimated pH (0–14); requires calibration for accuracy

  // ── Grove water/rain sensor ──────────────────────────────────────────────────
  int  rain_raw;       ///< Raw ADC value (0–4095)
  bool rain_detected;  ///< true when rain_raw > RAIN_THRESHOLD

  // ── Meta ─────────────────────────────────────────────────────────────────────
  bool bme_valid;  ///< false when BME280 returned NaN
  bool valid;      ///< false only if no reading has been taken yet
};

namespace SampleStore {
  /**
   * Initialise all sensors and peripherals, and mount LittleFS.
   * The relay is driven to the safe OFF state immediately on entry.
   * Must be called once from setup().
   */
  void begin();

  /** Read all sensors, push into RAM ring buffer, flush to flash periodically. */
  void takeSample();

  // ── Pump control ─────────────────────────────────────────────────────────────
  /**
   * Energise (on=true) or de-energise (on=false) the pump relay.
   * Turning the pump ON starts the PUMP_MAX_ON_MS safety timer; the pump is
   * automatically switched off when the timer expires even without an API call.
   */
  void setPump(bool on);

  /** Return the current commanded pump state (true = relay energised). */
  bool getPumpState();

  /** Return the GPIO level most recently written to the pump relay pin. */
  int getPumpRelayLevel();

  /** Return the configured relay active (energised) GPIO level. */
  int getPumpRelayActiveLevel();

  /** Return the configured relay OFF (de-energised) GPIO level. */
  int getPumpRelayOffLevel();

  /**
   * Must be called from loop().
   * Enforces the pump auto-off safety timer defined by PUMP_MAX_ON_MS.
   */
  void updatePumpTimer();

  // ── Data access ──────────────────────────────────────────────────────────────
  /** Return the most recent reading (valid = false if no reading taken yet). */
  SensorReading getLatest();

  /**
   * Serialise up to maxEntries recent ring-buffer readings as a JSON string.
   * Format: {"readings":[…]} oldest → newest.
   */
  String historyJson(int maxEntries = 50);

  /** Return the full persisted CSV from LittleFS (empty string if none). */
  String persistentLog();

  /** Delete the persistent CSV log from LittleFS. */
  void clearLog();
}

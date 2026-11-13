// One opt-in daily radio alarm. Pure C++11 calendar/fade policy; no ESP32 I/O.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace pocketjazz {

struct DailyRadioAlarmConfig {
  bool enabled = false;
  uint8_t hour = 7;
  uint8_t minute = 0;
  uint8_t preset = 0; // 0..8 map to the existing saved 1..9 presets
};

class DailyRadioAlarm {
 public:
  static constexpr size_t kConfigBytes = 8;
  static constexpr uint32_t kFadeMs = 60000;
  static constexpr uint32_t kStartTimeoutMs = 20000;

  static bool valid(const DailyRadioAlarmConfig& config) {
    return config.hour < 24 && config.minute < 60 && config.preset < 9;
  }

  static void encode(const DailyRadioAlarmConfig& config, uint8_t (&out)[kConfigBytes]) {
    out[0] = 'J'; out[1] = 'A'; out[2] = 1;
    out[3] = config.enabled ? 1 : 0;
    out[4] = config.hour;
    out[5] = config.minute;
    out[6] = config.preset;
    out[7] = checksum(out);
  }

  static bool decode(const uint8_t* bytes, size_t length, DailyRadioAlarmConfig& config) {
    if (!bytes || length != kConfigBytes || bytes[0] != 'J' ||
        bytes[1] != 'A' || bytes[2] != 1 || bytes[3] > 1 ||
        bytes[7] != checksum(bytes)) return false;
    DailyRadioAlarmConfig decoded{};
    decoded.enabled = bytes[3] != 0;
    decoded.hour = bytes[4];
    decoded.minute = bytes[5];
    decoded.preset = bytes[6];
    if (!valid(decoded)) return false;
    config = decoded;
    return true;
  }

  // An ordinal local-day identifier, not UTC seconds. A repeated DST hour
  // belongs to the same day; a missing spring-forward hour never matches.
  static int32_t civilDayKey(int year, int dayOfYear) {
    if (year < 2024 || year > 2100 || dayOfYear < 0 || dayOfYear > 365)
      return -1;
    return year * 1000 + dayOfYear;
  }

  static bool validDayKey(int32_t key) {
    return key >= 2024000 &&
           civilDayKey(key / 1000, key % 1000) == key;
  }

  static bool due(const DailyRadioAlarmConfig& config, bool ntpConfirmed,
                  int year, int dayOfYear, int hour, int minute, int second,
                  int32_t lastFiredDay) {
    if (!ntpConfirmed || !config.enabled || !valid(config) ||
        hour < 0 || hour >= 24 || minute < 0 || minute >= 60 ||
        second < 0 || second >= 60) return false;
    const int32_t today = civilDayKey(year, dayOfYear);
    // Also reject a clock that jumped to a day BEFORE a previously fired day.
    return today >= 0 && today > lastFiredDay &&
           hour == config.hour && minute == config.minute;
  }

  static uint8_t safeTarget(int normalVolume) {
    // M5 Speaker's 0..255 range: alarms deliberately remain on a low cap.
    return static_cast<uint8_t>(normalVolume < 24 ? 24 :
                                normalVolume > 80 ? 80 : normalVolume);
  }

  static uint8_t fadeVolume(uint32_t elapsedMs, uint8_t target) {
    if (target == 0) return 0;
    if (elapsedMs >= kFadeMs) return target;
    // Start silent; integer arithmetic avoids floating point/overflow.
    return static_cast<uint8_t>((static_cast<uint32_t>(target) * elapsedMs) / kFadeMs);
  }

 private:
  static uint8_t checksum(const uint8_t* bytes) {
    uint8_t sum = 0xa7;
    for (size_t i = 0; i < kConfigBytes - 1; ++i)
      sum = static_cast<uint8_t>((sum << 1) | (sum >> 7)) ^ bytes[i];
    return sum;
  }
};

} // namespace pocketjazz

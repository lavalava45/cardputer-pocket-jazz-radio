// SPDX-License-Identifier: MIT
// Defensive POSIX-TZ string validation for the optional SD clock config.
// It is a character/size guard, not an authoritative parser for every TZ rule.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>

namespace pocketjazz {
// Convert localtime_r and gmtime_r output to the effective UTC offset.
// Works even across Dec 31/Jan 1 and Feb 29; tm_gmtoff is not exposed by
// our pinned Arduino-ESP32 newlib headers. Pure function, host-testable.
inline int effectiveOffsetFromCalendars(const tm& local, const tm& utc) {
  int minutes = (local.tm_hour - utc.tm_hour) * 60 +
                local.tm_min - utc.tm_min;
  if (local.tm_year > utc.tm_year ||
      (local.tm_year == utc.tm_year && local.tm_yday > utc.tm_yday))
    minutes += 1440;
  else if (local.tm_year < utc.tm_year ||
           (local.tm_year == utc.tm_year && local.tm_yday < utc.tm_yday))
    minutes -= 1440;
  return minutes;
}
// Human UTC offset (east positive) is the INVERSE of POSIX TZ sign.
// Supports India +05:30, Newfoundland -03:30 etc. DST needs SD POSIX rule.
inline bool validUtcOffsetMinutes(int offset) {
  return offset >= -720 && offset <= 840 && offset % 30 == 0;
}
inline bool fixedUtcOffsetRule(int minutes, char* target, size_t size) {
  if (!target || size < 15 || !validUtcOffsetMinutes(minutes)) return false;
  const int absMinutes = minutes < 0 ? -minutes : minutes;
  const int hours = absMinutes / 60, rest = absMinutes % 60;
  const int written = rest
      ? snprintf(target, size, "UTC%c%d:%02d", minutes > 0 ? '-' : '+', hours, rest)
      : minutes
          ? snprintf(target, size, "UTC%c%d", minutes > 0 ? '-' : '+', hours)
          : snprintf(target, size, "UTC0");
  return written > 0 && static_cast<size_t>(written) < size;
}
inline bool validPosixTimeZone(const char* value) {
  if (!value) return false;
  size_t count = 0;
  const auto alpha = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
  };
  while (value[count]) {
    if (count >= 79) return false;
    const char c = value[count];
    const bool letter = alpha(c);
    const bool digit = c >= '0' && c <= '9';
    if (!letter && !digit && c != ',' && c != '.' &&
        c != '/' && c != '+' && c != '-' && c != ':') return false;
    ++count;
  }
  // This file is NOT a filesystem path, and must start with a POSIX-style
  // standard-time abbreviation followed by an explicit numeric UTC offset.
  if (count < 4) return false;
  size_t pos = 0;
  while (pos < count && alpha(value[pos])) ++pos;
  if (pos < 3) return false;
  if (pos < count && (value[pos] == '+' || value[pos] == '-')) ++pos;
  if (pos >= count || value[pos] < '0' || value[pos] > '9') return false;
  while (pos < count && value[pos] >= '0' && value[pos] <= '9') ++pos;
  // Optional HH:MM or HH:MM:SS in the standard offset.
  for (int segment = 0; segment < 2 && pos < count && value[pos] == ':'; ++segment) {
    ++pos;
    const size_t begin = pos;
    while (pos < count && value[pos] >= '0' && value[pos] <= '9') ++pos;
    if (pos - begin != 2) return false;
  }
  if (pos == count) return true; // e.g. UTC0
  // Optional DST abbreviation; a rule section starts only after its comma.
  const size_t dstStart = pos;
  while (pos < count && alpha(value[pos])) ++pos;
  if (pos - dstStart < 3) return false;
  // Optional DST numeric offset (not used by the bundled examples).
  if (pos < count && (value[pos] == '+' || value[pos] == '-')) {
    ++pos;
    if (pos >= count || value[pos] < '0' || value[pos] > '9') return false;
  }
  while (pos < count && value[pos] >= '0' && value[pos] <= '9') ++pos;
  if (pos == count) return true;
  if (value[pos++] != ',') return false;
  // Require two explicit nonempty DST transition rules separated by comma.
  int commas = 1;
  bool segmentHasDigit = false;
  for (; pos < count; ++pos) {
    const char c = value[pos];
    if (c == ',') {
      if (commas++ != 1 || !segmentHasDigit) return false;
      segmentHasDigit = false;
    } else if (c >= '0' && c <= '9') {
      segmentHasDigit = true;
    } else if (c != 'M' && c != 'J' && c != '.' && c != '/' &&
               c != '+' && c != '-' && c != ':') {
      return false;
    }
  }
  if (commas != 2 || !segmentHasDigit) return false;
  // Reject a dangling delimiter, empty offsets, and basic malformed rules.
  const char last = value[count - 1];
  return last != ',' && last != '.' && last != ':' &&
         last != '/' && last != '+' && last != '-';
}
} // namespace pocketjazz

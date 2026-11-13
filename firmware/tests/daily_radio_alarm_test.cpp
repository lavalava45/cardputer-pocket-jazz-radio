#ifdef NDEBUG
#undef NDEBUG // Assertions must run in MSVC Release.
#endif
#include <cassert>
#include "../PocketJazzRadio/DailyRadioAlarm.h"

int main() {
  using pocketjazz::DailyRadioAlarm;
  pocketjazz::DailyRadioAlarmConfig c{};
  assert(!c.enabled && c.hour == 7 && c.minute == 0 && c.preset == 0);
  assert(!DailyRadioAlarm::due(c, true, 2026, 314, 7, 0, 0, -1));
  c.enabled = true;
  c.hour = 2; c.minute = 30; c.preset = 8;
  uint8_t wire[DailyRadioAlarm::kConfigBytes]{};
  DailyRadioAlarm::encode(c, wire);
  pocketjazz::DailyRadioAlarmConfig d{};
  assert(DailyRadioAlarm::decode(wire, sizeof(wire), d));
  assert(d.enabled && d.hour == 2 && d.minute == 30 && d.preset == 8);
  wire[4] = 25;
  assert(!DailyRadioAlarm::decode(wire, sizeof(wire), d));
  DailyRadioAlarm::encode(c, wire);
  wire[7] ^= 0x40;
  assert(!DailyRadioAlarm::decode(wire, sizeof(wire), d));
  DailyRadioAlarm::encode(c, wire);
  assert(!DailyRadioAlarm::decode(wire, sizeof(wire) - 1, d));
  assert(!DailyRadioAlarm::decode(nullptr, sizeof(wire), d));
  assert(!DailyRadioAlarm::due(c, false, 2026, 305, 2, 30, 2, -1));
  assert(!DailyRadioAlarm::due(c, true, 2026, 305, 2, 29, 59, -1));
  assert(DailyRadioAlarm::due(c, true, 2026, 305, 2, 30, 35, -1));
  const int32_t today = DailyRadioAlarm::civilDayKey(2026, 305);
  assert(!DailyRadioAlarm::due(c, true, 2026, 305, 2, 30, 59, today));
  assert(!DailyRadioAlarm::due(c, true, 2026, 304, 2, 30, 0, today));
  assert(DailyRadioAlarm::due(c, true, 2026, 306, 2, 30, 0, today));
  assert(DailyRadioAlarm::civilDayKey(2026, 366) == -1);
  assert(!DailyRadioAlarm::validDayKey(-1));
  assert(!DailyRadioAlarm::validDayKey(0));
  assert(!DailyRadioAlarm::validDayKey(2026366));
  assert(DailyRadioAlarm::validDayKey(today));
  assert(DailyRadioAlarm::safeTarget(0) == 24);
  assert(DailyRadioAlarm::safeTarget(57) == 57);
  assert(DailyRadioAlarm::safeTarget(185) == 80);
  assert(DailyRadioAlarm::fadeVolume(0, 80) == 0);
  assert(DailyRadioAlarm::fadeVolume(15000, 80) == 20);
  assert(DailyRadioAlarm::fadeVolume(30000, 80) == 40);
  assert(DailyRadioAlarm::fadeVolume(59999, 80) <= 80);
  assert(DailyRadioAlarm::fadeVolume(60000, 80) == 80);
  assert(DailyRadioAlarm::fadeVolume(UINT32_MAX, 80) == 80);
  return 0;
}

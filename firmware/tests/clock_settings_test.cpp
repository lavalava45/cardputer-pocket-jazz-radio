#ifdef NDEBUG
#undef NDEBUG // Keep release-build regression checks active.
#endif
#include "../PocketJazzRadio/ClockSettings.h"
#include <cassert>
#include <cstring>
static tm fields(int year, int yday, int hour, int minute) {
  tm result{};
  result.tm_year = year - 1900;
  result.tm_yday = yday;
  result.tm_hour = hour;
  result.tm_min = minute;
  return result;
}
int main() {
  using pocketjazz::validPosixTimeZone;
  char generated[30] = {};
  assert(pocketjazz::fixedUtcOffsetRule(0,generated,sizeof(generated)) && !strcmp(generated,"UTC0"));
  assert(pocketjazz::fixedUtcOffsetRule(120,generated,sizeof(generated)) && !strcmp(generated,"UTC-2"));
  assert(pocketjazz::fixedUtcOffsetRule(330,generated,sizeof(generated)) && !strcmp(generated,"UTC-5:30"));
  assert(pocketjazz::fixedUtcOffsetRule(-210,generated,sizeof(generated)) && !strcmp(generated,"UTC+3:30"));
  assert(pocketjazz::fixedUtcOffsetRule(840,generated,sizeof(generated)) && !strcmp(generated,"UTC-14"));
  assert(!pocketjazz::fixedUtcOffsetRule(900,generated,sizeof(generated)));
  assert(!pocketjazz::fixedUtcOffsetRule(75,generated,sizeof(generated)));
  assert(validPosixTimeZone("UTC0"));
  assert(validPosixTimeZone("EST5EDT,M3.2.0/2,M11.1.0/2"));
  assert(validPosixTimeZone("CET-1CEST,M3.5.0/2,M10.5.0/3"));
  assert(validPosixTimeZone("IST-2IDT,M3.4.4/26,M10.5.0"));
  assert(validPosixTimeZone("UTC0DST+1"));
  assert(validPosixTimeZone("UTC0DST-1"));
  assert(!validPosixTimeZone("UTC0DST+"));
  assert(!validPosixTimeZone("UTC0DST-"));
  assert(!validPosixTimeZone(nullptr));
  assert(!validPosixTimeZone(""));
  assert(!validPosixTimeZone("UTC"));
  assert(!validPosixTimeZone("UTC0\nWiFi=secret"));
  assert(!validPosixTimeZone("UTC0 	"));
  assert(!validPosixTimeZone("../../etc/secret"));
  assert(!validPosixTimeZone("UTC0;setenv"));
  assert(!validPosixTimeZone("UTC0,"));
  char tooLong[90]{};
  std::memset(tooLong, 'A', 89);
  assert(!validPosixTimeZone(tooLong));
  using pocketjazz::effectiveOffsetFromCalendars;
  // Actual calendar arithmetic used by runtime localtime_r/UTC comparison.
  assert(effectiveOffsetFromCalendars(fields(2026, 313, 19, 0),
                                      fields(2026, 313, 17, 0)) == 120);
  assert(effectiveOffsetFromCalendars(fields(2026, 190, 0, 15),
                                      fields(2026, 189, 21, 15)) == 180);
  // Extreme supported offsets crossing midnight and New Year.
  assert(effectiveOffsetFromCalendars(fields(2027, 0, 13, 0),
                                      fields(2026, 364, 23, 0)) == 840);
  assert(effectiveOffsetFromCalendars(fields(2026, 364, 12, 0),
                                      fields(2027, 0, 0, 0)) == -720);
  // Half-hour time zones, including the leap day transition.
  assert(effectiveOffsetFromCalendars(fields(2024, 60, 2, 0),
                                      fields(2024, 59, 20, 30)) == 330);
  assert(effectiveOffsetFromCalendars(fields(2024, 58, 20, 30),
                                      fields(2024, 59, 0, 0)) == -210);
  assert(effectiveOffsetFromCalendars(fields(2024, 0, 0, 0),
                                      fields(2024, 0, 0, 0)) == 0);
  return 0;
}

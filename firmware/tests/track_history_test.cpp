#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/TrackHistory.h"
#include <cassert>
#include <cstring>

int main() {
  pocketjazz::TrackHistory<3> tracks;
  assert(tracks.size() == 0 && tracks.newest() == nullptr);
  assert(!tracks.add(nullptr, "Track", 100));
  assert(!tracks.add("Jazz", "", 100));
  assert(tracks.add("Radio Jazz", "Miles Davis - So What", 1000));
  assert(!tracks.add("Radio Jazz", "Miles Davis - So What", 2000));
  assert(tracks.size() == 1 && tracks.newest()->observedAtMs == 1000);
  assert(tracks.add("Radio Jazz", "John Coltrane - Naima", 3000));
  assert(tracks.add("Radio B", "Miles Davis - So What", 4000));
  assert(tracks.size() == 3);
  assert(tracks.newest(0) && std::strcmp(tracks.newest(0)->station, "Radio B") == 0);
  assert(tracks.newest(2) && std::strcmp(tracks.newest(2)->title, "Miles Davis - So What") == 0);
  assert(tracks.add("Radio Jazz", "Bill Evans", 5000)); // overwrite oldest
  assert(tracks.size() == 3 && tracks.newest(3) == nullptr);
  assert(std::strcmp(tracks.newest(2)->title, "John Coltrane - Naima") == 0);
  assert(tracks.add("Radio Jazz", "New\nTrack\tTitle", 6000));
  assert(std::strcmp(tracks.newest()->title, "NewTrackTitle") == 0);
  char giant[300];
  std::memset(giant, 'A', sizeof(giant));
  giant[sizeof(giant) - 1] = '\0';
  assert(tracks.add("Radio Jazz", giant, 7000));
  assert(std::strlen(tracks.newest()->title) == 95);
  const uint32_t rev = tracks.revision();
  tracks.clear();
  assert(tracks.size() == 0 && tracks.newest() == nullptr);
  assert(tracks.revision() == rev + 1);
  return 0;
}

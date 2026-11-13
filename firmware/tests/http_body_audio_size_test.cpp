#ifdef NDEBUG
#undef NDEBUG // Keep negative/EOF assertions active under MSVC Release.
#endif
#include "../PocketJazzRadio/HttpBodyAudioSize.h"
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveStallPolicy.h"
#include <cassert>
#include <cstdint>

int main() {
  using pocketjazz::projectedMp3Size;
  AudioLiveStallPolicy stall;
  stall.setTimeoutMs(4000);
  // Finite HTTP entity body: 4 MP3 bytes + 1 ICY length + 16 metadata +
  // another 4 MP3 bytes. TCP may remain connected after final payload byte.
  const int32_t contentLength = 25;
  uint32_t audio = 4;
  uint32_t body = 4;
  assert(projectedMp3Size(contentLength, body, audio, 0) == 25);
  assert(stall.shouldWait(1000, true,
                          projectedMp3Size(contentLength, body, audio, 0), audio));
  ++body; // ICY length byte = 1 (16 bytes of metadata), not MP3
  body += 7; // fragmented metadata
  assert(projectedMp3Size(contentLength, body, audio, 0) == 17);
  assert(stall.shouldWait(1200, true,
                          projectedMp3Size(contentLength, body, audio, 0), audio));
  body += 9; // remaining metadata, still no MP3
  assert(projectedMp3Size(contentLength, body, audio, 0) == 8);
  stall.progress();
  audio += 4;
  body += 4; // whole finite response consumed; HTTP TCP still connected
  assert(body == static_cast<uint32_t>(contentLength));
  assert(projectedMp3Size(contentLength, body, audio, 0) == 8);
  assert(!stall.shouldWait(1400, true,
                           projectedMp3Size(contentLength, body, audio, 0), audio));
  assert(!stall.waiting()); // no spurious 4-second final silence
  // No metadata: known Content-Length behaves like normal MP3 file position.
  assert(projectedMp3Size(1000, 250, 250, 0) == 1000);
  // Unknown HTTP entity length stays live until timeout/disconnect.
  assert(projectedMp3Size(-1, 40, 20, 0) == 0);
  assert(projectedMp3Size(-1, 40, 20, UINT32_MAX) == UINT32_MAX);
  // Malformed overconsumption is saturated; cannot underflow projected limit.
  assert(projectedMp3Size(25, 26, 8, 0) == 8);
  return 0;
}

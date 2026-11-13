#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioMp3SyncBoundary.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
    // Real helper invoked by AudioGeneratorMP3::Input, not a duplicate mock.
    uint8_t a[] = {0x00, 0x42, 0xFF};
    assert(audio_mp3::findSyncHeader(a, 3) == -1);
    assert(audio_mp3::carryPartialSync(a, 3) == 1);
    assert(a[0] == 0xFF);
    a[1] = 0xFB;  // arrives in the next network read
    a[2] = 0x90;
    assert(audio_mp3::findSyncHeader(a, 3) == 0);

    // 0xFF at the very end of a fully occupied decoder buffer must not
    // trigger a one-past-end read, and it must be kept for the next poll.
    uint8_t full[1536] = {};
    full[1535] = 0xFF;
    assert(audio_mp3::findSyncHeader(full, 1536) == -1);
    assert(audio_mp3::carryPartialSync(full, 1536) == 1);
    assert(full[0] == 0xFF);
    full[1] = 0xF3;
    assert(audio_mp3::findSyncHeader(full, 2) == 0);

    uint8_t junk[] = {0xFF, 0x00, 0x90};
    assert(audio_mp3::findSyncHeader(junk, 3) == -1);
    assert(audio_mp3::carryPartialSync(junk, 3) == 0);
    assert(audio_mp3::findSyncHeader(junk, 1) == -1);
    assert(audio_mp3::findSyncHeader(nullptr, 20) == -1);
    assert(audio_mp3::carryPartialSync(nullptr, 20) == 0);
    return 0;
}

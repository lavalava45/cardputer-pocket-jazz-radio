#ifdef NDEBUG
#undef NDEBUG // This gate executes assertions even in MSVC Release.
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveDecodeBudget.h"
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioMp3SyncBoundary.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
    AudioLiveDecodeBudget budget;
    budget.begin(100);
    // Upstream finite-file clients opt out: no behaviour change by default.
    for (int i = 0; i < 10000; ++i) assert(budget.tryStep(100 + i));

    budget.setEnabled(true);
    budget.begin(1000);
    for (int i = 0; i < 8; ++i) assert(budget.tryStep(1000));
    assert(!budget.tryStep(1000)); // no ninth read/decode in one iteration
    budget.begin(2000);
    assert(budget.tryStep(2007));
    assert(!budget.tryStep(2008)); // time budget is independent of read count

    // The subtraction is rollover safe, as with the independent frame timeout.
    budget.begin(UINT32_MAX - 3);
    assert(budget.tryStep(UINT32_MAX));
    assert(budget.tryStep(2));
    assert(!budget.tryStep(4));
    budget.begin(8);
    assert(budget.tryStep(8)); // next audio task tick can progress

    // Simulate a stream that always returns positive junk. Each iteration
    // must yield without an endless sync search, never falsely report EOF.
    uint8_t packet[1536] = {};
    unsigned decoded = 0, yields = 0, reads = 0;
    for (unsigned call = 0; call < 100; ++call) {
        budget.begin(call * 10);
        unsigned perCall = 0;
        while (budget.tryStep(call * 10)) {
            memset(packet, 0x11, sizeof(packet));
            ++perCall;
            ++reads;
            assert(audio_mp3::findSyncHeader(packet, sizeof(packet)) == -1);
            decoded += audio_mp3::carryPartialSync(packet, sizeof(packet));
        }
        assert(perCall == 8);
        ++yields;
        // STOP/next station can be serviced between the eight bounded pulls.
    }
    assert(reads == 800 && yields == 100 && decoded == 0);

    // A header that straddles a yield is retained, not duplicated or dropped.
    uint8_t split[] = {0x20, 0x44, 0xFF};
    assert(audio_mp3::findSyncHeader(split, 3) == -1);
    const int pendingSyncCarry = audio_mp3::carryPartialSync(split, 3);
    assert(pendingSyncCarry == 1 && split[0] == 0xFF);
    budget.begin(5000);
    assert(budget.tryStep(5000));
    split[pendingSyncCarry] = 0xFB;
    assert(audio_mp3::findSyncHeader(split, pendingSyncCarry + 1) == 0);
    return 0;
}

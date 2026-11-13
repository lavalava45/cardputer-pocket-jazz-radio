/*
 * Optional cooperative audio-source pull, Pocket Jazz local extension.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Upstream behavior remains the default.  Host tests use a mock source.
 */
#pragma once
#include <stdint.h>

namespace audio_buffer {
template <typename Source>
inline uint32_t pull(Source* src, void* destination, uint32_t wanted,
                     bool cooperative) {
    return cooperative ? src->readNonBlock(destination, wanted)
                       : src->read(destination, wanted);
}
inline bool actualShortage(uint32_t wanted, uint32_t got) {
    return got < wanted;
}
// After a direct read, Buffer resets filled=false and calls fill(). A
// successful opportunistic refill MUST be retained on the next read; the old
// code discarded it by performing another initial src->read(buffer,...).
// Restrict this change to opt-in live clients; upstream file semantics stay.
inline bool keepOpportunisticBytes(bool cooperative, bool filled,
                                   uint32_t newlyReceived) {
    return filled || (cooperative && newlyReceived != 0);
}
} // namespace audio_buffer

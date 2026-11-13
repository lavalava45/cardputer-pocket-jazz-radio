/*
 * Tiny rollover-safe deadline for vendored plain-HTTP ICY metadata reads.
 * Bounded by the same 2500-ms limit as Pocket Jazz SecureICYSource.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

namespace audio_icy {
inline bool metadataTimedOut(uint32_t startedAt, uint32_t now,
                             uint32_t limitMs = 2500) {
    return (uint32_t)(now - startedAt) >= limitMs;
}
} // namespace audio_icy

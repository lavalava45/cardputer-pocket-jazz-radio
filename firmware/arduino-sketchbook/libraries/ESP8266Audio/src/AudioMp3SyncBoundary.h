/*
 * Minimal no-heap MPEG stream sync fragment helper, used by MP3 Input().
 * Keep the trailing 0xFF byte when an MPEG header straddles a network read.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

namespace audio_mp3 {
inline int findSyncHeader(const uint8_t* data, int length) {
    if (!data || length < 2) return -1;
    for (int i = 0; i + 1 < length; ++i) {
        if (data[i] == 0xFF && (data[i + 1] & 0xE0) == 0xE0)
            return i;
    }
    return -1;
}

inline int carryPartialSync(uint8_t* data, int length) {
    if (!data || length <= 0 || data[length - 1] != 0xFF) return 0;
    data[0] = 0xFF;
    return 1;
}
} // namespace audio_mp3

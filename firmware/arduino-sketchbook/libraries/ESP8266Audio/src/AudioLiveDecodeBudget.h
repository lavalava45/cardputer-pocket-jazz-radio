/*
 * Opt-in cooperative work budget for a live MP3 decoder iteration.
 * No network access, allocation, or clock reads are performed here.
 * Default upstream MP3 file playback remains unlimited.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

class AudioLiveDecodeBudget {
public:
    void setEnabled(bool enabled) { enabled_ = enabled; }
    void begin(uint32_t now) { startedAt_ = now; steps_ = 0; }
    bool tryStep(uint32_t now) {
        if (!enabled_) return true;
        if (steps_ >= 8 || (uint32_t)(now - startedAt_) >= 8) return false;
        ++steps_;
        return true;
    }
private:
    bool enabled_ = false;
    uint32_t startedAt_ = 0;
    uint8_t steps_ = 0;
};

/*
 * Optional bounded "would block" policy for live AudioGeneratorMP3 inputs.
 * This local extension is disabled by default for upstream callers.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>

class AudioLiveStallPolicy {
public:
    void setTimeoutMs(uint32_t timeoutMs) {
        timeoutMs_ = timeoutMs;
        reset();
    }
    void reset() {
        waiting_ = false;
        since_ = 0;
    }
    void progress() { reset(); }
    bool waiting() const { return waiting_; }

    // Both zero and UINT32_MAX represent an unknown-length live body. A known
    // finite Content-Length is NOT EOF until the source has consumed that
    // many bytes. Previously ANY finite size caused an immediate stop on a
    // transient empty socket, even when position < reportedSize. This applies
    // only when the caller explicitly enables bounded live waiting.
    bool shouldWait(uint32_t now, bool sourceOpen, uint32_t reportedSize,
                    uint32_t reportedPos) {
        const bool atFiniteEnd = reportedSize != 0 &&
                                 reportedSize != UINT32_MAX &&
                                 reportedPos >= reportedSize;
        if (!timeoutMs_ || !sourceOpen || atFiniteEnd) {
            reset();
            return false;
        }
        if (!waiting_) {
            waiting_ = true;
            since_ = now;
        }
        // Safe across millis() rollover. Always finite, never a permanent
        // silence after a dead-but-TCP-connected station.
        return (uint32_t)(now - since_) < timeoutMs_;
    }
private:
    uint32_t timeoutMs_ = 0;
    uint32_t since_ = 0;
    bool waiting_ = false;
};

// Distinct from byte-level waiting: a live server may trickle one junk byte
// every few seconds, keeping a transport timeout reset while no valid MP3
// frame is ever decoded. A frame-progress deadline bounds this failure mode
// without treating occasional network gaps as immediate EOF.
class AudioLiveFrameWatchdog {
public:
    void setDeadlineMs(uint32_t limit) {
        limitMs_ = limit;
        seenStart_ = false;
    }
    void start(uint32_t now) {
        lastFrameAt_ = now;
        seenStart_ = true;
    }
    void decodedFrame(uint32_t now) {
        lastFrameAt_ = now;
        seenStart_ = true;
    }
    bool expired(uint32_t now) const {
        return limitMs_ && seenStart_ &&
               (uint32_t)(now - lastFrameAt_) >= limitMs_;
    }
private:
    uint32_t limitMs_ = 0;
    uint32_t lastFrameAt_ = 0;
    bool seenStart_ = false;
};

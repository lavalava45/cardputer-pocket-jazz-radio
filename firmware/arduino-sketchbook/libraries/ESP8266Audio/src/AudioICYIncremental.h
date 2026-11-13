/*
 * Pocket Jazz vendored ESP8266Audio: no-wait ICY metadata state.
 * Fixed-size metadata parser shared with verified HTTPS source; no Strings,
 * heap allocation, SD, network operations, delays or clock reads here.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "AudioICYDeadline.h"
#include "../../../../PocketJazzRadio/IcyMetadataText.h"

namespace audio_icy {

class IncrementalMetadata {
 public:
  static const uint32_t kTimeoutMs = 2500;
  static const uint32_t kMaxPerPoll = 256;

  void reset(uint32_t interval = 0) {
    interval_ = interval;
    audioLeft_ = interval;
    metadataLeft_ = 0;
    waitingLength_ = false;
    deadlineStarted_ = false;
    malformed_ = false;
    parser_.reset();
  }
  IncrementalMetadata() { reset(); }

  // No metadata for interval=0; audio bytes are always allowed.
  uint32_t audioAllowance(uint32_t requested) const {
    return interval_ ? (requested < audioLeft_ ? requested : audioLeft_) : requested;
  }
  bool needsMetadata() const {
    return interval_ != 0 && audioLeft_ == 0;
  }
  bool waitingLength() const { return waitingLength_; }
  uint32_t metadataRemaining() const { return metadataLeft_; }

  bool acceptAudio(uint32_t got) {
    if (interval_ && got > audioLeft_) return false;
    if (interval_) audioLeft_ -= got;
    return true;
  }
  void enterMetadata(uint32_t now) {
    if (!needsMetadata() || deadlineStarted_) return;
    waitingLength_ = true;
    startedAt_ = now;
    deadlineStarted_ = true;
    metadataLeft_ = 0;
    malformed_ = false;
    parser_.reset();
  }
  bool timedOut(uint32_t now) const {
    return deadlineStarted_ && metadataTimedOut(startedAt_, now, kTimeoutMs);
  }
  bool acceptLength(uint8_t lengthByte) {
    if (!waitingLength_ || !deadlineStarted_) return false;
    waitingLength_ = false;
    metadataLeft_ = static_cast<uint32_t>(lengthByte) * 16u;
    if (!metadataLeft_) completeBlock();
    return true;
  }
  // Each input piece is bounded by remaining metadata. Title is published only
  // after the entire advertised block is consumed, never from a partial chunk.
  bool acceptMetadata(const uint8_t* bytes, uint32_t count,
                      char (&title)[pocketjazz::IcyMetadataText::kTitleCapacity]) {
    title[0] = 0;
    if (!deadlineStarted_ || waitingLength_ || !metadataLeft_ ||
        (!bytes && count) || count > metadataLeft_) return false;
    if (!malformed_ && !parser_.feed(bytes, count)) malformed_ = true;
    metadataLeft_ -= count;
    if (!metadataLeft_) {
      if (!malformed_) parser_.finish(title);
      completeBlock();
    }
    return true;
  }
  uint32_t interval() const { return interval_; }
  uint32_t audioRemaining() const { return audioLeft_; }

 private:
  void completeBlock() {
    metadataLeft_ = 0;
    waitingLength_ = false;
    deadlineStarted_ = false;
    audioLeft_ = interval_;
    malformed_ = false;
    parser_.reset();
  }
  uint32_t interval_ = 0;
  uint32_t audioLeft_ = 0;
  uint32_t metadataLeft_ = 0;
  uint32_t startedAt_ = 0;
  bool waitingLength_ = false;
  bool deadlineStarted_ = false;
  bool malformed_ = false;
  pocketjazz::IcyMetadataText parser_;
};
} // namespace audio_icy

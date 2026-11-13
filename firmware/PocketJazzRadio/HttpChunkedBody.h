// Bounded, allocation-free HTTP/1.1 chunked-transfer framing decoder.
// Chunk markers must be stripped BEFORE ICY metadata accounting and MP3 decode.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef POCKETJAZZ_HTTP_CHUNKED_BODY_H
#define POCKETJAZZ_HTTP_CHUNKED_BODY_H

#include <cstddef>
#include <cstdint>

namespace pocketjazz {

class HttpChunkedBody {
 public:
  enum class State : uint8_t {
    SIZE, EXTENSION, SIZE_LF, PAYLOAD, PAYLOAD_CR, PAYLOAD_LF,
    TRAILER, TRAILER_LF, DONE, INVALID
  };

  void reset() {
    state_ = State::SIZE;
    size_ = remaining_ = 0;
    digits_ = extensionLength_ = trailerBytes_ = 0;
    trailerLineEmpty_ = true;
  }

  HttpChunkedBody() { reset(); }

  State state() const { return state_; }
  bool done() const { return state_ == State::DONE; }
  bool invalid() const { return state_ == State::INVALID; }
  uint32_t payloadRemaining() const {
    return state_ == State::PAYLOAD ? remaining_ : 0;
  }

  // Called ONLY for framing bytes. Bulk payload bytes are passed to
  // consumePayload() without per-byte virtual network reads.
  bool consumeFraming(uint8_t byte) {
    switch (state_) {
      case State::SIZE: {
        const int hex = digit(byte);
        if (hex >= 0) {
          if (digits_ >= 8 || size_ > ((kMaxChunk - static_cast<uint32_t>(hex)) / 16u))
            return reject();
          size_ = size_ * 16u + static_cast<uint32_t>(hex);
          ++digits_;
          return true;
        }
        if (!digits_) return reject();
        if (byte == ';') {
          state_ = State::EXTENSION;
          return true;
        }
        if (byte == '\r') {
          state_ = State::SIZE_LF;
          return true;
        }
        return reject();
      }
      case State::EXTENSION:
        if (byte == '\r') {
          state_ = State::SIZE_LF;
          return true;
        }
        if (byte < 32 || byte > 126 || ++extensionLength_ > 80)
          return reject();
        return true;
      case State::SIZE_LF:
        if (byte != '\n') return reject();
        remaining_ = size_;
        if (remaining_) state_ = State::PAYLOAD;
        else {
          state_ = State::TRAILER;
          trailerLineEmpty_ = true;
        }
        return true;
      case State::PAYLOAD:
        return reject();  // Payload is consumed exclusively via consumePayload.
      case State::PAYLOAD_CR:
        if (byte != '\r') return reject();
        state_ = State::PAYLOAD_LF;
        return true;
      case State::PAYLOAD_LF:
        if (byte != '\n') return reject();
        size_ = 0;
        digits_ = extensionLength_ = 0;
        state_ = State::SIZE;
        return true;
      case State::TRAILER:
        if (byte == '\r') {
          state_ = State::TRAILER_LF;
          return true;
        }
        if (byte < 32 || byte > 126 || ++trailerBytes_ > 512)
          return reject();
        trailerLineEmpty_ = false;
        return true;
      case State::TRAILER_LF:
        if (byte != '\n') return reject();
        if (trailerLineEmpty_) state_ = State::DONE;
        else {
          trailerLineEmpty_ = true;
          state_ = State::TRAILER;
        }
        return true;
      case State::DONE:
      case State::INVALID:
        return reject();
    }
    return reject();
  }

  bool consumePayload(size_t count) {
    if (state_ != State::PAYLOAD || count > remaining_) return reject();
    remaining_ -= static_cast<uint32_t>(count);
    if (!remaining_) state_ = State::PAYLOAD_CR;
    return true;
  }

 private:
  static constexpr uint32_t kMaxChunk = 16u * 1024u * 1024u;
  static int digit(uint8_t byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
  }
  bool reject() {
    state_ = State::INVALID;
    return false;
  }
  State state_ = State::SIZE;
  uint32_t size_ = 0;
  uint32_t remaining_ = 0;
  uint16_t trailerBytes_ = 0;
  uint8_t digits_ = 0;
  uint8_t extensionLength_ = 0;
  bool trailerLineEmpty_ = true;
};
} // namespace pocketjazz
#endif // POCKETJAZZ_HTTP_CHUNKED_BODY_H

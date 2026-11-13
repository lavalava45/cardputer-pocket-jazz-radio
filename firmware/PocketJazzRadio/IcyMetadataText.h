// SPDX-License-Identifier: MIT
// Bounded, allocation-free ICY metadata text parser; C++11, no Arduino I/O.
#pragma once
#ifndef POCKETJAZZ_ICY_METADATA_TEXT_H
#define POCKETJAZZ_ICY_METADATA_TEXT_H
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {

// Null/empty/explicit zero ICY header means no metadata. Nonzero intervals
// must be pure decimal digits, 1..1048576; no signs or whitespace.
inline bool parseIcyMetaInterval(const char* raw, uint32_t& result) {
  result = 0;
  if (!raw || !*raw) return true;
  uint32_t parsed = 0;
  for (size_t i = 0; raw[i]; ++i) {
    const char c = raw[i];
    if (c < '0' || c > '9') return false;
    const uint32_t digit = static_cast<uint32_t>(c - '0');
    if (parsed > (1048576u - digit) / 10u) return false;
    parsed = parsed * 10u + digit;
  }
  result = parsed;
  return true;
}

class IcyMetadataText {
 public:
  static const size_t kMaxMetadataBytes = 4080; // 255 * 16
  static const size_t kTitleCapacity = 96;       // 95 bytes + NUL

  IcyMetadataText() { reset(); }
  void reset() {
    state_ = SEEK;
    skippedQuote_ = 0;
    titleQuote_ = 0;
    matched_ = 0;
    consumed_ = 0;
    titleLength_ = 0;
    quoteSpaces_ = 0;
    pendingLength_ = 0;
    expectedLength_ = 0;
    outputFull_ = false;
    title_[0] = 0;
  }

  // Feed ONLY bytes after the ICY length byte, preserving state across reads.
  // False means malformed/oversized input; do not publish partial titles.
  bool feed(const uint8_t* bytes, size_t count) {
    if (state_ == BAD || (!bytes && count) || count > kMaxMetadataBytes - consumed_) {
      state_ = BAD;
      return false;
    }
    consumed_ += count;
    for (size_t i = 0; i < count; ++i) {
      const uint8_t c = bytes[i];
      switch (state_) {
        case SEEK:
          if (c == ' ' || c == '\t' || c == ';') break;
          if (c == 0) { state_ = PAD; break; }
          if (c == 'S') { state_ = KEY; matched_ = 1; }
          else { state_ = SKIP; skippedQuote_ = (c == '\'' || c == '"') ? c : 0; }
          break;
        case KEY: {
          static const char key[] = "StreamTitle";
          if (c == static_cast<uint8_t>(key[matched_])) {
            ++matched_;
            if (matched_ == sizeof(key) - 1) state_ = AFTER_KEY;
          } else if (c == ';') state_ = SEEK;
          else { state_ = SKIP; skippedQuote_ = (c == '\'' || c == '"') ? c : 0; }
          break;
        }
        case SKIP:
          if (c == 0) state_ = PAD;
          else if (skippedQuote_) { if (c == skippedQuote_) skippedQuote_ = 0; }
          else if (c == '\'' || c == '"') skippedQuote_ = c;
          else if (c == ';') state_ = SEEK;
          break;
        case AFTER_KEY:
          if (c == ' ' || c == '\t') break;
          if (c == '=') state_ = BEFORE_VALUE;
          else {
            // "StreamTitleExtra" starts with the same bytes but is a
            // different metadata key. Skip it, then continue looking for
            // an exact StreamTitle assignment later in the same block.
            state_ = c == ';' ? SEEK : SKIP;
            skippedQuote_ = (c == '\'' || c == '"') ? c : 0;
          }
          break;
        case BEFORE_VALUE:
          if (c == ' ' || c == '\t') break;
          if (c == '\'' || c == '"') { titleQuote_ = c; state_ = VALUE; }
          else state_ = BAD;
          break;
        case VALUE:
          if (c == titleQuote_) {
            // A quote might be a word apostrophe. Decide only once the next
            // byte (possibly in another read) proves it is the final quote.
            quoteSpaces_ = 0;
            state_ = MAYBE_CLOSE;
          } else if (c == 0) state_ = BAD;
          else acceptUtf8(c);
          break;
        case MAYBE_CLOSE:
          if (c == ' ' || c == '\t') {
            // Preserve the old allowance for whitespace after the closing
            // quote, but it might still be an apostrophe inside the title.
            ++quoteSpaces_; // bounded by kMaxMetadataBytes for entire feed
          } else if (c == ';' || c == 0) {
            flushMalformedPending();
            state_ = (c == ';') ? DONE : PAD_DONE;
          } else {
            // Not a terminator: the saved quote and any following whitespace
            // are part of the title. A second quote may itself be terminal.
            acceptUtf8(titleQuote_);
            for (size_t j = 0; j < quoteSpaces_ && !outputFull_; ++j)
              acceptUtf8(' ');
            quoteSpaces_ = 0;
            if (c == titleQuote_) state_ = MAYBE_CLOSE;
            else { state_ = VALUE; acceptUtf8(c); }
          }
          break;
        case AFTER_VALUE:
          if (c == ' ' || c == '\t') break;
          if (c == ';') state_ = DONE;
          else if (c == 0) state_ = PAD_DONE;
          else state_ = BAD;
          break;
        case DONE:
          // Other metadata fields are not needed, but NUL padding must be
          // terminal. A nonzero byte after padding invalidates this block.
          if (c == 0) state_ = PAD_DONE;
          break;
        case PAD:
          if (c != 0) state_ = BAD;
          break;
        case PAD_DONE:
          if (c != 0) state_ = BAD;
          break;
        case BAD: return false;
      }
      if (state_ == BAD) return false;
    }
    return true;
  }

  bool feed(const char* bytes, size_t count) {
    return feed(reinterpret_cast<const uint8_t*>(bytes), count);
  }

  // Call only at the end of the advertised ICY metadata block. A returned
  // title is UTF-8, NUL terminated, no more than 95 bytes, or false/empty.
  bool finish(char (&out)[kTitleCapacity]) {
    out[0] = '\0';
    // A final quote at the exact metadata-block end is also a valid close.
    if (state_ == MAYBE_CLOSE) {
      flushMalformedPending();
      state_ = AFTER_VALUE;
    }
    if (state_ != AFTER_VALUE && state_ != DONE && state_ != PAD_DONE) return false;
    size_t begin = 0;
    while (begin < titleLength_ && title_[begin] == ' ') ++begin;
    size_t end = titleLength_;
    while (end > begin && title_[end - 1] == ' ') --end;
    const size_t length = end - begin;
    if (!length) return false;
    std::memcpy(out, title_ + begin, length);
    out[length] = 0;
    return true;
  }

  size_t consumedBytes() const { return consumed_; }

 private:
  enum State : uint8_t {
    SEEK, KEY, SKIP, AFTER_KEY, BEFORE_VALUE, VALUE,
    MAYBE_CLOSE, AFTER_VALUE, DONE, PAD, PAD_DONE, BAD
  };
  State state_;
  uint8_t skippedQuote_;
  uint8_t titleQuote_;
  uint8_t matched_;
  size_t consumed_;
  size_t titleLength_;
  uint16_t quoteSpaces_;
  uint8_t pending_[4];
  uint8_t pendingLength_;
  uint8_t expectedLength_;
  bool outputFull_;
  char title_[kTitleCapacity];

  static bool forbidden(uint32_t cp) {
    return cp < 32u || (cp >= 127u && cp <= 159u) ||
           cp == 0x2028u || cp == 0x2029u || cp == 0xfeffu ||
           (cp >= 0x200bu && cp <= 0x200fu) ||
           (cp >= 0x202au && cp <= 0x202eu) ||
           (cp >= 0x2060u && cp <= 0x206fu);
  }
  void appendSpace() {
    if (outputFull_) return;
    if (titleLength_ < kTitleCapacity - 1) title_[titleLength_++] = ' ';
    else outputFull_ = true;
  }
  void appendCodepoint(uint32_t cp) {
    if (forbidden(cp)) { appendSpace(); return; }
    if (outputFull_) return;
    if (titleLength_ + pendingLength_ > kTitleCapacity - 1) {
      outputFull_ = true;
      return;
    }
    for (size_t i = 0; i < pendingLength_; ++i)
      title_[titleLength_++] = static_cast<char>(pending_[i]);
  }
  void flushMalformedPending() {
    if (pendingLength_) appendSpace();
    pendingLength_ = expectedLength_ = 0;
  }
  void acceptUtf8(uint8_t c) {
    if (!pendingLength_) {
      if (c < 0x80) {
        if (forbidden(c)) appendSpace();
        else if (!outputFull_) {
          if (titleLength_ < kTitleCapacity - 1) title_[titleLength_++] = static_cast<char>(c);
          else outputFull_ = true;
        }
        return;
      }
      if (c >= 0xc2 && c <= 0xdf) expectedLength_ = 2;
      else if (c >= 0xe0 && c <= 0xef) expectedLength_ = 3;
      else if (c >= 0xf0 && c <= 0xf4) expectedLength_ = 4;
      else { appendSpace(); return; }
      pending_[0] = c;
      pendingLength_ = 1;
      return;
    }
    if ((c & 0xc0u) != 0x80u) {
      flushMalformedPending();
      acceptUtf8(c); // reprocess first byte of the next sequence
      return;
    }
    pending_[pendingLength_++] = c;
    if (pendingLength_ != expectedLength_) return;
    uint32_t cp = pending_[0] & (expectedLength_ == 2 ? 0x1fu : expectedLength_ == 3 ? 0x0fu : 0x07u);
    for (size_t i = 1; i < pendingLength_; ++i) cp = (cp << 6) | (pending_[i] & 0x3fu);
    const bool valid = !(expectedLength_ == 2 && cp < 0x80u) &&
                       !(expectedLength_ == 3 && cp < 0x800u) &&
                       !(expectedLength_ == 4 && cp < 0x10000u) &&
                       !(cp >= 0xd800u && cp <= 0xdfffu) && cp <= 0x10ffffu;
    if (valid) appendCodepoint(cp);
    else appendSpace();
    pendingLength_ = expectedLength_ = 0;
  }
};

static_assert(sizeof(IcyMetadataText) <= 256, "ICY metadata state must stay small");
} // namespace pocketjazz
#endif // POCKETJAZZ_ICY_METADATA_TEXT_H

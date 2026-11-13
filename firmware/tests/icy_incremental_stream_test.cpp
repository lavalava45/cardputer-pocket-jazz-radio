#ifdef NDEBUG
#undef NDEBUG // Assert conditions must be ACTIVE in MSVC Release.
#endif
#include "../arduino-sketchbook/libraries/ESP8266Audio/src/AudioICYIncremental.h"
#include "../PocketJazzRadio/HttpChunkedBody.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <deque>

using audio_icy::IncrementalMetadata;
using pocketjazz::HttpChunkedBody;

static std::string metadata16(const std::string& value) {
  assert(value.size() <= 16);
  return value + std::string(16 - value.size(), '\0');
}

struct MockPlayer {
  IncrementalMetadata state;
  std::string pcmInput;
  std::string title;
  unsigned operations = 0;

  explicit MockPlayer(uint32_t interval) { state.reset(interval); }
  // Simulate one audio-worker poll with a finite number of delivered network
  // bytes. Never wait for a missing metadata byte (the source returns zero).
  size_t poll(const std::string& wire, size_t pos, size_t supplied, uint32_t now) {
    const size_t limit = std::min(wire.size(), pos + supplied);
    uint32_t budget = IncrementalMetadata::kMaxPerPoll;
    for (int i = 0; i < 4 && pos < limit; ++i) {
      ++operations;
      if (state.needsMetadata()) {
        state.enterMetadata(now);
        if (state.timedOut(now)) return pos;
        if (state.waitingLength()) {
          assert(state.acceptLength(static_cast<uint8_t>(wire[pos++])));
          continue;
        }
        const uint32_t count = std::min<uint32_t>(128,
            std::min<uint32_t>(state.metadataRemaining(),
                std::min<uint32_t>(budget, static_cast<uint32_t>(limit - pos))));
        if (!count) return pos;
        char text[pocketjazz::IcyMetadataText::kTitleCapacity]{};
        assert(state.acceptMetadata(reinterpret_cast<const uint8_t*>(wire.data() + pos),
                                    count, text));
        if (text[0]) title = text;
        budget -= count;
        pos += count;
      } else {
        const uint32_t count = state.audioAllowance(static_cast<uint32_t>(limit - pos));
        assert(count && state.acceptAudio(count));
        pcmInput.append(wire, pos, count);
        pos += count;
        // Release MP3 immediately; metadata is never awaited by this poll.
        return pos;
      }
    }
    return pos;
  }
};

static std::string dechunk(const std::string& wire, size_t split) {
  HttpChunkedBody chunks;
  std::string output;
  size_t pos = 0;
  while (pos < wire.size() && !chunks.done() && !chunks.invalid()) {
    const size_t available = std::min(split, wire.size() - pos);
    size_t consumed = 0;
    while (consumed < available && !chunks.done() && !chunks.invalid()) {
      if (chunks.payloadRemaining()) {
        const size_t len = std::min<size_t>(chunks.payloadRemaining(), available - consumed);
        output.append(wire, pos + consumed, len);
        assert(chunks.consumePayload(len));
        consumed += len;
      } else {
        chunks.consumeFraming(static_cast<uint8_t>(wire[pos + consumed]));
        ++consumed;
      }
    }
    pos += consumed;
  }
  assert(chunks.done() && !chunks.invalid());
  return output;
}

static std::string run(const std::string& wire, size_t split, std::string& title) {
  MockPlayer p(3);
  size_t pos = 0;
  uint32_t now = 100;
  // Every possible fragmented source read, including one byte at a time.
  while (pos < wire.size()) {
    const size_t earlier = pos;
    pos = p.poll(wire, pos, split, now++);
    assert(pos > earlier); // No infinite loop when supplied input exists.
  }
  title = p.title;
  return p.pcmInput;
}

int main() {
  static_assert(sizeof(IncrementalMetadata) <= 288,
                "Metadata decoder must be fixed-size and under 288 B");
  const std::string good = std::string("abc") + '\x01' +
                           metadata16("StreamTitle='A';") + "def";
  for (size_t split = 1; split <= good.size(); ++split) {
    std::string title;
    assert(run(good, split, title) == "abcdef" && title == "A");
  }
  // The ICY block may straddle independent HTTP chunk boundaries: the raw
  // HTTP framing is removed BEFORE ICY boundaries are measured.
  const std::string chunked = "2\r\nab\r\n4\r\nc\x01" + metadata16("St").substr(0, 2) +
      "\r\ne\r\n" + metadata16("StreamTitle='A';").substr(2, 14) +
      "\r\n3\r\ndef\r\n0\r\n\r\n";
  for (size_t fragment = 1; fragment <= 40; ++fragment) {
    const std::string raw = dechunk(chunked, fragment);
    assert(raw == good);
    std::string title;
    assert(run(raw, fragment, title) == "abcdef" && title == "A");
  }
  // As a metadata block stalls, bytes already in ring must remain playable.
  MockPlayer stalled(3);
  const std::string prefix = "abc\x01St";
  size_t pos = 0;
  for (size_t i = 0; i < prefix.size(); ++i)
    pos = stalled.poll(prefix, pos, 1, 1000);
  assert(stalled.pcmInput == "abc" && stalled.state.metadataRemaining() == 14);
  // Model the already-prefilled AudioFileSourceBuffer ring: the decoder can
  // consume these MP3 bytes even though the next network fragment is only
  // partial ICY metadata. No parser poll may erase/reset those buffered bytes.
  std::deque<uint8_t> ring(stalled.pcmInput.begin(), stalled.pcmInput.end());
  assert(ring.size() == 3);
  assert(ring.front() == 'a'); ring.pop_front();
  assert(ring.front() == 'b'); ring.pop_front();
  assert(ring.front() == 'c'); ring.pop_front();
  assert(ring.empty() && stalled.state.metadataRemaining() == 14);
  const unsigned oldOps = stalled.operations;
  assert(stalled.poll(prefix, pos, 0, 3499) == pos);
  assert(!stalled.state.timedOut(3499) && stalled.operations == oldOps);
  assert(stalled.state.timedOut(3500));
  // If socket EOF happens mid-block, the owning worker must close/reopen;
  // state reset is exact and never treats metadata leftovers as MP3 payload.
  stalled.state.reset(3);
  assert(stalled.state.audioRemaining() == 3 &&
         stalled.state.metadataRemaining() == 0 && !stalled.state.timedOut(9900));
  // Overflow and rollover cannot disable the shared 2.5-second timeout.
  IncrementalMetadata rollover;
  rollover.reset(1);
  assert(rollover.acceptAudio(1));
  rollover.enterMetadata(UINT32_MAX - 1499);
  assert(!rollover.timedOut(999) && rollover.timedOut(1000));

  // Malformed StreamTitle must not inject text or shift the following audio.
  const std::string malformed = std::string("abc") + '\x01' +
      metadata16("StreamTitle=X;") + "def";
  for (size_t split = 1; split <= 7; ++split) {
    std::string title;
    assert(run(malformed, split, title) == "abcdef" && title.empty());
  }
  // Zero metadata length restores audio immediately; subsequent blocks work.
  const std::string zero = std::string("abc") + '\0' + "def" + '\x01' +
                           metadata16("StreamTitle='B';") + "ghi";
  std::string title;
  assert(run(zero, 1, title) == "abcdefghi" && title == "B");
  // Reconnect discards partial metadata parser and resumes at new MP3 boundary.
  stalled.state.reset(3);
  assert(stalled.state.audioRemaining() == 3 && !stalled.state.needsMetadata());
  stalled.pcmInput.clear();
  stalled.title.clear();
  const std::string newStream = std::string("xyz") + '\x01' +
      metadata16("StreamTitle='C';") + "uvw";
  pos = 0;
  while (pos < newStream.size()) pos = stalled.poll(newStream, pos, 1, 4000);
  assert(stalled.pcmInput == "xyzuvw" && stalled.title == "C");
  return 0;
}

#ifdef NDEBUG
#undef NDEBUG // Release builds must execute every regression assertion.
#endif
#include "../PocketJazzRadio/HttpChunkedBody.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

using pocketjazz::HttpChunkedBody;

// Model a nonblocking TLS stream delivering at most 'split' raw network bytes
// per poll. Parser uses exactly the production state-machine API.
static std::string strip(const std::string& wire, size_t split, bool& done, bool& invalid) {
  HttpChunkedBody decoder;
  std::string payload;
  size_t cursor = 0;
  while (cursor < wire.size() && !decoder.done() && !decoder.invalid()) {
    const size_t until = cursor + (split < wire.size() - cursor ? split : wire.size() - cursor);
    while (cursor < until && !decoder.done() && !decoder.invalid()) {
      if (decoder.payloadRemaining()) {
        const size_t bytes = decoder.payloadRemaining() < until - cursor
                               ? decoder.payloadRemaining() : until - cursor;
        payload.append(wire, cursor, bytes);
        assert(decoder.consumePayload(bytes));
        cursor += bytes;
      } else {
        const uint8_t framing = static_cast<uint8_t>(wire[cursor++]);
        decoder.consumeFraming(framing);
      }
    }
  }
  done = decoder.done();
  invalid = decoder.invalid();
  return payload;
}

static std::string chunk(const std::string& sizeLine, const std::string& payload) {
  return sizeLine + "\r\n" + payload + "\r\n";
}

int main() {
  static_assert(sizeof(HttpChunkedBody) <= 32, "Decoder state must stay small and fixed-size");
  // FIP Jazz's observed HTTP body begins with 'd8a\r\n' (3466 bytes),
  // followed by MP3 data. Non-MP3 chunk framing must never reach decoder.
  std::string first(0xd8a, '\x55');
  first[0] = static_cast<char>(0xff);
  first[1] = static_cast<char>(0xfb);
  std::string second(512, '\x66');
  const std::string wire = chunk("d8a", first) +
                           chunk("200;station=fip", second) +
                           "0\r\nX-Status: done\r\n\r\n";
  const std::string expected = first + second;
  for (size_t fragment = 1; fragment <= 550; ++fragment) {
    bool done = false, invalid = false;
    const std::string actual = strip(wire, fragment, done, invalid);
    assert(done && !invalid && actual == expected);
    assert(static_cast<uint8_t>(actual[0]) == 0xff);
    assert(static_cast<uint8_t>(actual[1]) == 0xfb);
  }

  // ICY metadata crosses an unrelated HTTP chunk boundary. After decoding
  // chunks, the original ICY byte stream must remain unchanged.
  std::string icy = "MP3abc";
  icy += '\x01';  // one 16-byte metadata block
  icy += "StreamTitle='A';"; // exactly 16 bytes? adjust below
  assert(icy.size() == 23);
  const std::string metaWire = chunk("3", icy.substr(0, 3)) +
                               chunk("a", icy.substr(3, 10)) +
                               chunk("a", icy.substr(13, 10)) + "0\r\n\r\n";
  for (size_t fragment = 1; fragment <= 30; ++fragment) {
    bool done = false, invalid = false;
    assert(strip(metaWire, fragment, done, invalid) == icy && done && !invalid);
  }

  const char* malformed[] = {
    "\r\n", "g\r\n", "1\n", "1\rX", "0\rX",
    "1\r\nA\n\n", "1\r\nA\rX",
    "fffffffff\r\n", "1000001\r\n", // oversize >16MiB
    "1;\nA\r\n", "0\r\nBad\x01Trailer\r\n\r\n"
  };
  for (const char* bad : malformed) {
    bool done = false, invalid = false;
    strip(bad, 1, done, invalid);
    assert(invalid && !done);
  }
  for (const char* truncated : {"d8", "1\r", "1\r\nX", "0\r\n", "0\r\nX-Status: ok\r\n"}) {
    bool done = false, invalid = false;
    strip(truncated, 1, done, invalid);
    assert(!done && !invalid);
  }
  HttpChunkedBody parser;
  assert(!parser.consumePayload(1) && parser.invalid());  // Cannot skip delimiters.
  parser.reset();
  assert(parser.state() == HttpChunkedBody::State::SIZE);
  assert(parser.consumeFraming('1') && parser.consumeFraming('\r') &&
         parser.consumeFraming('\n'));
  assert(parser.payloadRemaining() == 1);
  assert(!parser.consumePayload(2) && parser.invalid());
  return 0;
}

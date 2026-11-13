#ifdef NDEBUG
#undef NDEBUG // Active behavior assertions in Release.
#endif
#include "../PocketJazzRadio/IcyMetadataText.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using pocketjazz::IcyMetadataText;
using pocketjazz::parseIcyMetaInterval;

static bool parse(const std::string& raw, std::string& result, size_t split = 4096) {
  IcyMetadataText parser;
  for (size_t i = 0; i < raw.size(); i += split) {
    const size_t count = (raw.size() - i < split) ? raw.size() - i : split;
    if (!parser.feed(raw.data() + i, count)) return false;
  }
  char out[96];
  if (!parser.finish(out)) return false;
  result = out;
  return true;
}

int main() {
  static_assert(sizeof(IcyMetadataText) <= 256, "No large scratch buffers");
  static_assert(IcyMetadataText::kMaxMetadataBytes == 4080, "ICY length byte bound");
  uint32_t interval = 77;
  assert(parseIcyMetaInterval(nullptr, interval) && interval == 0);
  assert(parseIcyMetaInterval("", interval) && interval == 0);
  assert(parseIcyMetaInterval("0", interval) && interval == 0);
  assert(parseIcyMetaInterval("000", interval) && interval == 0);
  assert(parseIcyMetaInterval("1", interval) && interval == 1);
  assert(parseIcyMetaInterval("1048576", interval) && interval == 1048576);
  assert(parseIcyMetaInterval("000001", interval) && interval == 1);
  for (const char* bad : {"-1", "+1", " 1", "1 ", "1xyz", "1.0",
                          "1048577", "4294967296", "999999999999999999999"}) {
    interval = 55;
    assert(!parseIcyMetaInterval(bad, interval) && interval == 0);
  }

  std::string title;
  const std::string meta = "StreamUrl='https://example.com';StreamTitle = '  Miles Davis - So What  ';";
  assert(parse(meta, title) && title == "Miles Davis - So What");
  // Every two-chunk split, including inside key, quote and value.
  for (size_t split = 1; split <= meta.size(); ++split)
    assert(parse(meta, title, split) && title == "Miles Davis - So What");
  assert(parse("StreamTitle=\"Blue Train\";", title) && title == "Blue Train");
  assert(parse("StreamTitle='Kind of Blue'", title) && title == "Kind of Blue");
  const std::string apostrophe = "StreamTitle='Don't Stop';";
  for (size_t split = 1; split <= apostrophe.size(); ++split)
    assert(parse(apostrophe, title, split) && title == "Don't Stop");
  assert(parse("StreamTitle='It's Jazz'  ;", title, 1) && title == "It's Jazz");
  assert(parse("StreamTitle='I' ve Jazz';", title, 1) && title == "I' ve Jazz");
  assert(parse("StreamTitle='Don''t';", title, 1) && title == "Don''t");
  assert(parse("StreamTitle='Apostrophe at end'';", title, 1) &&
         title == "Apostrophe at end'");
  assert(parse("StreamTitle='Don't Stop'", title, 1) && title == "Don't Stop");
  assert(parse("StreamTitle='Hi';\0\0\0", title) && title == "Hi");
  assert(parse("StreamTitle='Hi'\0\0", title) && title == "Hi");
  const std::string realPadding = std::string("StreamTitle='OK';") + std::string(20, '\0');
  assert(parse(realPadding, title, 1) && title == "OK");
  const std::string embeddedNul = std::string("StreamTitle='A") + '\0' + "B';";
  assert(!parse(embeddedNul, title, 1));
  const std::string dirtyPadding = std::string("StreamTitle='A'") + '\0' + "B";
  assert(!parse(dirtyPadding, title));
  const std::string dirtyAfterSemicolon = std::string("StreamTitle='A';") + '\0' + "INVALID";
  for (size_t split = 1; split <= dirtyAfterSemicolon.size(); ++split)
    assert(!parse(dirtyAfterSemicolon, title, split));
  const std::string okayWithOtherField = std::string("StreamTitle='A';StreamUrl='x';") +
      std::string(10, '\0');
  assert(parse(okayWithOtherField, title, 1) && title == "A");
  assert(parse("Other='StreamTitle=\"FAKE\"';StreamTitle='REAL';", title) && title == "REAL");
  const std::string prefixLookalike =
      "StreamTitleExtra='Fake';StreamTitleBackup=\"No\";StreamTitle='Real';";
  for (size_t split = 1; split <= prefixLookalike.size(); ++split)
    assert(parse(prefixLookalike, title, split) && title == "Real");
  assert(!parse("StreamTitleExtra='Wrong';", title));

  for (const std::string& bad : {
         "", "StreamTitle='", "StreamTitle='No closing", "StreamTitle='A'junk;",
         "streamtitle='a';", "XStreamTitle='a';", "StreamTitle=noquote;",
         "StreamTitle='';", "StreamTitle='   ';", "StreamTitle='\0data';",
         "StreamTitle='a\0b';", "StreamTitle='oops\";", "StreamTitle='oops'X"}) {
    assert(!parse(bad, title));
  }

  const std::string printableUtf8 = std::string("StreamTitle='") +
    "Caf\xc3\xa9 - \xe6\x9d\xb1\xe4\xba\xac" + "';";
  for (size_t split = 1; split < 9; ++split)
    assert(parse(printableUtf8, title, split) &&
           title == "Caf\xc3\xa9 - \xe6\x9d\xb1\xe4\xba\xac");
  const std::string badUtf8 = std::string("StreamTitle='A") +
    "\xff\xc0\xaf" + " B';";
  assert(parse(badUtf8, title) && title == "A    B");
  const std::string truncatedUtf8 = std::string("StreamTitle='A") + "\xe2\x82" + "';";
  assert(parse(truncatedUtf8, title) && title == "A"); // dangling UTF8 becomes trailing space, trimmed
  assert(parse("StreamTitle='a\t\r\nb';", title) && title == "a   b");
  const std::string controls = std::string("StreamTitle='a") + "\xc2\x85" +
    "b\xe2\x80\xa8" + "c';";
  assert(parse(controls, title) && title == "a b c");

  const std::string longAscii = "StreamTitle='" + std::string(300, 'A') + "';";
  assert(parse(longAscii, title) && title == std::string(95, 'A'));
  const std::string longUtf8 = "StreamTitle='" + std::string(47, 'a') +
    "\xe6\x9d\xb1" + std::string(200, 'b') + "';";
  assert(parse(longUtf8, title) && title == std::string(47, 'a') + "\xe6\x9d\xb1" + std::string(45, 'b'));
  // Once a full UTF-8 codepoint cannot fit, never append later ASCII bytes.
  const std::string noFragment = "StreamTitle='" + std::string(94, 'x') +
    "\xe6\x9d\xb1" + "Y';";
  assert(parse(noFragment, title) && title == std::string(94, 'x'));

  const std::string padded = "StreamTitle='A';" + std::string(4080 - 16, '\0');
  // Wrong sizes are not silently accepted by the helper's scan budget.
  assert(parse(padded, title) && title == "A");
  assert(!parse(padded + "x", title));
  IcyMetadataText parser;
  char output[96];
  assert(!parser.finish(output) && !output[0]);
  assert(!parser.feed(static_cast<const uint8_t*>(nullptr), 1));
  assert(!parser.finish(output));
  parser.reset();
  assert(parser.feed("StreamTitle='Recovered';", 24));
  assert(parser.finish(output) && std::strcmp(output, "Recovered") == 0);
  return 0;
}

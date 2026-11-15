#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/JazzNotesLayout.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static int pixels(const char* text) { return int(strlen(text) * 6); }

int main(int argc, char** argv) {
  using pocketjazz::layoutNotes;
  assert(argc == 2);
  const char* extended =
    "Recorded a series of renowned performances and influential albums with "
    "an ensemble of fellow musicians; played across several jazz scenes; "
    "born in a distant city; maintained a lasting musical legacy.";
  std::string merged;
  for (size_t page = 0; page < 16; ++page) {
    auto layout = layoutNotes(extended, page, 222, pixels);
    for (unsigned i = 0; i < layout.count; ++i) {
      assert(pixels(layout.lines[i]) <= 222);
      if (!merged.empty()) merged += ' ';
      merged += layout.lines[i];
    }
    if (!layout.more) break;
    assert(page < 15);
  }
  assert(merged == extended);
  auto first = layoutNotes(extended, 0, 222, pixels);
  assert(first.count > 0);
  assert(!layoutNotes("", 0, 222, pixels).more);
  assert(!layoutNotes("text", 0, 0, pixels).count);
  const char* broadName =
      "The International Ensemble of Progressive New Jazz and Extended Improvisation";
  const auto title = layoutNotes(broadName, 0, 222, pixels, 1);
  assert(title.count == 1 && title.more);
  assert(pixels(title.lines[0]) <= 222);
  std::string nameJoined;
  for (size_t i = 0; i < 8; ++i) {
    const auto page = layoutNotes(broadName, i, 222, pixels);
    for (unsigned j = 0; j < page.count; ++j) {
      if (!nameJoined.empty()) nameJoined += ' ';
      nameJoined += page.lines[j];
    }
    if (!page.more) break;
  }
  assert(nameJoined == broadName);

  std::ifstream input(argv[1], std::ios::binary);
  assert(input.good());
  const std::string tsv{std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>()};
  size_t begin = 0, checked = 0, paged = 0;
  while (begin < tsv.size()) {
    const auto end = tsv.find('\n', begin);
    assert(end != std::string::npos);
    const std::string record = tsv.substr(begin, end - begin);
    const auto a = record.find('\t');
    const auto b = record.find('\t', a + 1);
    const auto c = record.find('\t', b + 1);
    assert(a != std::string::npos && b != std::string::npos &&
           c != std::string::npos);
    const std::string summary = record.substr(b + 1, c - b - 1);
    const auto d = record.find('\t', c + 1);
    assert(d != std::string::npos);
    const std::string album = record.substr(c + 1, d - c - 1);
    const size_t visibleRows = album == "-" ? 7 : 6;
    const std::string artistName = record.substr(a + 1, b - a - 1);
    const auto shownName = layoutNotes(artistName.c_str(), 0, 222, pixels, 3);
    assert(!shownName.more); // detail view must retain complete real artist name
    if (album != "-") {
      const auto shownAlbum = layoutNotes(album.c_str(), 0, 222, pixels, 3);
      assert(!shownAlbum.more); // all real recording names fit the detail view
    }
    std::string reconstructed;
    for (size_t page = 0; page < 16; ++page) {
      const auto layout = layoutNotes(summary.c_str(), page, 222, pixels,
                                    visibleRows);
      for (unsigned i = 0; i < layout.count; ++i) {
        assert(pixels(layout.lines[i]) <= 222);
        if (!reconstructed.empty()) reconstructed += ' ';
        reconstructed += layout.lines[i];
      }
      if (layout.more) ++paged;
      else break;
      assert(page < 15);
    }
    if (reconstructed != summary) {
      fprintf(stderr, "Jazz Notes layout mismatch: key=%s summary=%s reconstructed=%s\n",
              record.substr(0, a).c_str(), summary.c_str(), reconstructed.c_str());
    }
    assert(reconstructed == summary);
    ++checked;
    begin = end + 1;
  }
  assert(checked == 1498);
  fprintf(stdout, "Notes viewport: %zu biographies verified, %zu additional pages at 6px/glyph\n",
          checked, paged);
  return 0;
}

#ifdef NDEBUG
#undef NDEBUG  // Assertions are part of the MSVC Release test contract.
#endif
#include "../PocketJazzRadio/JazzNotesSearch.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using pocketjazz::JazzNote;
using pocketjazz::JazzNotesSearch;
using pocketjazz::NotesFile;

struct Memory {
  std::vector<uint8_t> bytes;
  bool failRead = false;
  size_t calls = 0;
};
static bool readMemory(void* ctx, size_t offset, uint8_t* out, size_t count) {
  Memory& file = *static_cast<Memory*>(ctx);
  ++file.calls;
  if (file.failRead || offset > file.bytes.size() ||
      count > file.bytes.size() - offset) return false;
  if (count) memcpy(out, file.bytes.data() + offset, count);
  return true;
}
static NotesFile fileOf(Memory& memory) {
  NotesFile file = {&memory, memory.bytes.size(), readMemory};
  return file;
}
static Memory diskFile(const std::string& path) {
  std::ifstream input(path.c_str(), std::ios::binary);
  assert(input.good());
  Memory file;
  file.bytes.assign(std::istreambuf_iterator<char>(input),
                    std::istreambuf_iterator<char>());
  assert(!file.bytes.empty());
  return file;
}
static Memory fromString(const std::string& data) {
  Memory memory;
  memory.bytes.assign(data.begin(), data.end());
  return memory;
}
static std::string s(const Memory& file) {
  return std::string(file.bytes.begin(), file.bytes.end());
}
static std::string firstLine(const Memory& file) {
  std::string all = s(file);
  return all.substr(0, all.find('\n') + 1);
}

int main(int argc, char** argv) {
  assert(argc == 3);
  assert(sizeof(JazzNotesSearch) <= 4096 && "Index must fit bounded RAM budget");
  assert(sizeof(JazzNote) <= 700);
  Memory tsv = diskFile(argv[1]);
  Memory idx = diskFile(argv[2]);
  assert(JazzNotesSearch::fnv("miles-davis") == 0x6d5862e3UL);
  assert(JazzNotesSearch::fnv("a") == 0xe40c292cUL);

  JazzNotesSearch notes;
  assert(notes.open(fileOf(tsv), fileOf(idx)));
  assert(notes.ready() && notes.indexed() && notes.rows() == 36);
  JazzNote item = {};
  assert(notes.lookup("miles-davis", item));
  assert(strcmp(item.album, "Kind of Blue") == 0);
  assert(strstr(item.artistUrl, "/561d854a-6a28-4aa7-8c99-323e6ce46c2a"));
  size_t before = tsv.calls;
  assert(notes.lookup("bill-evans", item));
  assert(tsv.calls-before < 16 && "indexed lookup should use buffered IO");
  assert(notes.lookup("john-coltrane", item));
  assert(strcmp(item.album, "Blue Train") == 0);
  assert(!notes.lookup("no-such-artist", item));
  assert(!notes.lookup("../bad", item));
  assert(!notes.lookup("", item));
  assert(!notes.lookup("Miles-Davis", item));

  JazzNote matches[3] = {};
  size_t total = 0;
  assert(notes.search("blue", matches, 3, &total) >= 1);
  assert(total >= 1);
  assert(notes.search("KIND OF BLUE", matches, 3, &total) == 1);
  assert(strcmp(matches[0].key, "miles-davis") == 0);
  assert(notes.search("Miles Davis", matches, 3, &total) == 1);
  assert(strcmp(matches[0].key, "miles-davis") == 0);
  assert(notes.search("the", matches, 1, &total) == 1);
  assert(total > 1); // Bounded results, truthful match count.
  const std::string firstThe = matches[0].key;
  size_t pageTotal = 0;
  assert(notes.search("the", matches, 1, &pageTotal, 1) == 1);
  assert(pageTotal == total);
  assert(firstThe != matches[0].key); // Offset advances to the next real match.
  assert(notes.search("the", matches, 3, &pageTotal, total) == 0);
  assert(pageTotal == total); // End-of-list does not pretend there are no matches.
  assert(notes.search("", matches, 3, &total) == 0 && total == 0);
  assert(notes.search("\x01", matches, 3, &total) == 0);
  assert(notes.search("miles", nullptr, 0, &total) == 0 && total == 1);

  // Missing, corrupt, stale and unreadable index must not hide valid TSV.
  JazzNotesSearch fallback;
  assert(fallback.open(fileOf(tsv)));
  assert(!fallback.indexed() && fallback.rows() == 36);
  assert(fallback.lookup("miles-davis", item));
  Memory stale = idx;
  const std::string header = firstLine(idx);
  assert(header.size() > 50);
  stale.bytes[34] = stale.bytes[34] == 'a' ? 'b' : 'a';
  assert(fallback.open(fileOf(tsv), fileOf(stale)));
  assert(!fallback.indexed() && fallback.lookup("john-coltrane", item));
  Memory unreadableIndex = idx;
  unreadableIndex.failRead = true;
  assert(fallback.open(fileOf(tsv), fileOf(unreadableIndex)));
  assert(!fallback.indexed() && fallback.lookup("miles-davis", item));
  Memory badOffset = idx;
  std::string bad = s(idx);
  size_t row = bad.find('\n');
  assert(row != std::string::npos);
  size_t tab = bad.find('\t', row + 1);
  assert(tab != std::string::npos);
  bad[tab + 1] = '9'; // Points into an unrelated position.
  badOffset = fromString(bad);
  assert(fallback.open(fileOf(tsv), fileOf(badOffset)));
  assert(!fallback.indexed() && fallback.lookup("miles-davis", item));

  // Hard reject corrupt/truncated/unreadable data and >50 rows.
  Memory corrupt = tsv;
  corrupt.bytes.pop_back(); // No final LF.
  assert(!fallback.open(fileOf(corrupt), fileOf(idx)));
  Memory unreadable = tsv;
  unreadable.failRead = true;
  assert(!fallback.open(fileOf(unreadable), fileOf(idx)));
  std::string rowText = firstLine(tsv);
  assert(!rowText.empty());
  Memory over = fromString(std::string());
  std::string generated;
  for (int i = 0; i < 50; ++i) {
    char key[24];
    std::snprintf(key, sizeof(key), "synthetic-%02d", i);
    std::string copy = rowText;
    copy.replace(0, copy.find('\t'), key);
    generated += copy;
  }
  over = fromString(generated);
  assert(fallback.open(fileOf(over)));  // Exact row limit (50).
  assert(fallback.rows() == 50);
  assert(fallback.lookup("synthetic-49", item));
  generated += rowText;
  over = fromString(generated);
  assert(!fallback.open(fileOf(over))); // 51 rows forbidden.

  // A broken TSV is rejected, not partly exposed as a trusted corpus.
  std::string malformed = s(tsv);
  const size_t separator = malformed.find('\t');
  assert(separator != std::string::npos);
  malformed[separator] = 'x';
  Memory malformedFile = fromString(malformed);
  assert(!fallback.open(fileOf(malformedFile)));
  std::string duplicate = firstLine(tsv) + firstLine(tsv);
  Memory duplicateFile = fromString(duplicate);
  assert(!fallback.open(fileOf(duplicateFile)));
  std::string withControl = firstLine(tsv);
  withControl[5] = '\x7f';
  Memory controlFile = fromString(withControl);
  assert(!fallback.open(fileOf(controlFile)));

  std::puts("JazzNotesSearch: actual 36 indexed, fallback, bounded search, 50/51 and corruption PASS");
  return 0;
}

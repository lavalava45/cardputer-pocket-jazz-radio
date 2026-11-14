#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/JazzEncyclopedia.h"
#include "../PocketJazzRadio/JazzFacts.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

struct Store {
  std::string contents;
  bool fail = false;
  size_t reads = 0;
};
static bool get(void* ctx, size_t offset, uint8_t* dest, size_t n) {
  Store& s = *static_cast<Store*>(ctx);
  ++s.reads;
  if (s.fail || offset > s.contents.size() || n > s.contents.size()-offset) return false;
  memcpy(dest, s.contents.data()+offset, n);
  return true;
}
static pocketjazz::NotesFile file(Store& s) {
  return {&s, s.contents.size(), get};
}
int main(int argc, char** argv) {
  using namespace pocketjazz;
  assert(argc == 3);
  static_assert(sizeof(JazzEncyclopedia) < 128, "No full encyclopedia in RAM");
  static_assert(sizeof(JazzFacts) < 128, "No full fact corpus in RAM");
  Store data;
  for (size_t i = 0; i < 800; ++i) {
    char key[32], name[64];
    snprintf(key, sizeof(key), "jazz-artist-%04u", unsigned(i));
    snprintf(name, sizeof(name), "Jazz Artist %04u", unsigned(i));
    data.contents += std::string(key) + "\t" + name +
         "\tJazz musician from a cited source with a factual biography." +
         "\t-\thttps://www.wikidata.org/wiki/Q123\n";
  }
  JazzEncyclopedia db;
  assert(db.open(file(data)));
  JazzBiography rows[3] = {};
  size_t stored=0,total=0;
  assert(db.search("Jazz Artist", rows, 3, 0, stored, total));
  assert(total == 800 && stored == 3);
  assert(strcmp(rows[0].key, "jazz-artist-0000") == 0);
  assert(db.search("Jazz Artist", rows, 3, 3, stored, total));
  assert(total == 800 && stored == 3);
  assert(strcmp(rows[0].key, "jazz-artist-0003") == 0);
  assert(db.search("Jazz Artist", rows, 3, 798, stored, total));
  assert(total == 800 && stored == 2);
  assert(db.search("artist-0677", rows, 3, 0, stored, total));
  assert(stored == 1 && strcmp(rows[0].key, "jazz-artist-0677") == 0);
  assert(db.find("jazz-artist-0677", rows[0]));
  assert(strcmp(rows[0].name, "Jazz Artist 0677") == 0);
  assert(!db.find("../bad", rows[0]));
  assert(!db.search("\x01", rows, 3, 0, stored, total));
  data.fail = true;
  assert(!db.search("artist", rows, 3, 0, stored, total));
  data.fail = false;
  Store malformed = {data.contents, false, 0};
  malformed.contents.pop_back();
  assert(db.open(file(malformed)));
  assert(!db.search("artist", rows, 3, 0, stored, total));
  malformed.contents = "bad key\tUnknown\tstuff\t-\thttps://en.wikipedia.org/wiki/Foo\n";
  assert(db.open(file(malformed)));
  assert(!db.search("unknown", rows, 3, 0, stored, total));
  assert(db.open(file(data)));

  Store facts;
  for (size_t i=0; i<210; ++i) {
    facts.contents += "FACT\tA sourced jazz-related detail from a historical record.\t";
    facts.contents += "Subject\thttps://www.wikidata.org/wiki/Q100\n";
  }
  JazzFacts factDb;
  assert(factDb.open(file(facts)) && factDb.size() == 210);
  JazzFact fact = {};
  assert(factDb.at(209, fact));
  assert(!strcmp(fact.type, "FACT"));
  assert(factDb.choose(210, fact));
  assert(!factDb.at(210, fact));
  Store interleaved = {
      "QUOTE\tFirst quote\tA\thttps://en.wikiquote.org/wiki/A\n"
      "FACT\tA fact\tSubject\thttps://en.wikipedia.org/wiki/Jazz\n"
      "QUOTE\tSecond quote\tB\thttps://en.wikiquote.org/wiki/B\n"
      "QUOTE\tThird quote\tC\thttps://en.wikiquote.org/wiki/C\n"
      "FACT\tAnother fact\tSubject\thttps://en.wikipedia.org/wiki/Jazz\n"};
  assert(factDb.open(file(interleaved)));
  assert(factDb.quoteCount() == 3 && factDb.size() == 5);
  size_t absolute = 999;
  assert(factDb.quoteAt(0, fact, absolute) && absolute == 0);
  assert(!strcmp(fact.text, "First quote"));
  assert(factDb.quoteAt(1, fact, absolute) && absolute == 2);
  assert(!strcmp(fact.text, "Second quote"));
  assert(factDb.quoteAt(2, fact, absolute) && absolute == 3);
  assert(!strcmp(fact.text, "Third quote"));
  assert(!factDb.quoteAt(3, fact, absolute));
  Store badFact = {"QUOTE\tWrong\tAlice\thttp://nonsecure\n"};
  assert(!factDb.open(file(badFact)));

  // Real, sourced TSVs, not just synthetic fixtures.
  std::ifstream encyclopediaFile(argv[1], std::ios::binary);
  std::ifstream factFile(argv[2], std::ios::binary);
  assert(encyclopediaFile.good() && factFile.good());
  Store sourced = {};
  Store sourcedFacts = {};
  sourced.contents.assign(std::istreambuf_iterator<char>(encyclopediaFile),
                          std::istreambuf_iterator<char>());
  sourcedFacts.contents.assign(std::istreambuf_iterator<char>(factFile),
                               std::istreambuf_iterator<char>());
  assert(db.open(file(sourced)));
  assert(db.search("jazz", rows, 3, 0, stored, total));
  assert(total >= 360);
  assert(db.find("miles-davis", rows[0]));
  assert(!strcmp(rows[0].album, "Kind of Blue"));
  assert(factDb.open(file(sourcedFacts)) && factDb.size() >= 150);
  assert(factDb.quoteCount() >= 25);
  for (size_t i = 0; i < factDb.size(); ++i) {
    assert(factDb.at(i, fact));
    assert(!strncmp(fact.source, "https://", 8));
  }
  return 0;
}

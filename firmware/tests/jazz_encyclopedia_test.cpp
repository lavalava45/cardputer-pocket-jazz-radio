#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../PocketJazzRadio/JazzEncyclopedia.h"
#include "../PocketJazzRadio/JazzFacts.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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
static uint32_t testCrc32(const std::string& content) {
  uint32_t crc = 0xffffffffu;
  for (unsigned char value : content) {
    crc ^= value;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xffffffffu;
}
static std::string indexV2(const std::string& tsv, const std::string& rows,
                           unsigned count) {
  char header[128]{};
  snprintf(header, sizeof(header), "#PJ-JAZZ-IDX\t2\t%zu\t%u\t%08lx\t%08lx\n",
           tsv.size(), count,
           (unsigned long)testCrc32(tsv), (unsigned long)testCrc32(rows));
  return std::string(header) + rows;
}
int main(int argc, char** argv) {
  using namespace pocketjazz;
  assert(argc == 3);
  static_assert(sizeof(JazzEncyclopedia) < 128, "No full encyclopedia in RAM");
  static_assert(sizeof(JazzFacts) < 128, "No full fact corpus in RAM");
  Store data;
  Store indexed;
  std::string indexRows;
  for (size_t i = 0; i < 800; ++i) {
    char key[32], name[64];
    snprintf(key, sizeof(key), "jazz-artist-%04u", unsigned(i));
    snprintf(name, sizeof(name), "Jazz Artist %04u", unsigned(i));
    const size_t start = data.contents.size();
    data.contents += std::string(key) + "\t" + name +
         "\tJazz musician from a cited source with a factual biography." +
         "\t-\thttps://www.wikidata.org/wiki/Q123\n";
    indexRows += std::string(key) + "\t" + std::to_string(start) + "\n";
  }
  indexed.contents = indexV2(data.contents, indexRows, 800);
  JazzEncyclopedia db;
  assert(db.open(file(data)));
  assert(db.verifyIndex(file(indexed)));
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
  data.reads = indexed.reads = 0;
  assert(db.find("jazz-artist-0677", rows[0], file(indexed)));
  assert(strcmp(rows[0].name, "Jazz Artist 0677") == 0);
  assert(data.reads < 15);  // The 800-row corpus was not streamed.
  for (size_t i = 0; i < 800; ++i) {
    char target[32];
    snprintf(target, sizeof(target), "jazz-artist-%04u", unsigned(i));
    data.reads = 0;
    assert(db.find(target, rows[0], file(indexed)));
    assert(strcmp(rows[0].key, target) == 0);
    assert(data.reads < 15);
  }
  indexed.contents.replace(0, 12, "#BAD-JAZZ-IDX");
  assert(db.find("jazz-artist-0677", rows[0], file(indexed)));
  indexed.contents = indexV2(data.contents, indexRows, 800);
  assert(!db.find("jazz-artist-9000", rows[0], file(indexed)));
  data.reads = 0;
  assert(!db.find("jazz-artist-0400-extra", rows[0], file(indexed), true));
  assert(data.reads == 0); // a verified index miss must never stream the TSV
  // Same-sized stale data and index MUST invalidate trusted negative lookup.
  std::string savedTsv = data.contents;
  data.contents[90] = data.contents[90] == 'c' ? 'd' : 'c';
  assert(!db.verifyIndex(file(indexed)));
  data.contents = savedTsv;
  std::string savedIndex = indexed.contents;
  indexed.contents[indexed.contents.size()-5] = '7';
  assert(!db.verifyIndex(file(indexed)));
  indexed.contents = savedIndex;
  assert(db.verifyIndex(file(indexed)));

  // Adversarial but correctly re-checksummed *structural* index mistakes:
  // a buggy generator might emit valid CRCs yet omit, reorder, or mispoint
  // entries. Only exact sorted, complete key/offset mapping may be trusted.
  const size_t lastEntry = indexRows.find("jazz-artist-0799\t");
  assert(lastEntry != std::string::npos);
  Store missing = {indexV2(data.contents, indexRows.substr(0, lastEntry), 799)};
  assert(!db.verifyIndex(file(missing)));
  data.reads = 0;
  assert(db.find("jazz-artist-0799", rows[0], file(missing),
                 db.verifyIndex(file(missing))));
  assert(!strcmp(rows[0].key, "jazz-artist-0799"));
  assert(data.reads >= 10); // untrusted miss is resolved by full TSV scan

  std::string swappedRows = indexRows;
  const size_t firstEnd = swappedRows.find('\n') + 1;
  const size_t secondEnd = swappedRows.find('\n', firstEnd) + 1;
  assert(firstEnd > 0 && secondEnd > firstEnd);
  swappedRows.replace(0, secondEnd,
                      indexRows.substr(firstEnd, secondEnd - firstEnd) +
                      indexRows.substr(0, firstEnd));
  Store reversed = {indexV2(data.contents, swappedRows, 800)};
  assert(!db.verifyIndex(file(reversed)));

  std::string wrongRows = indexRows;
  const size_t firstTab = wrongRows.find('\t');
  assert(firstTab > 0);
  wrongRows[firstTab - 1] = '9';
  Store badKey = {indexV2(data.contents, wrongRows, 800)};
  assert(!db.verifyIndex(file(badKey)));

  Store longHeader = {std::string("#PJ-JAZZ-IDX\t2\t") +
                      std::string(1024, '9') + "\t800\t12345678\t12345678\n" +
                      indexRows};
  assert(!db.verifyIndex(file(longHeader)));
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
  assert(factDb.findSubject("Subject", fact, absolute) && absolute == 1);
  assert(!strcmp(fact.type, "FACT") && !strcmp(fact.text, "A fact"));
  assert(!factDb.findSubject("Not a real topic", fact, absolute));
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
  std::string indexPath = argv[1];
  assert(indexPath.size() > 4 && indexPath.substr(indexPath.size()-4) == ".tsv");
  indexPath.replace(indexPath.size()-4, 4, ".idx");
  std::ifstream actualIndex(indexPath, std::ios::binary);
  assert(actualIndex.good());
  Store actualIndexed;
  actualIndexed.contents.assign(std::istreambuf_iterator<char>(actualIndex),
                                std::istreambuf_iterator<char>());
  assert(db.verifyIndex(file(actualIndexed)));
  std::vector<std::string> keys;
  size_t lineStart = actualIndexed.contents.find('\n') + 1;
  while (lineStart < actualIndexed.contents.size()) {
    const size_t tab = actualIndexed.contents.find('\t', lineStart);
    const size_t eol = actualIndexed.contents.find('\n', tab);
    assert(tab != std::string::npos && eol != std::string::npos);
    keys.push_back(actualIndexed.contents.substr(lineStart, tab-lineStart));
    lineStart = eol + 1;
  }
  assert(keys.size() == 1498);
  for (size_t i = 0; i < keys.size(); ++i) {
    sourced.reads = actualIndexed.reads = 0;
    assert(db.find(keys[i].c_str(), rows[0], file(actualIndexed), true));
    assert(keys[i] == rows[0].key);
    assert(sourced.reads <= 10); // one boundary byte + <=8 bounded 96-byte row reads
    // Between each adjacent pair, an unknown artist must be an indexed miss.
    if (i + 1 < keys.size()) {
      const std::string unknown = keys[i] + "-zzz";
      if (unknown < keys[i + 1]) {
        sourced.reads = 0;
        assert(!db.find(unknown.c_str(), rows[0], file(actualIndexed), true));
        assert(sourced.reads == 0);
      }
    }
  }
  sourced.reads = 0;
  assert(!db.find("zzzzzzzzzzzzzz", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave6 must be reachable through the *production* TSV + verified SD index,
  // not merely present in editorial JSON. Check exact provenance and prose.
  assert(db.find("margie-hyams", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Shearing"));
  assert(strstr(rows[0].source, "allmusic.com"));
  assert(db.find("glauco-venier", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Miniatures"));
  assert(strstr(rows[0].source, "ecmrecords.com"));
  assert(db.find("sibongile-khumalo", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Ancient Evenings"));
  assert(db.find("snooky-young", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Lunceford"));
  assert(db.search("Charly Antolini", rows, 3, 0, stored, total));
  assert(stored >= 1 && strcmp(rows[0].key, "charly-antolini") == 0);
  sourced.reads = 0;
  assert(!db.find("margie-hyams-trio", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave7 acceptance through the actual indexed, bounded production reader.
  assert(db.find("attila-zoller", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Gypsy Cry"));
  assert(strstr(rows[0].source, "vtjazz.org"));
  assert(db.find("boyd-raeburn", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "bebop"));
  assert(strstr(rows[0].source, "sova.si.edu"));
  assert(db.find("irene-aebi", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Steve Lacy"));
  assert(db.find("wolfgang-puschnig", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "SamulNori"));
  assert(db.search("Chico O'Farrill", rows, 3, 0, stored, total));
  assert(stored >= 1 && strcmp(rows[0].key, "chico-o-farrill") == 0);
  sourced.reads = 0;
  assert(!db.find("attila-zoller-quartet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave8 source-verified notes must survive the full production TSV/SD
  // index and actual C++ lookup, not merely staging JSON existence.
  assert(db.find("alice-babs", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Second Sacred Concert"));
  assert(strstr(rows[0].source, "bso.org"));
  assert(db.find("bola-sete", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Mambossa"));
  assert(db.find("nicole-mitchell", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Dresser"));
  assert(db.find("ralph-towner", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "At First Light"));
  assert(db.find("omer-simeon", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Jelly Roll Morton"));
  assert(db.search("Alice Babs", rows, 3, 0, stored, total));
  assert(stored >= 1 && strcmp(rows[0].key, "alice-babs") == 0);
  sourced.reads = 0;
  assert(!db.find("alice-babs-quartet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave9: real production indexed data and exact C++ search, not staging.
  assert(db.find("cat-anderson", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Ellington"));
  assert(strstr(rows[0].source, "si.edu"));
  assert(db.find("conte-candoli", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "West Coast"));
  assert(db.find("chris-flory", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "City Life"));
  assert(db.find("miya-masaoka", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "koto"));
  assert(db.find("stanley-cowell", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Illusion Suite"));
  assert(db.find("motohiko-hino", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Kazumi Watanabe"));
  assert(db.search("Cat Anderson", rows, 3, 0, stored, total));
  assert(stored >= 1 && strcmp(rows[0].key, "cat-anderson") == 0);
  sourced.reads = 0;
  assert(!db.find("cat-anderson-ensemble", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave10 records reached the same actual SD-bound indexed parser.
  assert(db.find("esperanza-spalding", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Chamber Music Society"));
  assert(strstr(rows[0].source, "concord.com"));
  assert(db.find("catherine-russell", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Send For Me"));
  assert(db.find("buck-hill", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Capital Hill"));
  assert(db.find("gil-coggins", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Miles Davis"));
  assert(db.find("mike-westbrook", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Love Songs"));
  assert(db.search("Catherine Russell", rows, 3, 0, stored, total));
  assert(stored >= 1 && strcmp(rows[0].key, "catherine-russell") == 0);
  sourced.reads = 0;
  assert(!db.find("buck-hill-trio", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave11 requires actual C++ lookups via the verified production SD index.
  assert(db.find("calvin-newborn", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Here Is Phineas"));
  assert(strstr(rows[0].source, "jazzdisco.org"));
  assert(db.find("cecil-scott", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Billie Holiday"));
  assert(db.find("carlo-krahmer", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "1948"));
  assert(db.find("thomas-t-dahl", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].source, "ecmrecords.com"));
  assert(db.find("mark-tokar", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Lviv"));
  assert(db.find("ann-burton", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Blue Burton"));
  sourced.reads = 0;
  assert(!db.find("calvin-newborn-quartet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  assert(db.find("nathan-davis", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Happy Girl"));
  assert(strstr(rows[0].source, "mps-music.com"));
  sourced.reads = 0;
  assert(!db.find("nathan-davis-quintet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Subsequent source-QA publication: actual production TSV and SD index.
  assert(db.find("fred-guy", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Toodle-O"));
  assert(strstr(rows[0].source, "ellingtonia.com"));
  assert(db.find("george-tucker", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Us Three"));
  assert(strstr(rows[0].source, "jazzdisco.org"));
  assert(db.find("othello-molineaux", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Opus Pocus"));
  assert(strstr(rows[0].source, "jacopastorius.com"));
  assert(db.find("stan-tracey", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Jackie Dougan"));
  sourced.reads = 0;
  assert(!db.find("george-tucker-quintet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Fresh source-verified wave12 continuation via REAL offline SD index.
  assert(db.find("al-lucas", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Boneology"));
  assert(strstr(rows[0].source, "jazzdisco.org"));
  assert(db.find("castor-mccord", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Tiger Rag"));
  assert(db.find("chris-white", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "You Had Better Listen"));
  assert(db.find("carl-morten-iversen", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Trio '87"));
  assert(db.find("peter-beets", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].source, "crisscrossjazz.com"));
  assert(db.find("bing-crosby", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Mississippi Mud"));
  assert(db.find("china-forbes", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Sympathique"));
  assert(db.find("anthony-wilson", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Live in Paris"));
  assert(db.find("nathan-davis", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "recorded Happy Girl in 1965"));
  sourced.reads = 0;
  assert(!db.find("castor-mccord-orchestra", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Research02 preserves the exact SD index contract for the newly admitted sources.
  assert(db.find("chris-pyne", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Brass Project"));
  assert(strstr(rows[0].source, "ecmrecords.com"));
  assert(db.find("bruce-ditmas", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "What If"));
  assert(strstr(rows[0].source, "arkadiarecords.com"));
  assert(db.find("bert-boeren", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "2024"));
  assert(strstr(rows[0].source, "oaprecords.com"));
  sourced.reads = 0;
  assert(!db.find("chris-pyne-quartet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
  // Wave13 source-audited musician cards through REAL indexed C++ SD IO.
  assert(db.find("bruce-barth", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Village Vanguard"));
  assert(db.find("bruce-forman", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Cow Bop"));
  assert(db.find("barney-mcall", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Groove Collective"));
  assert(db.find("chad-wackerman", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Zappa"));
  assert(db.find("axel-zwingenberger", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Big Joe Turner"));
  assert(db.find("candy-dulfer", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Saxuality"));
  assert(db.find("dave-stryker", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Groove Street"));
  assert(db.find("jacky-terrasson", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Thelonious Monk"));
  assert(db.find("holly-cole", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Dark Moon"));
  assert(db.find("champian-fulton", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Birdland"));
  assert(db.find("greg-abate", rows[0], file(actualIndexed), true));
  assert(strstr(rows[0].summary, "Ray Charles"));
  sourced.reads = 0;
  assert(!db.find("greg-abate-quartet", rows[0], file(actualIndexed), true));
  assert(sourced.reads == 0);
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

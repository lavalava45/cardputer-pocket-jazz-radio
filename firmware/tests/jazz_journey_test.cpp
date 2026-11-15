#ifdef NDEBUG
#undef NDEBUG  // Assertions must stay active in Release CTest builds.
#endif
#include "../PocketJazzRadio/JazzJourney.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static bool readFile(void* ctx, size_t at, uint8_t* out, size_t len) {
  FILE* f = static_cast<FILE*>(ctx);
  return fseek(f, long(at), SEEK_SET) == 0 && fread(out, 1, len, f) == len;
}
static pocketjazz::NotesFile open(const char* path, FILE*& f) {
  f = fopen(path, "rb");
  assert(f);
  assert(fseek(f, 0, SEEK_END) == 0);
  const long size = ftell(f);
  assert(size > 0);
  return {f, size_t(size), readFile};
}
static void catalogTest(const char* first, const char* second) {
  using namespace pocketjazz;
  FILE* lessons = nullptr;
  FILE* tips = nullptr;
  JazzJourneyCatalog data;
  assert(data.openLessons(open(first, lessons)));
  assert(data.openTips(open(second, tips)));
  JazzJourneyLesson stage{};
  for (int i = 0; i < JazzJourneyCatalog::kLessons; ++i) {
    assert(data.lesson(i, stage));
    assert(stage.id == i && stage.trail == i / 6);
    assert(stage.style == JazzJourneyCatalog::styleForLesson(i));
    assert(stage.correct >= 1 && stage.correct <= 3);
    assert(strlen(stage.source) > 10 && strlen(stage.story) > 35);
    assert(strlen(stage.artist) > 3);
  }
  assert(!data.lesson(JazzJourneyCatalog::kLessons, stage));
  JazzListeningTip tip{};
  for (int i = 0; i < JazzJourneyCatalog::kTips; ++i) {
    assert(data.tip(i, tip));
    assert(tip.id == i && tip.lesson < JazzJourneyCatalog::kLessons);
    assert(strlen(tip.prompt) >= 30);
  }
  assert(!data.tip(JazzJourneyCatalog::kTips, tip));
  fclose(lessons);
  fclose(tips);
}
static void progressTest() {
  using namespace pocketjazz;
  JazzJourneyProgress original{};
  assert(original.completedCount() == 0);
  assert(original.markLearned(0));
  assert(!original.markLearned(0));
  assert(original.markLearned(6));
  assert(original.markLearned(17));
  assert(original.markHeard(17));
  assert(original.learnedLesson(17) && original.heardLesson(17));
  assert(!original.markHeard(24));
  assert(original.styles(original.learned) ==
    (1u << 0 | 1u << 1 | 1u << 5));
  assert(original.completedCount() == 3);
  char wire[48]{};
  assert(original.serialize(wire, sizeof(wire)));
  assert(strlen(wire) == 32);
  JazzJourneyProgress restored{};
  assert(restored.parse(wire));
  assert(restored.learned == original.learned);
  assert(restored.heard == original.heard);
  assert(!restored.dirty);
  wire[5] = wire[5] == '0' ? 'F' : '0';
  JazzJourneyProgress broken{};
  assert(!broken.parse(wire));
  assert(broken.learned == 0 && broken.heard == 0);
  assert(!original.serialize(wire, 12));
}
int main(int argc, char** argv) {
  assert(argc == 3);
  static_assert(sizeof(pocketjazz::JazzJourneyLesson) < 1100,
                "Single SD lesson is too large for constrained RAM");
  static_assert(sizeof(pocketjazz::JazzJourneyCatalog) <= 64,
                "Catalog must hold file callbacks, not rows");
  catalogTest(argv[1], argv[2]);
  progressTest();
  puts("Jazz Journey 24 linked lessons, 30 listening cards, CRC progress PASS");
  return 0;
}

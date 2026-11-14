// Allocation-free Notes buffer must safely switch rich/legacy page layouts.
#include "../PocketJazzRadio/NotesResultWorkspace.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  using pocketjazz::JazzBiography;
  using pocketjazz::JazzNote;
  using pocketjazz::NotesResultWorkspace;
  static_assert(sizeof(JazzNote) == 613, "Review original legacy page size");
  static_assert(sizeof(JazzBiography) == 693, "Review rich page size");
  static_assert(sizeof(NotesResultWorkspace) == 3 * sizeof(JazzBiography),
                "Only one rich result page should be held in RAM");
  constexpr size_t oldBytes = 3 * sizeof(JazzNote) + 3 * sizeof(JazzBiography);
  constexpr size_t newBytes = sizeof(NotesResultWorkspace);
  static_assert(oldBytes - newBytes == 1839, "Regression in guaranteed RAM savings");

  NotesResultWorkspace page;
  JazzNote* legacy = page.useLegacy();
  std::strcpy(legacy[0].key, "miles-davis");
  std::strcpy(legacy[1].key, "john-coltrane");
  assert(std::strcmp(page.legacy(0).key, "miles-davis") == 0);
  assert(std::strcmp(page.legacy(1).key, "john-coltrane") == 0);
  JazzBiography* rich = page.useEncyclopedia();
  std::strcpy(rich[0].name, "Thelonious Monk");
  std::strcpy(rich[2].name, "Bill Evans");
  assert(std::strcmp(page.encyclopedia(0).name, "Thelonious Monk") == 0);
  assert(std::strcmp(page.encyclopedia(2).name, "Bill Evans") == 0);
  legacy = page.useLegacy();
  assert(legacy[0].key[0] == 0);
  assert(legacy[1].key[0] == 0);
  std::strcpy(legacy[2].description, "Legacy fallback after rich corpus");
  assert(std::strcmp(page.legacy(2).description,
                     "Legacy fallback after rich corpus") == 0);
  rich = page.useEncyclopedia();
  assert(rich[2].name[0] == 0);
  std::cout << "PASS Notes result page mode switching; "
            << oldBytes << " -> " << newBytes << " bytes ("
            << (oldBytes - newBytes) << " saved)\n";
}

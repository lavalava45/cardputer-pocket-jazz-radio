// SPDX-License-Identifier: GPL-3.0-or-later
// One allocation-free page for mutually exclusive Notes search formats.
#pragma once
#include "JazzNotesSearch.h"
#include "JazzEncyclopedia.h"
#include <new>
#include <stddef.h>
#include <type_traits>

namespace pocketjazz {
class NotesResultWorkspace {
 public:
  static constexpr size_t kPageEntries = 3;
  static_assert(std::is_trivially_destructible<JazzNote>::value,
                "Legacy Notes records must not own resources");
  static_assert(std::is_trivially_destructible<JazzBiography>::value,
                "Encyclopedia records must not own resources");

  NotesResultWorkspace() : storage_() {}
  JazzNote* useLegacy() {
    ::new (static_cast<void*>(storage_.legacy)) JazzNote[kPageEntries]{};
    return storage_.legacy;
  }
  JazzBiography* useEncyclopedia() {
    ::new (static_cast<void*>(storage_.encyclopedia))
        JazzBiography[kPageEntries]{};
    return storage_.encyclopedia;
  }
  const JazzNote& legacy(size_t i) const { return storage_.legacy[i]; }
  const JazzBiography& encyclopedia(size_t i) const {
    return storage_.encyclopedia[i];
  }

 private:
  union Page {
    JazzNote legacy[kPageEntries];
    JazzBiography encyclopedia[kPageEntries];
    Page() : legacy{} {}
    ~Page() {}
  } storage_;
};
// Saves one 1839-byte search page with no additional heap or stack storage.
static_assert(sizeof(NotesResultWorkspace) ==
                  NotesResultWorkspace::kPageEntries * sizeof(JazzBiography),
              "Notes results must occupy a single rich page");
} // namespace pocketjazz

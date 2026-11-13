// SPDX-License-Identifier: GPL-3.0-or-later
// An intent generation is updated by the UI BEFORE enqueueing its copied
// AudioCommand; the audio worker reads it without touching the network client.
#pragma once
#include <stdint.h>

namespace pocketjazz {
inline bool audioIntentSuperseded(uint32_t active, uint32_t latestRequested) {
  // Volume-only changes retain the generation and must not force a new GET.
  // A STOP/station change advances it, even while a blocking GET is running.
  return active != latestRequested;
}
}  // namespace pocketjazz

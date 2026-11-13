// SPDX-License-Identifier: MIT
// COMPILE ONLY. Verifies the actual Pocket Jazz legacy monitor hook accepts
// the reusable v0.8e SDK observer. Never call .setup(), begin(), playRaw() or
// install a second M5.Speaker release callback in this sketch.
#include <Arduino.h>
#include <M5Unified.h>
#include <atomic>
#include "../../PocketJazzRadio/AudioOutputM5Speaker.h"
#include "cardputer_audio/CardputerPcmMirror.h"

using cardputer_audio::CardputerPcmMirror;
static AudioOutputM5Speaker soleLegacySpeaker(&M5.Speaker, 0);
static CardputerPcmMirror<256,3> monitorMirror;
static std::atomic<uint32_t> existingVuFrameCount(0);
static const uint32_t kProbeRate = 44100;
static const uint32_t kProbeEpoch = 1;

// Mirrors the existing application's one-monitor contract. In actual app
// integration the existing pcmMonitor() must remain exactly once per buffer;
// calling setMonitor twice would replace VU monitoring and is forbidden.
static void legacyVuOnce(const int16_t* stereo, size_t frames) {
  if (stereo && frames) existingVuFrameCount.fetch_add(uint32_t(frames),
                                                       std::memory_order_relaxed);
}
static void compositeMonitor(void*, const int16_t* acceptedStereo, size_t frames) {
  if (!acceptedStereo || !frames) return;
  legacyVuOnce(acceptedStereo, frames);
  (void)monitorMirror.observeAccepted(acceptedStereo, frames,
                                     {kProbeRate, 2}, kProbeEpoch);
}

static_assert(sizeof(CardputerPcmMirror<256,3>) < 8192,
              "No-PSRAM monitor footprint must be bounded");

void setup() {
  // Compile/link-only test: no hardware setup or speaker ownership transfer.
  (void)monitorMirror.beginQuiescent({kProbeRate, 2}, kProbeEpoch);
  soleLegacySpeaker.setMonitor(compositeMonitor, nullptr);
  const int16_t fakeStereo[] = {100, -100, 200, -200};
  compositeMonitor(nullptr, fakeStereo, 2);
  int16_t copied[256*2] = {};
  size_t copiedFrames = 0;
  (void)monitorMirror.take(kProbeEpoch, {kProbeRate, 2}, copied, 256,
                           copiedFrames);
  soleLegacySpeaker.setMonitor(nullptr, nullptr);
  monitorMirror.stopQuiescent();
}
void loop() {}

// SPDX-License-Identifier: MIT
// Pocket Jazz: deterministic, original, intentionally quiet tuning ambience.
// These constexpr PCM clips live in flash (.rodata), not in a heap/SD buffer.
// There is never any playback/CPU work when both settings are disabled.
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace pocketjazz {
namespace tuning {

static constexpr uint32_t kRateHz = 8000;
static constexpr size_t kSamples = 8192; // 1.024 s mono; repeats via M5 speaker
static constexpr uint8_t kChannel = 1;   // channel 0 belongs to the MP3 decoder
// The previous fixed channel level 42 was reported as painfully loud on the
// physical ADV, despite the small signed 8-bit waveform. Keep the FX far
// below the music channel: this is only a faint tuning hiss, not background
// noise mixed over a playing track. Actual speaker gain is nonlinear; the
// conservative cap must still be accepted by ear on a real device.
static constexpr uint8_t kQuietChannelVolume = 8;
static constexpr uint8_t kReferenceMasterVolume = 185;
constexpr uint8_t channelVolumeForMaster(uint8_t master) {
  if (!master) return 0;
  const uint16_t level =
      (static_cast<uint16_t>(master) * kQuietChannelVolume +
       kReferenceMasterVolume - 1) / kReferenceMasterVolume;
  return static_cast<uint8_t>(level > kQuietChannelVolume
                                  ? kQuietChannelVolume : level);
}
static constexpr uint32_t kFadeMs = 90;
// Leave headroom for the still-unallocated split libmad workspace (29,192 B)
// AND for the existing TLS/Wi-Fi connection. Do not trade reliable jazz for
// optional sound effects: on a RAM-constrained session FX is skipped.
static constexpr uint32_t kMinimumFreeHeap = 64000;
static constexpr uint32_t kMinimumLargestBlock = 32768;

// A deterministic LCG avoids a runtime PRNG and makes clicks reproducible.
// Audio is signed 8-bit mono, peak <= 28/128 (-13 dBFS pre-volume), with a
// second volume reduction on the dedicated sound-effect virtual channel.
constexpr std::array<int8_t, kSamples> makeClip(bool crackle, bool hiss) {
  std::array<int8_t, kSamples> out{};
  uint32_t seed = 0xAD3C594Bu;
  for (size_t i = 0; i < kSamples; ++i) {
    seed = seed * 1664525u + 1013904223u;
    int16_t sample = hiss ? static_cast<int16_t>((seed >> 28) % 5) - 2 : 0;
    if (crackle) {
      // Three irregular short transients per loop, each decaying in 24 samples.
      constexpr size_t starts[] = {119, 2917, 6813};
      for (size_t start : starts) {
        if (i >= start && i < start + 24) {
          const int envelope = 24 - static_cast<int>(i - start);
          const int polarity = (seed & 1u) ? 1 : -1;
          sample += polarity * (envelope * 20 / 24);
        }
      }
    }
    out[i] = static_cast<int8_t>(sample);
  }
  return out;
}

// Only ROM-backed arrays: never malloc, new, SD access or random() in the
// audio owner, including during TCP/HTTPS handshakes.
inline constexpr auto kCrackleOnly = makeClip(true, false);
inline constexpr auto kHissOnly = makeClip(false, true);
inline constexpr auto kCrackleAndHiss = makeClip(true, true);

inline const int8_t* selectClip(bool crackle, bool hiss) {
  if (crackle && hiss) return kCrackleAndHiss.data();
  if (crackle) return kCrackleOnly.data();
  return hiss ? kHissOnly.data() : nullptr;
}

} // namespace tuning
} // namespace pocketjazz

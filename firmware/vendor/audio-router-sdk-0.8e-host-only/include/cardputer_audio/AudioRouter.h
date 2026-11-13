#ifndef CARDPUTER_AUDIO_AUDIO_ROUTER_H
#define CARDPUTER_AUDIO_AUDIO_ROUTER_H

#include <stddef.h>
#include <stdint.h>

namespace cardputer_audio {

// v0: interleaved signed PCM16, native little-endian ESP32/PC samples.
// One frame contains one sample per channel.
struct Format {
  uint32_t sample_rate;
  uint8_t channels;
};

enum class Output : uint8_t {
  LocalI2S = 0,
  UsbDevice = 1,
  BleBridge = 2,
  Count = 3
};

// Implemented by a transport, not by an audio-generating application.
// tryWrite MUST NOT block on hardware/network I/O, and MUST copy all accepted
// frames to transport-owned memory before returning true.
class Sink {
 public:
  virtual ~Sink() {}
  virtual bool supports(Format format) const = 0;
  virtual bool tryWrite(const int16_t *interleaved, size_t frames) = 0;
};

struct SinkStats {
  uint64_t frames_accepted;
  uint64_t frames_dropped;
};

// Counts successful output state transitions, not repeated idempotent calls.
// Observed only from the single producer/configuration task.
struct OutputTransitionStats {
  uint32_t enabled;
  uint32_t disabled;
};

// Portable fan-out core. A single producer calls write() and also configures
// attachments/enables. Changing config from another task needs external sync.
// Transport consumers implement their own bounded SPSC queues/clocking.
class AudioRouter {
 public:
  AudioRouter();

  bool begin(Format format);
  bool attach(Output output, Sink *sink);
  bool enable(Output output, bool enabled);
  bool isEnabled(Output output) const;
  Format format() const;
  SinkStats stats(Output output) const;
  OutputTransitionStats transitions(Output output) const;

  // Number of sinks that accepted the WHOLE frame block.
  // Does not convert or resample; disconnected/full sinks may drop a block.
  size_t write(const int16_t *interleaved, size_t frames);

 private:
  struct Slot {
    Sink *sink;
    bool enabled;
    SinkStats stats;
    OutputTransitionStats transitions;
  };
  static const size_t kSlots = static_cast<size_t>(Output::Count);
  static size_t index(Output output);
  Slot slots_[kSlots];
  Format format_;
  bool started_;
};

}  // namespace cardputer_audio
#endif

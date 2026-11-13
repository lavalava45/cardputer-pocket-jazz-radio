#pragma once
#include <cstdint>
class AudioOutput {
 public:
  uint32_t hertz = 44100;
  virtual ~AudioOutput() = default;
  virtual bool begin() { return true; }
  virtual bool ConsumeSample(int16_t[2]) { return true; }
  virtual void flush() {}
  virtual bool stop() { return true; }
};

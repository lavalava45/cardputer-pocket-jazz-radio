// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <AudioGeneratorMP3.h>
#include <cstdlib>
#include <new>

namespace pocketjazz {

// libmad accepts four independent caller-owned regions. Allocating the frame
// (the largest) first avoids requiring a single 29-KiB hole after HTTPS GET.
// The workspace must outlive AudioGeneratorMP3 and be released after stop().
class Mp3CodecWorkspace {
 public:
  bool allocate() {
    if (ready()) return true;
    release();
    frame_ = malloc(AudioGeneratorMP3::preAllocFrameSize());
    if (frame_) synth_ = malloc(AudioGeneratorMP3::preAllocSynthSize());
    if (synth_) stream_ = malloc(AudioGeneratorMP3::preAllocStreamSize());
    if (stream_) input_ = malloc(AudioGeneratorMP3::preAllocBuffSize());
    if (!ready()) release();
    return ready();
  }

  AudioGeneratorMP3* createDecoder() const {
    if (!ready()) return nullptr;
    return new (std::nothrow) AudioGeneratorMP3(
        input_, AudioGeneratorMP3::preAllocBuffSize(),
        stream_, AudioGeneratorMP3::preAllocStreamSize(),
        frame_, AudioGeneratorMP3::preAllocFrameSize(),
        synth_, AudioGeneratorMP3::preAllocSynthSize());
  }

  void release() {
    free(input_); input_ = nullptr;
    free(stream_); stream_ = nullptr;
    free(synth_); synth_ = nullptr;
    free(frame_); frame_ = nullptr;
  }

  bool ready() const { return input_ && stream_ && frame_ && synth_; }

 private:
  void* input_ = nullptr;
  void* stream_ = nullptr;
  void* frame_ = nullptr;
  void* synth_ = nullptr;
};

}  // namespace pocketjazz

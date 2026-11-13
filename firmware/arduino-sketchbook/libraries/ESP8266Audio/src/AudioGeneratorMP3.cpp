/*
    AudioGeneratorMP3
    Wrap libmad MP3 library to play audio

    Copyright (C) 2017  Earle F. Philhower, III

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "AudioGeneratorMP3.h"
#include "AudioMp3SyncBoundary.h"

AudioGeneratorMP3::AudioGeneratorMP3() {
    running = false;
    file = NULL;
    output = NULL;
    buff = NULL;
    synth = NULL;
    frame = NULL;
    stream = NULL;
    nsCountMax = 1152 / 32;
    madInitted = false;
}

AudioGeneratorMP3::AudioGeneratorMP3(void *space, int size): preallocateSpace(space), preallocateSize(size) {
    running = false;
    file = NULL;
    output = NULL;
    buff = NULL;
    synth = NULL;
    frame = NULL;
    stream = NULL;
    nsCountMax = 1152 / 32;
    madInitted = false;
}

AudioGeneratorMP3::AudioGeneratorMP3(void *buff, int buffSize, void *stream, int streamSize, void *frame, int frameSize, void *synth, int synthSize):
    preallocateSpace(buff), preallocateSize(buffSize),
    preallocateStreamSpace(stream), preallocateStreamSize(streamSize),
    preallocateFrameSpace(frame), preallocateFrameSize(frameSize),
    preallocateSynthSpace(synth), preallocateSynthSize(synthSize) {
    running = false;
    file = NULL;
    output = NULL;
    buff = NULL;
    synth = NULL;
    frame = NULL;
    stream = NULL;
    nsCountMax = 1152 / 32;
    madInitted = false;
}

AudioGeneratorMP3::~AudioGeneratorMP3() {
    if (!preallocateSpace) {
        free(buff);
        free(synth);
        free(frame);
        free(stream);
    }
}


bool AudioGeneratorMP3::stop() {
    frameBitrate.reset();
    if (madInitted) {
        mad_synth_finish(synth);
        mad_frame_finish(frame);
        mad_stream_finish(stream);
        madInitted = false;
    }

    if (!preallocateSpace) {
        free(buff);
        free(synth);
        free(frame);
        free(stream);
    }

    buff = NULL;
    synth = NULL;
    frame = NULL;
    stream = NULL;

    running = false;
    output->stop();
    return file->close();
}

bool AudioGeneratorMP3::isRunning() {
    return running;
}

enum mad_flow AudioGeneratorMP3::ErrorToFlow() {
    char err[64];
    char errLine[128];

    // Special case - eat "lost sync @ byte 0" as it always occurs and is not really correct....it never had sync!
    if ((lastReadPos == 0) && (stream->error == MAD_ERROR_LOSTSYNC)) {
        return MAD_FLOW_CONTINUE;
    }

    strcpy_P(err, mad_stream_errorstr(stream));
    snprintf_P(errLine, sizeof(errLine), PSTR("Decoding error '%s' at byte offset %d"),
               err, (stream->this_frame - buff) + lastReadPos);
    yield(); // Something bad happened anyway, ensure WiFi gets some time, too
    cb.st(stream->error, errLine);
    return MAD_FLOW_CONTINUE;
}

enum mad_flow AudioGeneratorMP3::Input() {
    int unused = 0;
    const bool resumedPendingSync = pendingSyncCarry != 0;

    if (stream->next_frame) {
        unused = lastBuffLen - (stream->next_frame - buff);
        if (unused < 0) {
            desync();
            unused = 0;
        } else {
            memmove(buff, stream->next_frame, unused);
        }
        stream->next_frame = NULL;
    } else if (pendingSyncCarry) {
        // A previous call yielded mid-resynchronization. carryPartialSync()
        // already moved the trailing 0xFF to buff[0]; libmad has no pointer
        // to this undecoded byte, so preserve it explicitly.
        unused = pendingSyncCarry;
    }
    pendingSyncCarry = 0;

    // A full 0x600-byte buffer cannot accept even one more byte. If libmad
    // could not decode a frame from it, retaining all those bytes during
    // a live stall would issue read(..., 0) forever until timeout. Restore
    // the original resync behaviour on this specific bounded case.
    if (unused >= buffLen) {
        desync();
        unused = 0;
        liveStallPolicy.reset();
    } else if (unused == lastBuffLen && !liveStallPolicy.waiting() &&
               !resumedPendingSync) {
        // Something wicked this way came, throw it all out and try again
        unused = 0;
    }

    bool foundHeader = false;
    do {
        // A live server can continue sending bytes without ever delivering
        // a valid MPEG frame. Leave the inner sync-search loop at a finite
        // deadline instead of monopolising the audio task forever.
        if (frameWatchdog.expired(millis())) return MAD_FLOW_STOP;
        if (!liveDecodeBudget.tryStep(millis())) {
            // Never monopolize the owner's command queue on a stream that
            // supplies limitless positive but headerless bytes. The buffer
            // contains exactly `unused` pending sync bytes at its beginning.
            pendingSyncCarry = unused;
            lastBuffLen = unused;
            workBudgetDeferred = true;
            return MAD_FLOW_IGNORE;
        }
        lastReadPos = file->getPos() - unused;
        int len = buffLen - unused;
        len = file->read(buff + unused, len);
        if (len == 0) {
            // A live HTTP/ICY read can have a transient empty TCP buffer even
            // while the connection is open. SourceBuffer may have tried a
            // direct read after underflow. A zero-byte read is not always EOF.
            if (liveStallPolicy.shouldWait(millis(), file->isOpen(),
                                           file->getSize(), file->getPos())) {
                lastBuffLen = unused;
                stream->next_frame = unused ? buff : NULL; // retain partial frame
                return MAD_FLOW_IGNORE; // return to app task, not MP3::stop()
            }
            // Real EOF or prolonged live silence: caller can close/reconnect.
            return MAD_FLOW_STOP;
        }
        if (len > 0) liveStallPolicy.progress();
        if (len < 0) {
            desync();
            return MAD_FLOW_STOP;
        }

        lastBuffLen = len + unused;
        // The sync probe examines two bytes. A full 0x600-byte decoder
        // buffer must never read buff[lastBuffLen] (one past allocation).
        const int offset = audio_mp3::findSyncHeader(buff, lastBuffLen);
        if (offset >= 0) {
            if (offset) {
                memmove(buff, buff + offset, lastBuffLen - offset);
                lastBuffLen -= offset;
            }
            foundHeader = true;
        }
        if (!foundHeader) {
            // MPEG sync may be split across two reads: ... FF | FB ...
            // Preserve one final 0xFF byte for the next iteration, without
            // retaining arbitrary junk or exceeding the fixed buffer.
            unused = audio_mp3::carryPartialSync(buff, lastBuffLen);
        } else {
            // Try and fill rest of buffer in case there was some shiftage
            // This extra refill is opportunistic, not worth delaying the
            // audio task when its per-call decode budget is exhausted.
            len = liveDecodeBudget.tryStep(millis())
                      ? file->read(buff + lastBuffLen, buffLen - lastBuffLen)
                      : 0;
            if (len < 0) {
                desync();
                return MAD_FLOW_STOP;
            }
            lastBuffLen += len;
            if (lastBuffLen < 8) {
                if (liveStallPolicy.shouldWait(millis(), file->isOpen(),
                                               file->getSize(), file->getPos())) {
                    stream->next_frame = buff;
                    return MAD_FLOW_IGNORE;
                }
                return MAD_FLOW_STOP;
            }
        }
    } while (!foundHeader);

    mad_stream_buffer(stream, buff, lastBuffLen);

    return MAD_FLOW_CONTINUE;
}

void AudioGeneratorMP3::desync() {
    audioLogger->printf_P(PSTR("MP3:desync\n"));
    if (stream) {
        stream->next_frame = nullptr;
        stream->this_frame = nullptr;
        stream->sync = 0;
    }
    lastBuffLen = 0;
    pendingSyncCarry = 0;
}

bool AudioGeneratorMP3::DecodeNextFrame() {
    if (mad_frame_decode(frame, stream) == -1) {
        ErrorToFlow(); // Always returns CONTINUE
        return false;
    }
    frameBitrate.decodedFrame(static_cast<uint32_t>(frame->header.bitrate));
    nsCountMax  = MAD_NSBSAMPLES(&frame->header);
    return true;
}

bool AudioGeneratorMP3::GetOneSample(int16_t sample[2]) {
    if (synth->pcm.samplerate != lastRate) {
        output->SetRate(synth->pcm.samplerate);
        lastRate = synth->pcm.samplerate;
    }
    if (synth->pcm.channels != lastChannels) {
        output->SetChannels(synth->pcm.channels);
        lastChannels = synth->pcm.channels;
    }

    // If we're here, we have one decoded frame and sent 0 or more samples out
    if (samplePtr < synth->pcm.length) {
        sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
        sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
        samplePtr++;
    } else {
        samplePtr = 0;

        switch (mad_synth_frame_onens(synth, frame, nsCount++)) {
        case MAD_FLOW_STOP:
        case MAD_FLOW_BREAK: audioLogger->printf_P(PSTR("msf1ns failed\n"));
            return false; // Either way we're done
        default:
            break; // Do nothing
        }
        // for IGNORE and CONTINUE, just play what we have now
        sample[AudioOutput::LEFTCHANNEL ] = synth->pcm.samples[0][samplePtr];
        sample[AudioOutput::RIGHTCHANNEL] = synth->pcm.samples[1][samplePtr];
        samplePtr++;
    }
    return true;
}


bool AudioGeneratorMP3::loop() {
    bool resumedFromWorkYield = false;
    if (!running) {
        goto done;    // Nothing to do here!
    }

    liveDecodeBudget.begin(millis());
    resumedFromWorkYield = workBudgetDeferred;
    workBudgetDeferred = false;

    // First, try and push in the stored sample.  If we can't, then punt and try later
    if (!resumedFromWorkYield && !liveStallPolicy.waiting() &&
            !output->ConsumeSample(lastSample)) {
        goto done;    // Can't send, but no error detected
    }

    // Try and stuff the buffer one sample at a time
    do {
        // Decode next frame if we're beyond the existing generated data
        if ((samplePtr >= synth->pcm.length) && (nsCount >= nsCountMax)) {
retry:
            if (frameWatchdog.expired(millis())) {
                return false; // no valid MPEG frames for the bounded deadline
            }
            if (!liveDecodeBudget.tryStep(millis())) {
                workBudgetDeferred = true;
                goto done;
            }
            enum mad_flow inputFlow = Input();
            if (inputFlow == MAD_FLOW_STOP) {
                return false;
            }
            if (inputFlow == MAD_FLOW_IGNORE) {
                goto done; // still running; don't consume stale sample or busy-spin
            }

            if (!DecodeNextFrame()) {
                if (stream->error == MAD_ERROR_BUFLEN &&
                        liveStallPolicy.shouldWait(millis(), file->isOpen(),
                                                   file->getSize(), file->getPos())) {
                    // Partial MP3 frame, not necessarily a corrupt frame.
                    // On next Input() retain these bytes and append new ones.
                    stream->next_frame = buff;
                    goto done;
                }
                if (stream->error == MAD_ERROR_BUFLEN) {
                    // randomly seeking can lead to endless
                    // and unrecoverable "MAD_ERROR_BUFLEN" loop
                    audioLogger->printf_P(PSTR("MP3:ERROR_BUFLEN %d\n"), unrecoverable);
                    if (++unrecoverable >= 3) {
                        unrecoverable = 0;
                        stop();
                        return running;
                    }
                } else {
                    unrecoverable = 0;
                }
                goto retry;
            }
            samplePtr = 9999;
            nsCount = 0;
            frameWatchdog.decodedFrame(millis());
        }

        if (!GetOneSample(lastSample)) {
            audioLogger->printf_P(PSTR("G1S failed\n"));
            running = false;
            goto done;
        }
        if (lastChannels == 1) {
            lastSample[1] = lastSample[0];
        }
    } while (running && output->ConsumeSample(lastSample));

done:
    file->loop();
    output->loop();

    return running;
}



bool AudioGeneratorMP3::begin(AudioFileSource *source, AudioOutput *output) {
    if (!source) {
        return false;
    }
    file = source;
    if (!output) {
        return false;
    }
    this->output = output;
    if (!file->isOpen()) {
        audioLogger->printf_P(PSTR("MP3 source file not open\n"));
        return false; // Error
    }

    // Reset error count from previous file
    unrecoverable = 0;
    liveStallPolicy.reset();
    frameBitrate.reset();
    frameWatchdog.start(millis());
    pendingSyncCarry = 0;
    workBudgetDeferred = false;

    output->SetChannels(2);

    if (!output->begin()) {
        return false;
    }

    // Where we are in generating one frame's data, set to invalid so we will run loop on first getsample()
    samplePtr = 9999;
    nsCount = 9999;
    lastRate = 0;
    lastChannels = 0;
    lastReadPos = 0;
    lastBuffLen = 0;

    // Allocate all large memory chunks
    if (preallocateStreamSize + preallocateFrameSize + preallocateSynthSize) {
        if (preallocateSize >= preAllocBuffSize() &&
                preallocateStreamSize >= preAllocStreamSize() &&
                preallocateFrameSize >= preAllocFrameSize() &&
                preallocateSynthSize >= preAllocSynthSize()) {
            buff = reinterpret_cast<unsigned char *>(preallocateSpace);
            stream = reinterpret_cast<struct mad_stream *>(preallocateStreamSpace);
            frame = reinterpret_cast<struct mad_frame *>(preallocateFrameSpace);
            synth = reinterpret_cast<struct mad_synth *>(preallocateSynthSpace);
        } else {
            output->stop();
            audioLogger->printf_P("OOM error in MP3:  Want %d/%d/%d/%d bytes, have %d/%d/%d/%d bytes preallocated.\n",
                                  preAllocBuffSize(), preAllocStreamSize(), preAllocFrameSize(), preAllocSynthSize(),
                                  preallocateSize, preallocateStreamSize, preallocateFrameSize, preallocateSynthSize);
            return false;
        }
    } else if (preallocateSpace) {
        uint8_t *p = reinterpret_cast<uint8_t *>(preallocateSpace);
        buff = reinterpret_cast<unsigned char *>(p);
        p += preAllocBuffSize();
        stream = reinterpret_cast<struct mad_stream *>(p);
        p += preAllocStreamSize();
        frame = reinterpret_cast<struct mad_frame *>(p);
        p += preAllocFrameSize();
        synth = reinterpret_cast<struct mad_synth *>(p);
        p += preAllocSynthSize();
        int neededBytes = p - reinterpret_cast<uint8_t *>(preallocateSpace);
        if (neededBytes > preallocateSize) {
            output->stop();
            audioLogger->printf_P("OOM error in MP3:  Want %d bytes, have %d bytes preallocated.\n", neededBytes, preallocateSize);
            return false;
        }
    } else {
        buff = reinterpret_cast<unsigned char *>(malloc(buffLen));
        stream = reinterpret_cast<struct mad_stream *>(malloc(sizeof(struct mad_stream)));
        frame = reinterpret_cast<struct mad_frame *>(malloc(sizeof(struct mad_frame)));
        synth = reinterpret_cast<struct mad_synth *>(malloc(sizeof(struct mad_synth)));
        if (!buff || !stream || !frame || !synth) {
            free(buff);
            free(stream);
            free(frame);
            free(synth);
            buff = NULL;
            stream = NULL;
            frame = NULL;
            synth = NULL;

            output->stop();
            audioLogger->printf_P("OOM error in MP3\n");
            return false;
        }
    }

    mad_stream_init(stream);
    mad_frame_init(frame);
    mad_synth_init(synth);
    synth->pcm.length = 0;
    mad_stream_options(stream, 0); // TODO - add options support
    madInitted = true;

    running = true;
    return true;
}

// The following are helper routines for use in libmad to check stack/heap free
// and to determine if there's enough stack space to allocate some blocks there
// instead of precious heap.

#undef stackenter
extern "C" {
#ifdef ESP32
    //TODO - add ESP32 checks
    void stackenter(const char *s, const char *t, int i) {
    }
    int stackfree() {
        return 8192;
    }
#elif defined(ESP8266) && !defined(CORE_MOCK)
#include <cont.h>
    extern cont_t g_cont;

    void stackenter(const char *s, const char *t, int i) {
        (void) t;
        (void) i;
        register uint32_t *sp asm("a1");
        int freestack = 4 * (sp - g_cont.stack);
        int freeheap = ESP.getFreeHeap();
        if ((freestack < 512) || (freeheap < 5120)) {
            static int laststack, lastheap;
            if (laststack != freestack || lastheap != freeheap) {
                audioLogger->printf_P(PSTR("%s: FREESTACK=%d, FREEHEAP=%d\n"), s, /*t, i,*/ freestack, /*cont_get_free_stack(&g_cont),*/ freeheap);
            }
            if (freestack < 256) {
                audioLogger->printf_P(PSTR("out of stack!\n"));
            }
            if (freeheap < 1024) {
                audioLogger->printf_P(PSTR("out of heap!\n"));
            }
            Serial.flush();
            laststack = freestack;
            lastheap = freeheap;
        }
    }

    int stackfree() {
        register uint32_t *sp asm("a1");
        int freestack = 4 * (sp - g_cont.stack);
        return freestack;
    }
#else
    void stackenter(const char *s, const char *t, int i) {
        (void) s;
        (void) t;
        (void) i;
    }
    int stackfree() {
        return 8192;
    }
#endif
}

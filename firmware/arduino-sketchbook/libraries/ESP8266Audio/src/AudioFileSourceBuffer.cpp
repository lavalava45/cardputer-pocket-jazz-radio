/*
    AudioFileSourceBuffer
    Double-buffered file source using system RAM

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

#include <Arduino.h>
#include "AudioFileSourceBuffer.h"
#include "AudioSourcePullPolicy.h"

#pragma GCC optimize ("O3")

AudioFileSourceBuffer::AudioFileSourceBuffer(AudioFileSource *source, uint32_t buffSizeBytes) {
    buffSize = buffSizeBytes;
    buffer = (uint8_t*)malloc(sizeof(uint8_t) * buffSize);
    if (!buffer) {
        audioLogger->printf_P(PSTR("Unable to allocate AudioFileSourceBuffer::buffer[]\n"));
    }
    deallocateBuffer = true;
    writePtr = 0;
    readPtr = 0;
    src = source;
    length = 0;
    filled = false;
}

AudioFileSourceBuffer::AudioFileSourceBuffer(AudioFileSource *source, void *inBuff, uint32_t buffSizeBytes) {
    buffSize = buffSizeBytes;
    buffer = (uint8_t*)inBuff;
    deallocateBuffer = false;
    writePtr = 0;
    readPtr = 0;
    src = source;
    length = 0;
    filled = false;
}

AudioFileSourceBuffer::~AudioFileSourceBuffer() {
    if (deallocateBuffer) {
        free(buffer);
    }
    buffer = NULL;
}

bool AudioFileSourceBuffer::seek(int32_t pos, int dir) {
    shortageEpisode.reset();
    if (dir == SEEK_CUR && (readPtr + pos) < length) {
        readPtr += pos;
        return true;
    } else {
        // Invalidate
        readPtr = 0;
        writePtr = 0;
        length = 0;
        return src->seek(pos, dir);
    }
}

bool AudioFileSourceBuffer::close() {
    shortageEpisode.reset();
    if (deallocateBuffer) {
        free(buffer);
    }
    buffer = NULL;
    return src->close();
}

bool AudioFileSourceBuffer::isOpen() {
    return src->isOpen();
}

uint32_t AudioFileSourceBuffer::getSize() {
    return src->getSize();
}

uint32_t AudioFileSourceBuffer::getPos() {
    return src->getPos();
}

uint32_t AudioFileSourceBuffer::getFillLevel() {
    return length;
}

uint32_t AudioFileSourceBuffer::pullSource(void* data, uint32_t len,
                                           bool cooperative) {
    if (!sourcePollMonitor)
        return audio_buffer::pull(src, data, len, cooperative);
    const uint32_t started = micros();
    const uint32_t got = audio_buffer::pull(src, data, len, cooperative);
    sourcePollMonitor(sourcePollMonitorArg, micros() - started);
    return got;
}

uint32_t AudioFileSourceBuffer::read(void *data, uint32_t len) {
    if (!buffer) {
        return pullSource(data, len, cooperativeSourceRead);
    }

    const uint32_t requested = len;
    uint32_t bytes = 0;
    if (!filled) {
        // Fill up completely before returning any data at all
        cb.st(STATUS_FILLING, PSTR("Refilling buffer"));
        length = pullSource(buffer, buffSize, cooperativeSourceRead);
        writePtr = length % buffSize;
        filled = true;
    }

    // Pull from buffer until we've got none left or we've satisfied the request
    uint8_t *ptr = reinterpret_cast<uint8_t*>(data);
    uint32_t toReadFromBuffer = (len < length) ? len : length;
    if ((toReadFromBuffer > 0) && (readPtr >= writePtr)) {
        uint32_t toReadToEnd = (toReadFromBuffer < (uint32_t)(buffSize - readPtr)) ? toReadFromBuffer : (buffSize - readPtr);
        memcpy(ptr, &buffer[readPtr], toReadToEnd);
        readPtr = (readPtr + toReadToEnd) % buffSize;
        len -= toReadToEnd;
        length -= toReadToEnd;
        ptr += toReadToEnd;
        bytes += toReadToEnd;
        toReadFromBuffer -= toReadToEnd;
    }
    if (toReadFromBuffer > 0) { // We know RP < WP at this point
        memcpy(ptr, &buffer[readPtr], toReadFromBuffer);
        readPtr = (readPtr + toReadFromBuffer) % buffSize;
        len -= toReadFromBuffer;
        length -= toReadFromBuffer;
        ptr += toReadFromBuffer;
        bytes += toReadFromBuffer;
        toReadFromBuffer -= toReadFromBuffer;
    }

    if (len) {
        // A source shortage is NOT synonymous with a user-audible underrun.
        // The old code emitted UNDERFLOW even when the direct read returned
        // all missing bytes. Pocket Jazz opts into readNonBlock here so a
        // 500-ms HTTPS stall cannot hold its tiny ~35-ms PCM speaker queue.
        const uint32_t missing = len;
        const uint32_t delivered = pullSource(ptr, missing, cooperativeSourceRead);
        bytes += delivered;
        // We're out of buffered data, need to force a complete refill.  Thanks, @armSeb
        readPtr = 0;
        writePtr = 0;
        length = 0;
        filled = false;
    }

    fill();
    // An empty live socket is checked often by the same single audio task.
    // Emit one short-read episode, not hundreds of events for the SAME gap.
    // The episode rearms only after a full decoder request was delivered.
    if (shortageEpisode.onRead(requested, bytes))
        cb.st(STATUS_UNDERFLOW, PSTR("Audio bytes genuinely unavailable"));

    return bytes;
}

void AudioFileSourceBuffer::fill() {
    if (!buffer) {
        return;
    }

    if (length < buffSize) {
        // Now try and opportunistically fill the buffer
        if (readPtr > writePtr) {
            if (readPtr == writePtr + 1) {
                return;
            }
            uint32_t bytesAvailMid = readPtr - writePtr - 1;
            int cnt = pullSource(&buffer[writePtr], bytesAvailMid, true);
            length += cnt;
            writePtr = (writePtr + cnt) % buffSize;
            filled = audio_buffer::keepOpportunisticBytes(cooperativeSourceRead, filled, cnt);
            return;
        }

        if (buffSize > writePtr) {
            uint32_t bytesAvailEnd = buffSize - writePtr;
            int cnt = pullSource(&buffer[writePtr], bytesAvailEnd, true);
            length += cnt;
            writePtr = (writePtr + cnt) % buffSize;
            filled = audio_buffer::keepOpportunisticBytes(cooperativeSourceRead, filled, cnt);
            if (cnt != (int)bytesAvailEnd) {
                return;
            }
        }

        if (readPtr > 1) {
            uint32_t bytesAvailStart = readPtr - 1;
            int cnt = pullSource(&buffer[writePtr], bytesAvailStart, true);
            length += cnt;
            writePtr = (writePtr + cnt) % buffSize;
            filled = audio_buffer::keepOpportunisticBytes(cooperativeSourceRead, filled, cnt);
        }
    }
}



bool AudioFileSourceBuffer::loop() {
    if (!src->loop()) {
        return false;
    }
    fill();
    return true;
}


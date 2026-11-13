/*
    AudioFileSourceHTTPStream
    Connect to a HTTP based streaming service

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

#pragma once

#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#else
#include <HTTPClient.h>
#endif

#include "AudioFileSourceHTTPStream.h"
#include "AudioICYIncremental.h"
#include "../../../../PocketJazzRadio/HttpChunkedBody.h"
#include "../../../../PocketJazzRadio/HttpBodyAudioSize.h"

class AudioFileSourceICYStream : public AudioFileSourceHTTPStream {
public:
    AudioFileSourceICYStream();
    AudioFileSourceICYStream(const char *url);
    virtual ~AudioFileSourceICYStream() override;

    virtual bool open(const char *url) override;
    virtual bool close() override;
    virtual uint32_t getSize() override;
    // Valid only immediately after a failed HTTP open that received an
    // explicit, bounded HTTP -> HTTPS Location. Caller owns the verified TLS
    // source; this plain NetworkClient NEVER attempts a TLS connection.
    const char* httpsUpgrade() const { return httpsUpgrade_; }

private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock) override;
    uint32_t readBody(uint8_t *data, uint32_t len);
    void reportDisconnected();
    audio_icy::IncrementalMetadata metadata;
    pocketjazz::HttpChunkedBody chunkDecoder;
    bool chunked = false;
    bool disconnectedReported = false;
    uint32_t bodyBytesConsumed_ = 0;
    char httpsUpgrade_[192]{};
};

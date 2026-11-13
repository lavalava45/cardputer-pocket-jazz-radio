/*
    AudioFileSourceICYStream — Streaming Shoutcast ICY source.
    Copyright (C) 2017 Earle F. Philhower, III
    Pocket Jazz Radio 2026: bounded, nonwaiting incremental ICY/HTTP body.
    SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "AudioFileSourceICYStream.h"
#include "../../../../PocketJazzRadio/SafeHttpsUpgrade.h"
#include <algorithm>
#include <string.h>

AudioFileSourceICYStream::AudioFileSourceICYStream() {
    pos = 0;
    size = -1;
    reconnectTries = 0;
    reconnectDelayMs = 0;
    saveURL[0] = 0;
}

AudioFileSourceICYStream::AudioFileSourceICYStream(const char *url)
    : AudioFileSourceICYStream() { open(url); }

bool AudioFileSourceICYStream::open(const char *url) {
    close();
    pos = 0;
    size = -1;
    disconnectedReported = false;
    bodyBytesConsumed_ = 0;
    // Plain NetworkClient must never accidentally carry HTTPS; strict HTTPS
    // uses the separate verified SecureICYSource on Cardputer.
    if (!url || strncmp(url, "http://", 7) != 0 || !http.begin(client, url)) {
        cb.st(STATUS_HTTPFAIL, PSTR("Invalid plain HTTP stream"));
        return false;
    }
    static const char *headers[] = {
        "icy-metaint", "icy-name", "icy-genre", "icy-br", "Transfer-Encoding"
    };
    http.addHeader("Icy-MetaData", "1");
    http.collectHeaders(headers, 5);
    http.setReuse(false);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        // Arduino-ESP32 HTTPClient::setURL() refuses a redirect to a different
        // protocol. Do not retry that Location through plain NetworkClient;
        // expose it for the audio owner to construct ONE strict-TLS source.
        if ((code == 301 || code == 302 || code == 303 || code == 307 || code == 308) &&
            pocketjazz::copySafeHttpsUpgrade(http.getLocation().c_str(),
                                              httpsUpgrade_, sizeof(httpsUpgrade_))) {
            http.end();
            return false;
        }
        cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
        http.end();
        return false;
    }
    uint32_t interval = 0;
    const String metaHeader = http.header("icy-metaint");
    if (!pocketjazz::parseIcyMetaInterval(metaHeader.c_str(), interval)) {
        cb.st(STATUS_HTTPFAIL, PSTR("Invalid ICY metadata interval"));
        http.end();
        return false;
    }
    String encoding = http.header("Transfer-Encoding");
    encoding.trim();
    encoding.toLowerCase();
    if (encoding.length() && encoding != "identity" && encoding != "chunked") {
        cb.st(STATUS_HTTPFAIL, PSTR("Unsupported HTTP transfer encoding"));
        http.end();
        return false;
    }
    chunked = encoding == "chunked";
    chunkDecoder.reset();
    metadata.reset(interval);
    size = chunked ? -1 : http.getSize();
    strncpy(saveURL, url, sizeof(saveURL) - 1);
    saveURL[sizeof(saveURL) - 1] = 0;
    return true;
}

AudioFileSourceICYStream::~AudioFileSourceICYStream() { http.end(); }

void AudioFileSourceICYStream::reportDisconnected() {
    if (!disconnectedReported) {
        disconnectedReported = true;
        cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    }
}

// Raw HTTPClient::getStreamPtr() is NOT dechunked by HTTPClient. A shared,
// fixed-memory framing parser removes HTTP chunk markers BEFORE ICY length
// accounting and the MP3 decoder. Only payload is returned; no dynamic String
// growth or per-MP3-byte network reads.
uint32_t AudioFileSourceICYStream::readBody(uint8_t *data, uint32_t len) {
    if (!data || !len || !http.connected()) return 0;
    auto *stream = http.getStreamPtr();
    if (!stream) return 0;
    if (!chunked) {
        const int avail = stream->available();
        if (avail <= 0) return 0;
        const int got = stream->read(data, std::min<uint32_t>(len, static_cast<uint32_t>(avail)));
        if (got > 0) bodyBytesConsumed_ += static_cast<uint32_t>(got);
        return got > 0 ? static_cast<uint32_t>(got) : 0;
    }
    // Strict upper bound per call, even for maliciously long framed input.
    for (uint16_t budget = 0; budget < 128 && !chunkDecoder.invalid() &&
                             !chunkDecoder.done(); ++budget) {
        const int avail = stream->available();
        if (avail <= 0) return 0;
        if (chunkDecoder.payloadRemaining()) {
            const uint32_t wanted = std::min<uint32_t>(len,
                std::min<uint32_t>(chunkDecoder.payloadRemaining(), avail));
            const int got = stream->read(data, wanted);
            if (got <= 0) return 0;
            if (!chunkDecoder.consumePayload(got)) break;
            bodyBytesConsumed_ += static_cast<uint32_t>(got);
            return static_cast<uint32_t>(got);
        }
        const int ch = stream->read();
        if (ch < 0 || !chunkDecoder.consumeFraming(static_cast<uint8_t>(ch))) break;
    }
    if (chunkDecoder.invalid()) {
        cb.st(STATUS_HTTPFAIL, PSTR("Invalid HTTP chunked body"));
        http.end();
    } else if (chunkDecoder.done()) {
        // Valid final zero chunk means clean finite EOF, not an MP3 byte.
        http.end();
    }
    return 0;
}

uint32_t AudioFileSourceICYStream::getSize() {
    // Base source exposes the physical HTTP Content-Length even though pos
    // counts only MP3. Project a consistent MP3 end as ICY metadata arrives.
    return pocketjazz::projectedMp3Size(size, bodyBytesConsumed_,
                                        static_cast<uint32_t>(pos), UINT32_MAX);
}

uint32_t AudioFileSourceICYStream::readInternal(void *data, uint32_t len,
                                                bool nonBlock) {
    if (!data || !len) return 0;
    if (!http.connected()) {
        reportDisconnected();
        // The audio task's single owner schedules reconnect. NEVER sleep,
        // open a new socket or retry synchronously in readNonBlock().
        if (nonBlock || !reconnectTries) return 0;
        for (int i = 0; i < reconnectTries; ++i) {
            cb.st(STATUS_RECONNECTING, PSTR("Reconnecting HTTP"));
            delay(reconnectDelayMs);
            if (open(saveURL)) {
                cb.st(STATUS_RECONNECTED, PSTR("HTTP reconnected"));
                break;
            }
        }
        if (!http.connected()) return 0;
    }
    uint32_t delivered = 0;
    // Nonblocking policy: at most one MP3 body read and at most two metadata
    // fragments (128 B each) per call. No loops waiting for available bytes.
    uint32_t budget = audio_icy::IncrementalMetadata::kMaxPerPoll;
    for (int iterations = 0; iterations < 4 && delivered < len; ++iterations) {
        if (metadata.needsMetadata()) {
            metadata.enterMetadata(millis());
            if (metadata.timedOut(millis())) {
                cb.st(STATUS_NODATA, PSTR("ICY metadata stalled"));
                http.end(); // let owning audioWorker perform bounded reconnect
                return delivered;
            }
            if (metadata.waitingLength()) {
                uint8_t metadataLength = 0;
                if (readBody(&metadataLength, 1) != 1) return delivered;
                if (!metadata.acceptLength(metadataLength)) {
                    cb.st(STATUS_HTTPFAIL, PSTR("Invalid ICY length state"));
                    http.end();
                    return delivered;
                }
                continue;
            }
            if (!budget) return delivered;
            uint8_t scratch[128];
            const uint32_t toRead = std::min<uint32_t>(sizeof(scratch),
                std::min<uint32_t>(metadata.metadataRemaining(), budget));
            const uint32_t got = readBody(scratch, toRead);
            if (!got) return delivered;
            char title[pocketjazz::IcyMetadataText::kTitleCapacity]{};
            if (!metadata.acceptMetadata(scratch, got, title)) {
                cb.st(STATUS_HTTPFAIL, PSTR("Invalid ICY metadata state"));
                http.end();
                return delivered;
            }
            budget -= got;
            if (title[0]) cb.md("StreamTitle", false, title);
            // If more than 256 bytes advertised, preserve parser/remaining
            // state for the next poll; never wait for a large ICY block.
            continue;
        }
        uint32_t wanted = metadata.audioAllowance(len - delivered);
        // Content-Length includes metadata bytes and does not equal MP3 pos.
        if (size > 0 && !metadata.interval()) {
            if (pos >= size) return delivered;
            wanted = std::min<uint32_t>(wanted, static_cast<uint32_t>(size - pos));
        }
        if (!wanted) return delivered;
        if (!nonBlock && !delivered) {
            // Historical blocking read() may wait for MP3, but this branch is
            // never entered by cooperative AudioFileSourceBuffer::fill().
            const uint32_t started = millis();
            auto *stream = http.getStreamPtr();
            while (stream && !stream->available() && http.connected() &&
                   static_cast<uint32_t>(millis() - started) < 500) yield();
        }
        const uint32_t got = readBody(static_cast<uint8_t*>(data) + delivered, wanted);
        if (!got) return delivered;
        if (!metadata.acceptAudio(got)) {
            cb.st(STATUS_HTTPFAIL, PSTR("Invalid ICY audio boundary"));
            http.end();
            return delivered;
        }
        pos += static_cast<int>(got);
        delivered += got;
        // Never consume a delayed metadata block in the same call as playable
        // MP3: return bytes immediately so the decoder can drain its ring.
        return delivered;
    }
    return delivered;
}

bool AudioFileSourceICYStream::close() {
    httpsUpgrade_[0] = 0;
    metadata.reset();
    chunkDecoder.reset();
    chunked = false;
    disconnectedReported = false;
    http.end();
    return true;
}

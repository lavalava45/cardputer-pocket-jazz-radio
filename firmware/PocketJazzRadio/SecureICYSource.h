/*
 * Verified HTTPS ICY/MP3 input for ESP8266Audio 2.4.1 / Arduino-ESP32 3.x.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The caller provides a PEM CA certificate (or concatenated roots). No
 * insecure TLS mode or implicit scheme downgrade is supported. HTTP streams
 * continue to use the upstream AudioFileSourceICYStream independently.
 */
#pragma once

#include <AudioFileSource.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Arduino.h>
#include "IcyMetadataText.h"
#include "HttpChunkedBody.h"
#include "HttpBodyAudioSize.h"
#include "AudioRuntimeDiagnostics.h"

// Phase values are deliberately numeric: the UI and serial trace display no
// hostnames, station URLs, passwords or certificate contents.
enum RadioHttpsPhase : uint8_t {
  HTTPS_NONE = 0, HTTPS_INVALID = 1, HTTPS_BEGIN = 2, HTTPS_DNS = 3,
  HTTPS_TCP_TLS = 4, HTTPS_POST_CONNECT = 5, HTTPS_HTTP = 6, HTTPS_READY = 7
};

struct RadioHttpsTrace {
  uint8_t phase = HTTPS_NONE;
  uint8_t hop = 0;             // 1-based HTTP request index, including redirects
  int8_t dnsOk = -1;          // -1=not attempted, 0=failed, 1=resolved
  int8_t secureOk = -1;       // -1=not attempted, 0=failed, 1=connected
  int httpStatus = 0;         // signed HTTPClient result, not a TLS error
  uint32_t dnsMs = 0;
  uint32_t secureMs = 0;      // TCP + strict TLS combined, not TLS alone
  uint32_t requestMs = 0;
  uint32_t freeBefore = 0;   // immediately before TCP/TLS; 8-bit capable heap
  uint32_t largestBefore = 0;
  uint32_t freeAfter = 0;    // immediately after TCP/TLS result
  uint32_t largestAfter = 0;
};

class RadioTracedTlsClient final : public NetworkClientSecure {
 public:
  RadioHttpsTrace trace;
  pocketjazz::AudioRuntimeProbe* runtime = nullptr;
  // HTTPClient holds us as a NetworkClient*. The base connect(host,port,timeout)
  // is virtual through ESPLwIPClient; override to observe its ONE DNS lookup
  // and the SAME verified host/IP TLS connection, not a separate probe socket.
  int connect(const char* host, uint16_t port, int32_t timeout) override;
};

class SecureICYSource final : public AudioFileSource {
 public:
  // caPem must remain alive throughout this source's lifetime; it is not copied.
  SecureICYSource(const char *url, const char *caPem);
  ~SecureICYSource() override;

  bool open(const char *url) override;
  uint32_t read(void *data, uint32_t len) override;
  uint32_t readNonBlock(void *data, uint32_t len) override;
  bool seek(int32_t, int) override { return false; }
  bool close() override;
  bool isOpen() override;
  uint32_t getSize() override;
  uint32_t getPos() override;
  bool RegisterStatusCB(AudioStatus::statusCBFn fn, void *data) override;
  // Constructors open immediately; retain failures until a callback is registered.
  int lastFailureCode() const { return lastFailureCode_; }
  int lastTlsError() const { return lastTlsError_; }
  RadioHttpsTrace lastHttpsTrace() const { return trace_; }
  void setRuntimeProbe(pocketjazz::AudioRuntimeProbe* p) { tls_.runtime = p; }
  // A background station-title peek is lower-priority than the actual player:
  // bound EACH HTTP hop and forbid long redirect chains. Call before open().
  // This never relaxes CA/hostname verification or permits HTTP fallback.
  void setPeekLimits() {
    maxRedirects_ = 1;
    connectTimeoutMs_ = 1500;
    ioTimeoutMs_ = 2000;
  }

 private:
  bool startRequest(const String &url);
  void reportFailure(int code, const String &message);
  // De-chunk raw HTTPClient socket bytes before parsing ICY or feeding MP3.
  uint32_t readBody(uint8_t* out, uint32_t len);
  bool consumeMetadata(bool nonBlocking);
  uint32_t readAudio(void *data, uint32_t len, bool nonBlocking);
  static bool safeUrl(const String &url);
  static bool resolveRedirect(const String &base, const String &location, String &next);

  const char *caPem_;
  RadioTracedTlsClient tls_;  // TLS object must outlive http_.
  HTTPClient http_;
  RadioHttpsTrace trace_;
  String requestUrl_;
  bool open_ = false;
  uint32_t position_ = 0;
  uint32_t bodyBytesConsumed_ = 0;
  int32_t length_ = -1;
  bool chunked_ = false;
  pocketjazz::HttpChunkedBody chunkDecoder_;
  uint32_t icyInterval_ = 0;
  uint32_t untilMetadata_ = 0;
  uint32_t metadataLeft_ = 0;
  uint32_t metadataStartedAt_ = 0;
  bool needsMetadataLength_ = false;
  // One fixed <=256-byte streaming parser instead of per-byte String growth
  // (ICY advertises up to 4080 metadata bytes). Audio data stays unchanged.
  pocketjazz::IcyMetadataText metadataParser_;
  bool metadataMalformed_ = false;
  int lastFailureCode_ = 0;
  int lastTlsError_ = 0;
  String lastFailureMessage_;
  uint8_t maxRedirects_ = 5;
  int32_t connectTimeoutMs_ = 5000;
  uint16_t ioTimeoutMs_ = 4000;
};

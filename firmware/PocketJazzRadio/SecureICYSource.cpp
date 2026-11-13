#include "SecureICYSource.h"

#include <time.h>
#include <algorithm>
#include <esp_heap_caps.h>

namespace {
constexpr time_t MIN_VALID_TLS_TIME = 1704067200;  // 2024-01-01 UTC
constexpr uint32_t METADATA_TIMEOUT_MS = 2500;

bool isRedirect(int code) {
  return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}
}  // namespace

int RadioTracedTlsClient::connect(const char* host, uint16_t port, int32_t timeout) {
  trace.dnsOk = -1;
  trace.secureOk = -1;
  trace.phase = HTTPS_DNS;
  if (runtime) runtime->enter(pocketjazz::AudioStage::DNS, millis());
  const uint32_t dnsStarted = millis();
  IPAddress address;
  // Identical lookup to NetworkClientSecure::connect(host,...), but exposed as
  // one phase. No second DNS lookup and no additional network connection.
  if (!Network.hostByName(host, address)) {
    trace.dnsMs = millis() - dnsStarted;
    trace.dnsOk = 0;
    return 0;
  }
  trace.dnsMs = millis() - dnsStarted;
  trace.dnsOk = 1;
  trace.phase = HTTPS_TCP_TLS;
  if (runtime) runtime->enter(pocketjazz::AudioStage::TCP_TLS, millis());
  _timeout = timeout; // exactly the timeout set by the original SDK overload
  trace.freeBefore = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  trace.largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const uint32_t secureStarted = millis();
  // Preserve SNI and hostname verification: NEVER use connect(IP,port), which
  // would omit the hostname; forward to the exact SDK host-validated overload.
  const int ok = NetworkClientSecure::connect(address, port, host,
                                              _CA_cert, _cert, _private_key);
  trace.secureMs = millis() - secureStarted;
  trace.freeAfter = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  trace.largestAfter = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  trace.secureOk = ok ? 1 : 0;
  if (runtime && ok) runtime->enter(pocketjazz::AudioStage::HTTP, millis());
  return ok;
}

SecureICYSource::SecureICYSource(const char *url, const char *caPem) : caPem_(caPem) {
  if (url) open(url);
}

SecureICYSource::~SecureICYSource() { close(); }

bool SecureICYSource::RegisterStatusCB(AudioStatus::statusCBFn fn, void *data) {
  const bool ok = AudioFileSource::RegisterStatusCB(fn, data);
  if (fn && lastFailureCode_ != 0) cb.st(lastFailureCode_, lastFailureMessage_.c_str());
  return ok;
}

void SecureICYSource::reportFailure(int code, const String &message) {
  lastFailureCode_ = code;
  lastFailureMessage_ = message;
  // Latch before close()/next hop can reset transport state. Peek has no probe.
  if (tls_.runtime) tls_.runtime->fault(millis(), 1, code, lastTlsError_,
      heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
      tls_.trace.freeBefore, tls_.trace.largestBefore);
  cb.st(code, lastFailureMessage_.c_str());
}

bool SecureICYSource::safeUrl(const String &url) {
  if (!url.startsWith("https://") || url.length() > 1024) return false;
  const int hostBegin = 8;
  const int pathBegin = url.indexOf('/', hostBegin);
  const int queryBegin = url.indexOf('?', hostBegin);
  // Query is not part of the authority, even if the URL has no '/' path.
  const int hostEnd = pathBegin < 0 ? (queryBegin < 0 ? url.length() : queryBegin)
      : (queryBegin < 0 ? pathBegin : std::min(pathBegin, queryBegin));
  const String authority = url.substring(hostBegin, hostEnd);
  if (authority.isEmpty() || authority.indexOf('@') >= 0 || authority.indexOf('#') >= 0 ||
      authority.indexOf('\\') >= 0 || authority.indexOf(' ') >= 0) return false;
  // HTTPClient handles hostnames and ports. Explicitly prohibit embedded credentials.
  return url.indexOf('\r') < 0 && url.indexOf('\n') < 0 && url.indexOf('\\') < 0;
}

// HTTPClient::beginInternal() in Arduino-ESP32 3.3.9 splits the authority
// ONLY at '/', so a valid host?query would otherwise become a bogus DNS host.
// Normalize direct HTTPS and every HTTPS->HTTPS redirect, not only upgrades
// originating at a plain HTTP stream. No scheme or security policy changes.
static String httpClientCompatibleHttpsUrl(const String &url) {
  const int query = url.indexOf('?', 8);
  const int slash = url.indexOf('/', 8);
  if (query >= 0 && (slash < 0 || query < slash))
    return url.substring(0, query) + "/" + url.substring(query);
  return url;
}

bool SecureICYSource::resolveRedirect(const String &base, const String &location, String &next) {
  String target = location;
  target.trim();
  if (target.startsWith("https://")) {
    next = target;
  } else if (target.startsWith("/") && !target.startsWith("//")) {
    const int slash = base.indexOf('/', 8);
    next = (slash >= 0 ? base.substring(0, slash) : base) + target;
  } else {
    return false;  // Reject HTTP, protocol-relative, non-root-relative, malformed redirects.
  }
  return safeUrl(next);
}

bool SecureICYSource::startRequest(const String &url) {
  http_.end();
  tls_.stop();
  tls_.trace = RadioHttpsTrace{};
  // setCACert() enables chain verification and hostname checking in mbedTLS.
  tls_.setCACert(caPem_);
  // SDK 3.3.9 defaults to a separate 120-second TLS handshake deadline.
  // HTTPClient's connect/read timeouts do not configure it. Units here are
  // seconds; round up so the 1500-ms optional peek gets a finite 2-s bound.
  tls_.setHandshakeTimeout((connectTimeoutMs_ + 999u) / 1000u);
  if (!http_.begin(tls_, url)) return false;
  const char *headers[] = {"icy-metaint", "icy-name", "icy-genre", "icy-br", "Location",
                           "Content-Type", "Transfer-Encoding"};
  http_.collectHeaders(headers, 7);
  http_.addHeader("Icy-MetaData", "1");
  http_.setReuse(false);
  http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http_.setConnectTimeout(connectTimeoutMs_);
  http_.setTimeout(ioTimeoutMs_);
  return true;
}

bool SecureICYSource::open(const char *url) {
  close();
  lastFailureCode_ = 0;
  lastTlsError_ = 0;
  trace_ = RadioHttpsTrace{};
  lastFailureMessage_ = String();
  if (!url || !safeUrl(url) || !caPem_ || !*caPem_ || time(nullptr) < MIN_VALID_TLS_TIME) {
    trace_.phase = HTTPS_INVALID;
    reportFailure(2, "HTTPS needs valid URL, verified CA and synchronized time");
    return false;
  }
  String target = httpClientCompatibleHttpsUrl(String(url));
  for (uint8_t hop = 0; hop <= maxRedirects_; ++hop) {
    if (target.length() > 1024) {
      reportFailure(2, "HTTPS normalized URL exceeds safe length");
      break;
    }
    trace_.phase = HTTPS_BEGIN;
    trace_.hop = hop + 1;
    if (tls_.runtime) tls_.runtime->setHop(hop + 1);
    if (tls_.runtime) tls_.runtime->enter(pocketjazz::AudioStage::OPEN, millis());
    if (!startRequest(target)) {
      reportFailure(2, "HTTPS HTTPClient.begin failed");
      break;
    }
    if (tls_.runtime) tls_.runtime->enter(pocketjazz::AudioStage::HTTP, millis());
    const uint32_t requestStarted = millis();
    const int status = http_.GET();
    trace_ = tls_.trace;
    trace_.hop = hop + 1;
    trace_.requestMs = millis() - requestStarted;
    trace_.httpStatus = status;
    if (status == HTTP_CODE_OK) trace_.phase = HTTPS_READY;
    else if (status < 0 && trace_.dnsOk == 0) trace_.phase = HTTPS_DNS;
    else if (status < 0 && trace_.secureOk == 0) trace_.phase = HTTPS_TCP_TLS;
    else if (status == HTTPC_ERROR_CONNECTION_REFUSED && trace_.secureOk == 1)
      trace_.phase = HTTPS_POST_CONNECT;
    else trace_.phase = HTTPS_HTTP;
    if (status < 0) {
      // HTTPClient collapses DNS/TCP/TLS open failures into -1. Preserve
      // the underlying mbedTLS integer before end()/stop(), without exposing
      // certificates, credentials, station URLs or unbounded strings.
      char tlsErrorText[80]{};
      lastTlsError_ = trace_.dnsOk == 0 ? 0 : tls_.lastError(tlsErrorText, sizeof(tlsErrorText));
    }
    if (status == HTTP_CODE_OK) {
      String type = http_.header("Content-Type");
      type.toLowerCase();
      type.trim();
      // Explicit error-document MIME types are not radio data. Accept absent,
      // generic application/octet-stream and broadcaster-specific audio types.
      if (type.startsWith("text/html") || type.startsWith("application/json") ||
          type.startsWith("text/xml") || type.startsWith("application/xml")) {
        reportFailure(415, String("Unexpected non-audio Content-Type: ") + type);
        close();
        return false;
      }
      String transferEncoding = http_.header("Transfer-Encoding");
      transferEncoding.toLowerCase();
      transferEncoding.trim();
      // HTTPClient::getStreamPtr() exposes raw TCP/TLS entity bytes; it does
      // NOT de-chunk Transfer-Encoding. Radio France/FIP uses chunked HTTP;
      // unstripped hex sizes and CRLF corrupt MP3 frames each few kilobytes.
      // Reject unsupported codings rather than passing framing to the decoder.
      if (transferEncoding.length() && transferEncoding != "identity" &&
          transferEncoding != "chunked") {
        reportFailure(415, "Unsupported HTTP Transfer-Encoding");
        close();
        return false;
      }
      chunked_ = transferEncoding == "chunked";
      chunkDecoder_.reset();
      const String interval = http_.header("icy-metaint");
      if (!pocketjazz::parseIcyMetaInterval(interval.c_str(), icyInterval_)) {
        reportFailure(2, "Invalid ICY metadata interval");
        close();
        return false;
      }
      untilMetadata_ = icyInterval_;
      needsMetadataLength_ = false;
      metadataLeft_ = 0;
      metadataStartedAt_ = 0;
      metadataParser_.reset();
      metadataMalformed_ = false;
      length_ = chunked_ ? -1 : http_.getSize();
      position_ = 0;
      bodyBytesConsumed_ = 0;
      requestUrl_ = target;
      open_ = true;
      lastFailureCode_ = 0;
      lastFailureMessage_ = String();
      return true;
    }
    // Preserve actual 403/404/5xx and negative HTTPClient transport errors.
    // The callback's int code is signed; avoid converting these to status 2.
    if (!isRedirect(status)) {
      reportFailure(status, status < 0 ? HTTPClient::errorToString(status)
                                        : String("HTTP status ") + status);
      break;
    }
    if (hop == maxRedirects_) {
      reportFailure(status, "HTTPS redirect limit reached");
      break;
    }
    String next;
    if (!resolveRedirect(target, http_.getLocation(), next)) {
      reportFailure(status, "HTTPS redirect rejected: unsafe or malformed Location");
      break;
    }
    target = httpClientCompatibleHttpsUrl(next);
  }
  close();
  return false;
}

uint32_t SecureICYSource::readBody(uint8_t* out, uint32_t len) {
  if (!open_ || !out || !len) return 0;
  auto* stream = http_.getStreamPtr();
  if (!stream) return 0;
  // On ordinary non-chunked streams retain the original bulk read path.
  if (!chunked_) {
    const int available = stream->available();
    if (available <= 0) return 0;
    const int got = stream->read(out, std::min<uint32_t>(len, static_cast<uint32_t>(available)));
    if (got > 0) bodyBytesConsumed_ += static_cast<uint32_t>(got);
    return got > 0 ? static_cast<uint32_t>(got) : 0;
  }
  // Parse only the small ASCII chunk delimiters byte-wise; transfer MP3 in
  // bulk so the TLS and I2S tasks are not burdened with per-audio-byte calls.
  // A bounded number of framing bytes per poll prevents a malicious server
  // from monopolizing this audio task.
  uint16_t framingBudget = 256;
  while (framingBudget-- && stream->available() > 0 && !chunkDecoder_.done() &&
         !chunkDecoder_.invalid()) {
    const uint32_t remaining = chunkDecoder_.payloadRemaining();
    if (remaining) {
      const int available = stream->available();
      const uint32_t wanted = std::min<uint32_t>(
          len, std::min<uint32_t>(remaining, static_cast<uint32_t>(available)));
      if (!wanted) return 0;
      const int got = stream->read(out, wanted);
      if (got <= 0) return 0;
      if (!chunkDecoder_.consumePayload(static_cast<uint32_t>(got))) break;
      bodyBytesConsumed_ += static_cast<uint32_t>(got);
      return static_cast<uint32_t>(got);
    }
    const int ch = stream->read();
    if (ch < 0) return 0;
    if (!chunkDecoder_.consumeFraming(static_cast<uint8_t>(ch))) break;
  }
  if (chunkDecoder_.invalid()) {
    reportFailure(-16, "Malformed HTTP chunked body; closing stream");
    close();
  } else if (chunkDecoder_.done()) {
    close(); // Explicit zero-size final chunk; not MP3 data.
  }
  return 0;
}

bool SecureICYSource::consumeMetadata(bool nonBlocking) {
  pocketjazz::AudioRuntimeProbe::Scope stage(tls_.runtime, pocketjazz::AudioStage::META, millis());
  const uint32_t started = millis();
  // A complete ICY block may be 4080 B. Even when all bytes are already in
  // mbedTLS, never drain the whole block in one audio-worker call.
  uint32_t pollBudget = nonBlocking ? 256u : pocketjazz::IcyMetadataText::kMaxMetadataBytes;
  if (needsMetadataLength_) {
    uint8_t lengthByte = 0;
    if (readBody(&lengthByte, 1) != 1) return false;
    metadataLeft_ = static_cast<uint32_t>(lengthByte) * 16;
    metadataParser_.reset();
    metadataMalformed_ = false;
    needsMetadataLength_ = false;
  }
  while (metadataLeft_ > 0) {
    if (!pollBudget) return false; // Keep parser/remaining bytes for next poll.
    uint8_t scratch[128];
    const uint32_t wanted = std::min<uint32_t>(
        sizeof(scratch), std::min<uint32_t>(metadataLeft_, pollBudget));
    const uint32_t got = readBody(scratch, wanted);
    if (!got) {
      if (nonBlocking || millis() - started >= METADATA_TIMEOUT_MS) return false;
      delay(1);
      continue;
    }
    // Never assemble broadcaster-provided metadata in a dynamic String.
    // Even malformed values must be consumed to keep MP3 byte boundaries.
    if (!metadataMalformed_ &&
        !metadataParser_.feed(scratch, static_cast<size_t>(got)))
      metadataMalformed_ = true;
    metadataLeft_ -= got;
    pollBudget -= got;
  }
  char title[pocketjazz::IcyMetadataText::kTitleCapacity];
  if (!metadataMalformed_ && metadataParser_.finish(title))
    cb.md("StreamTitle", false, title);
  metadataParser_.reset();
  metadataMalformed_ = false;
  untilMetadata_ = icyInterval_;
  return true;
}

uint32_t SecureICYSource::readAudio(void *data, uint32_t len, bool nonBlocking) {
  pocketjazz::AudioRuntimeProbe::Scope stage(tls_.runtime, pocketjazz::AudioStage::NET_READ, millis());
  if (!open_ || !data || !len || !http_.connected()) return 0;
  if (icyInterval_ && untilMetadata_ == 0) {
    if (!needsMetadataLength_ && metadataLeft_ == 0) {
      needsMetadataLength_ = true;
      metadataStartedAt_ = millis();
    }
    if (!consumeMetadata(nonBlocking)) {
      // A broadcaster may hang after the metadata length byte while TCP
      // remains connected. Keep ONE deadline across repeated decoder reads,
      // not a fresh 2.5 s budget each time, and reconnect on timeout.
      if ((uint32_t)(millis() - metadataStartedAt_) >= METADATA_TIMEOUT_MS) {
        reportFailure(-15, "ICY metadata stalled; closing stream");
        close();
      }
      return 0;
    }
  }
  if (icyInterval_) len = std::min(len, untilMetadata_);
  if (length_ >= 0) {
    if (position_ >= static_cast<uint32_t>(length_)) return 0;
    len = std::min(len, static_cast<uint32_t>(length_) - position_);
  }
  if (!len) return 0;
  if (!nonBlocking) {
    const uint32_t started = millis();
    uint32_t got = 0;
    while (http_.connected() && millis() - started < 500) {
      got = readBody(static_cast<uint8_t*>(data), len);
      if (got || !open_) break;
      delay(1);
    }
    if (!got) return 0;
    position_ += got;
    if (icyInterval_) untilMetadata_ -= got;
    return got;
  }
  const uint32_t got = readBody(static_cast<uint8_t*>(data), len);
  position_ += got;
  if (icyInterval_) untilMetadata_ -= got;
  return got;
}

uint32_t SecureICYSource::read(void *data, uint32_t len) { return readAudio(data, len, false); }
uint32_t SecureICYSource::readNonBlock(void *data, uint32_t len) { return readAudio(data, len, true); }
bool SecureICYSource::close() {
  open_ = false;
  chunked_ = false;
  chunkDecoder_.reset();
  http_.end();
  tls_.stop();
  return true;
}
bool SecureICYSource::isOpen() { return open_ && http_.connected(); }
uint32_t SecureICYSource::getSize() {
  return pocketjazz::projectedMp3Size(length_, bodyBytesConsumed_, position_, 0);
}
uint32_t SecureICYSource::getPos() { return position_; }

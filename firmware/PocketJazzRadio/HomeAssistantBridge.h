// SPDX-License-Identifier: MIT
// Portable, transport-neutral MQTT payload contract. NO socket, TLS or MQTT client.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace pocketjazz {
namespace ha {

enum { kTopicMax = 80, kPayloadMax = 256, kTitleMax = 96,
       kNoticeMax = 120, kQueueCapacity = 4,
       kForwardUrlMax = 192, kRecipientsMax = 4, kRecipientMax = 24,
       kForwardHostsMax = 4, kForwardHostMax = 64,
       kNoticeMinIntervalMs = 15000, kForwardMinIntervalMs = 10000 };

// Host-only rendering suggestion; never issues a sound/volume/NVS command.
// Muted means no audio volume adjustment, even during a notification.
struct NoticePolicy {
  bool display;
  uint8_t duckPercent;
  uint16_t durationMs;
  NoticePolicy() : display(false), duckPercent(0), durationMs(0) {}
};

// Accept only an explicitly terminated string *within* the supplied
// accessible-byte budget. Do not read s[capacity] to discover that a field
// which filled its entire fixed-size array is unterminated.
inline bool boundedNul(const char* s, size_t capacity, size_t& length) {
  length = 0;
  if (!s || !capacity) return false;
  while (length < capacity) {
    if (!s[length]) return true;
    ++length;
  }
  return false;
}

class NoticeRatePolicy {
 public:
  NoticeRatePolicy() : seen_(false), last_(0) {}
  bool decide(uint32_t nowMs, bool muted, NoticePolicy& out) {
    out = NoticePolicy();
    if (seen_ && uint32_t(nowMs - last_) < kNoticeMinIntervalMs) return false;
    seen_ = true;
    last_ = nowMs;
    out.display = true;
    // The roadmap's approximate 30% transient duck, never persisted in NVS.
    out.duckPercent = muted ? 0 : 30;
    out.durationMs = muted ? 0 : 1000;
    return true;
  }
  void clear() { seen_ = false; last_ = 0; }
 private:
  bool seen_;
  uint32_t last_;
};

// Strictly limited HTTPS media URL: no HTTP downgrade, embedded credentials,
// fragment, backslash, control chars, whitespace or JSON metacharacters.
// A separately configured HA automation decides whether/how to play this URL.
inline bool validForwardUrl(const char* url) {
  size_t n = 0;
  if (!boundedNul(url, kForwardUrlMax, n) || n < 12 ||
      std::strncmp(url, "https://", 8)) return false;
  const size_t hostBegin = 8;
  size_t hostEnd = hostBegin;
  for (; hostEnd < n && url[hostEnd] != '/' && url[hostEnd] != '?'; ++hostEnd) {
    const char c = url[hostEnd];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
  }
  if (hostEnd == hostBegin || hostEnd - hostBegin > 253 ||
      url[hostBegin] == '.' || url[hostBegin] == '-' ||
      url[hostEnd - 1] == '.' || url[hostEnd - 1] == '-') return false;
  bool hasDot = false;
  size_t lastDot = hostBegin;
  for (size_t i = hostBegin; i < hostEnd; ++i) {
    if (url[i] == '.') {
      if (i == hostBegin || url[i - 1] == '.' || url[i - 1] == '-') return false;
      hasDot = true;
      lastDot = i;
    }
  }
  if (!hasDot || hostEnd - lastDot < 3) return false;
  for (size_t i = lastDot + 1; i < hostEnd; ++i)
    if (!((url[i] >= 'a' && url[i] <= 'z') ||
          (url[i] >= 'A' && url[i] <= 'Z'))) return false;
  for (size_t i = hostEnd; i < n; ++i) {
    const unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= 0x20 || c >= 0x7f || c == '"' || c == '\\' || c == '#' ||
        c == '<' || c == '>' || c == '{' || c == '}') return false;
  }
  return true;
}

// A transport MUST enforce: authenticated TLS with verified broker certificate
// and hostname, authorized publisher identity, subscription ACL, and a fresh
// authenticated session. No credentials are passed through this module.
struct VerifiedSession {
  bool tlsPeerVerified;
  bool hostnameVerified;
  bool clientAuthenticated;
  bool commandAclVerified;
  VerifiedSession() : tlsPeerVerified(false), hostnameVerified(false),
      clientAuthenticated(false), commandAclVerified(false) {}
  bool permitted() const {
    return tlsPeerVerified && hostnameVerified && clientAuthenticated && commandAclVerified;
  }
};

inline bool validUtf8(const uint8_t* bytes, size_t n) {
  if (!bytes && n) return false;
  for (size_t i = 0; i < n;) {
    const uint8_t a = bytes[i++];
    if (a < 0x80) { if (a == 0 || a < 0x20 || a == 0x7f) return false; continue; }
    uint32_t cp = 0; size_t tail = 0;
    if (a >= 0xc2 && a <= 0xdf) { cp = a & 31; tail = 1; }
    else if (a >= 0xe0 && a <= 0xef) { cp = a & 15; tail = 2; }
    else if (a >= 0xf0 && a <= 0xf4) { cp = a & 7; tail = 3; }
    else return false;
    if (tail > n - i) return false;
    for (size_t j = 0; j < tail; ++j) {
      const uint8_t b = bytes[i++];
      if ((b & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (b & 63);
    }
    if (cp < (tail == 1 ? 0x80u : tail == 2 ? 0x800u : 0x10000u) ||
        cp > 0x10ffffu || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  }
  return true;
}

inline bool validDeviceId(const char* id) {
  if (!id) return false;
  size_t n = 0;
  while (n < 24 && id[n]) {
    const char c = id[n++];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return n != 0 && n < 24 && id[n] == 0;
}

enum CommandKind { kNone, kPlay, kPause, kStop, kNext, kPrevious, kSetVolume };
struct Command {
  CommandKind kind;
  uint8_t volume;
  Command() : kind(kNone), volume(0) {}
};

// No MQTT network implementation is supplied. MQTT frame length, packet-id,
// keepalive, reconnect and retransmission are exclusively transport duties.
// This layer rejects retained, non-QoS0 and out-of-policy commands.
class HomeAssistantBridge {
 public:
  HomeAssistantBridge() : enabled_(false), head_(0), size_(0),
      recipientCount_(0), hostCount_(0), forwarded_(false), forwardedAt_(0) {
    commandTopic_[0] = stateTopic_[0] = noticeTopic_[0] = notifyTopic_[0] = forwardTopic_[0] = 0;
    std::memset(recipients_, 0, sizeof(recipients_));
    std::memset(hosts_, 0, sizeof(hosts_));
  }
  bool configure(const char* deviceId, bool enabled) {
    enabled_ = false;
    clear();
    recipientCount_ = 0;
    hostCount_ = 0;
    std::memset(recipients_, 0, sizeof(recipients_));
    std::memset(hosts_, 0, sizeof(hosts_));
    forwarded_ = false;
    forwardedAt_ = 0;
    commandTopic_[0] = stateTopic_[0] = noticeTopic_[0] = notifyTopic_[0] = forwardTopic_[0] = 0;
    if (!validDeviceId(deviceId)) return false;
    const int a = std::snprintf(commandTopic_, sizeof(commandTopic_), "pocketjazz/%s/cmd", deviceId);
    const int b = std::snprintf(stateTopic_, sizeof(stateTopic_), "pocketjazz/%s/state", deviceId);
    const int c = std::snprintf(noticeTopic_, sizeof(noticeTopic_), "pocketjazz/%s/notice", deviceId);
    const int d = std::snprintf(notifyTopic_, sizeof(notifyTopic_), "pocketjazz/%s/notify", deviceId);
    const int e = std::snprintf(forwardTopic_, sizeof(forwardTopic_), "pocketjazz/%s/forward", deviceId);
    if (a < 0 || b < 0 || c < 0 || d < 0 || e < 0 || a >= int(sizeof(commandTopic_)) ||
        b >= int(sizeof(stateTopic_)) || c >= int(sizeof(noticeTopic_)) ||
        d >= int(sizeof(notifyTopic_)) || e >= int(sizeof(forwardTopic_))) return false;
    enabled_ = enabled;
    return true;
  }
  bool enabled() const { return enabled_; }
  const char* commandTopic() const { return commandTopic_; }
  const char* stateTopic() const { return stateTopic_; }
  const char* noticeTopic() const { return noticeTopic_; }
  const char* notifyTopic() const { return notifyTopic_; }
  const char* forwardTopic() const { return forwardTopic_; }

  // Human-approved logical recipients only, never accepting an HA entity ID
  // or target hostname from an inbound MQTT payload. Configure while stopped.
  bool configureRecipients(const char* const* names, size_t count) {
    recipientCount_ = 0;
    std::memset(recipients_, 0, sizeof(recipients_));
    if (!names || count == 0 || count > kRecipientsMax) return false;
    for (size_t i = 0; i < count; ++i) {
      if (!validDeviceId(names[i])) {
        std::memset(recipients_, 0, sizeof(recipients_));
        return false;
      }
      for (size_t j = 0; j < i; ++j) {
        if (std::strcmp(names[i], names[j]) == 0) {
          std::memset(recipients_, 0, sizeof(recipients_));
          return false;
        }
      }
      const size_t len = std::strlen(names[i]);
      std::memcpy(recipients_[i], names[i], len + 1);
    }
    recipientCount_ = static_cast<uint8_t>(count);
    return true;
  }

  // Human-approved station domains; blocks forwarding arbitrary HTTPS URLs
  // that could cause SSRF or fetch internal services through Home Assistant.
  bool configureStationHosts(const char* const* hosts, size_t count) {
    hostCount_ = 0;
    std::memset(hosts_, 0, sizeof(hosts_));
    if (!hosts || count == 0 || count > kForwardHostsMax) return false;
    for (size_t i = 0; i < count; ++i) {
      size_t len = 0;
      if (!boundedNul(hosts[i], kForwardHostMax, len)) return false;
      if (len < 4 || len > kForwardHostMax) return false;
      char probe[kForwardHostMax + 10] = {};
      const int n = std::snprintf(probe, sizeof(probe), "https://%s/", hosts[i]);
      if (n < 0 || size_t(n) >= sizeof(probe) || !validForwardUrl(probe)) return false;
      for (size_t j = 0; j < i; ++j)
        if (std::strcmp(hosts[i], hosts[j]) == 0) return false;
      std::memcpy(hosts_[i], hosts[i], len + 1);
    }
    hostCount_ = static_cast<uint8_t>(count);
    return true;
  }

  // Authenticated caller and externally audited publish ACL required.
  // Caller supplies monotonic milliseconds; 10 s interval survives rollover.
  bool encodeForward(const char* recipient, const char* url, uint32_t nowMs,
                     uint64_t issuedAtUnixSeconds, const VerifiedSession& session,
                     char* output, size_t cap) {
    if (output && cap) output[0] = 0;
    if (!enabled_ || !session.permitted() || !recipient || !output || !cap ||
        issuedAtUnixSeconds < 1704067200ULL || issuedAtUnixSeconds > 4102444800ULL ||
        !validForwardUrl(url)) return false;
    size_t recipientLength = 0;
    if (!boundedNul(recipient, kRecipientMax, recipientLength) ||
        !validDeviceId(recipient)) return false;
    bool allowed = false;
    for (size_t i = 0; i < recipientCount_; ++i)
      if (std::strlen(recipients_[i]) == recipientLength &&
          std::memcmp(recipient, recipients_[i], recipientLength) == 0) allowed = true;
    if (!allowed) return false;
    size_t hostLen = 0;
    while (url[8 + hostLen] && url[8 + hostLen] != '/' && url[8 + hostLen] != '?') ++hostLen;
    bool allowedHost = false;
    for (size_t i = 0; i < hostCount_; ++i)
      if (std::strlen(hosts_[i]) == hostLen &&
          std::memcmp(url + 8, hosts_[i], hostLen) == 0) allowedHost = true;
    if (!allowedHost || (forwarded_ && uint32_t(nowMs - forwardedAt_) < kForwardMinIntervalMs)) return false;
    const int n = std::snprintf(output, cap,
        "{\"recipient\":\"%s\",\"url\":\"%s\",\"issued_at\":%llu}",
        recipient, url, static_cast<unsigned long long>(issuedAtUnixSeconds));
    if (n < 0 || size_t(n) >= cap || n > kPayloadMax) {
      output[0] = 0;
      return false;
    }
    forwarded_ = true;
    forwardedAt_ = nowMs;
    return true;
  }
  void resetForwardThrottle() { forwarded_ = false; forwardedAt_ = 0; }

  bool decodeCommand(const char* topic, const uint8_t* bytes, size_t len,
                     bool retained, uint8_t qos, const VerifiedSession& session,
                     Command& out) const {
    out = Command();
    if (!enabled_ || !session.permitted() || !topic || !commandTopic_[0] ||
        std::strcmp(topic, commandTopic_) != 0 || retained || qos != 0 ||
        !bytes || len == 0 || len > 32 || !validUtf8(bytes, len)) return false;
    const struct { const char* text; CommandKind kind; } actions[] = {
      {"play", kPlay}, {"pause", kPause}, {"stop", kStop},
      {"next", kNext}, {"previous", kPrevious}
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i) {
      if (len == std::strlen(actions[i].text) &&
          std::memcmp(bytes, actions[i].text, len) == 0) {
        out.kind = actions[i].kind;
        return true;
      }
    }
    if (len < 8 || len > 10 || std::memcmp(bytes, "volume:", 7) != 0) return false;
    unsigned volume = 0;
    for (size_t i = 7; i < len; ++i) {
      if (bytes[i] < '0' || bytes[i] > '9') return false;
      volume = volume * 10 + bytes[i] - '0';
    }
    if (volume > 100 || (len > 8 && bytes[7] == '0')) return false;
    out.kind = kSetVolume;
    out.volume = static_cast<uint8_t>(volume);
    return true;
  }

  // State is bounded UTF-8 JSON; caller chooses whether/when to publish it.
  // No credentials, network config or arbitrary JSON are accepted.
  bool encodeState(const char* status, uint8_t volume, const char* title,
                   char* output, size_t capacity) const {
    size_t titleLength = 0;
    if (!enabled_ || !output || !capacity || volume > 100 || !status || !title ||
        !boundedNul(title, kTitleMax, titleLength) ||
        titleLength > kTitleMax ||
        !validUtf8(reinterpret_cast<const uint8_t*>(title), titleLength)) return false;
    if (std::strcmp(status, "playing") && std::strcmp(status, "paused") &&
        std::strcmp(status, "idle") && std::strcmp(status, "unavailable")) return false;
    size_t used = 0;
    output[0] = 0;
    const int n = std::snprintf(output, capacity, "{\"state\":\"%s\",\"volume\":%u,\"title\":\"", status, volume);
    if (n < 0 || size_t(n) >= capacity) return false;
    used = static_cast<size_t>(n);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(title); *p; ++p) {
      const char* escaped = (*p == '"' ? "\\\"" : *p == '\\' ? "\\\\" : nullptr);
      if (used + (escaped ? 2 : 1) + 3 > capacity) return false;
      if (escaped) { output[used++] = escaped[0]; output[used++] = escaped[1]; }
      else output[used++] = static_cast<char>(*p);
    }
    if (used + 3 > capacity || used + 3 > kPayloadMax) return false;
    output[used++] = '"'; output[used++] = '}'; output[used] = 0;
    return true;
  }

  bool enqueueNotice(const uint8_t* text, size_t len) {
    if (!enabled_ || !text || !len || len > kNoticeMax ||
        size_ == kQueueCapacity || !validUtf8(text, len)) return false;
    const size_t tail = (head_ + size_) % kQueueCapacity;
    std::memcpy(notices_[tail], text, len);
    notices_[tail][len] = 0;
    ++size_;
    return true;
  }
  // Inbound HA UI notices are deliberately a separate ACL-scoped topic.
  // The rendering layer must not use notices as commands or markup.
  bool receiveNotice(const char* topic, const uint8_t* bytes, size_t len,
                     bool retained, uint8_t qos, const VerifiedSession& session) {
    if (!session.permitted() || !topic || std::strcmp(topic, notifyTopic_) != 0 ||
        retained || qos != 0) return false;
    return enqueueNotice(bytes, len);
  }
  bool popNotice(char* dst, size_t capacity) {
    if (!dst || !size_ || capacity <= std::strlen(notices_[head_])) return false;
    std::strcpy(dst, notices_[head_]);
    notices_[head_][0] = 0;
    head_ = (head_ + 1) % kQueueCapacity;
    --size_;
    return true;
  }
  size_t pendingNotices() const { return size_; }
  void clear() {
    head_ = size_ = 0;
    std::memset(notices_, 0, sizeof(notices_));
  }

 private:
  bool enabled_;
  char commandTopic_[kTopicMax], stateTopic_[kTopicMax], noticeTopic_[kTopicMax], notifyTopic_[kTopicMax], forwardTopic_[kTopicMax];
  char notices_[kQueueCapacity][kNoticeMax + 1];
  size_t head_, size_;
  char recipients_[kRecipientsMax][kRecipientMax + 1];
  uint8_t recipientCount_;
  char hosts_[kForwardHostsMax][kForwardHostMax + 1];
  uint8_t hostCount_;
  bool forwarded_;
  uint32_t forwardedAt_;
};
} // namespace ha
} // namespace pocketjazz

// SPDX-License-Identifier: MIT
#include "HaMqttTransport.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(POCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT)
#include <cstring>
#include <ctime>

namespace pocketjazz { namespace ha {
namespace {
const time_t kMinimumTlsTime = 1704067200;  // valid clock is prerequisite

bool dnsHostname(const char* host) {
  if (!host) return false;
  const size_t n = strnlen(host, 254);
  if (n < 4 || n > 253 || host[0] == '.' || host[n - 1] == '.') return false;
  bool dot = false;
  size_t label = 0;
  for (size_t i = 0; i < n; ++i) {
    const char c = host[i];
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9');
    if (c == '.') {
      if (!label || label > 63 || host[i - 1] == '-') return false;
      label = 0;
      dot = true;
    } else {
      if (!alnum && c != '-') return false;
      if (label == 0 && c == '-') return false;
      ++label;
    }
  }
  return dot && label && label <= 63 && host[n - 1] != '-';
}

bool boundedNonempty(const char* value, size_t max) {
  return value && value[0] && strnlen(value, max + 1) <= max;
}
} // namespace

HaMqttTransport::HaMqttTransport()
    : client_(nullptr), rx_(nullptr), connected_(false), ready_(false),
      dropped_(0), sessionEpoch_(0), observedEpoch_(0),
      cmdSubId_(-1), notifySubId_(-1),
      cmdSubscribed_(false), notifySubscribed_(false), cmdHead_(0),
      cmdCount_(0), statePending_(false) {
  state_[0] = 0;
}

HaMqttTransport::~HaMqttTransport() { stop(); }

bool HaMqttTransport::start(const HaMqttOptions& options) {
  if (client_) return false;
  if (!options.brokerAclAudited || !dnsHostname(options.hostname) ||
      !boundedNonempty(options.caPem, 8192) ||
      std::strstr(options.caPem, "-----BEGIN CERTIFICATE-----") != options.caPem ||
      !std::strstr(options.caPem, "-----END CERTIFICATE-----") ||
      !boundedNonempty(options.username, 64) ||
      !boundedNonempty(options.password, 128) ||
      !boundedNonempty(options.clientId, 48) ||
      std::time(nullptr) < kMinimumTlsTime) return false;
  if (std::strchr(options.clientId, '/') ||
      !bridge_.configure(options.deviceId, true)) return false;
  if (options.approvedRecipientCount != 0 &&
      !bridge_.configureRecipients(options.approvedRecipients,
                                   options.approvedRecipientCount)) {
    bridge_ = HomeAssistantBridge();
    return false;
  }
  if (options.approvedStationHostCount != 0 &&
      !bridge_.configureStationHosts(options.approvedStationHosts,
                                     options.approvedStationHostCount)) {
    bridge_ = HomeAssistantBridge();
    return false;
  }

  rx_ = xQueueCreate(kRxCapacity, sizeof(Inbound));
  if (!rx_) { bridge_.configure(options.deviceId, false); return false; }
  esp_mqtt_client_config_t config = {};
  config.broker.address.hostname = options.hostname;
  config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
  config.broker.address.port = 8883;
  config.broker.verification.certificate = options.caPem;
  config.broker.verification.skip_cert_common_name_check = false;
  // NULL common_name makes esp-tls verify the actual requested hostname.
  config.credentials.username = options.username;
  config.credentials.client_id = options.clientId;
  config.credentials.authentication.password = options.password;
  config.session.keepalive = 30;
  config.session.disable_clean_session = false;
  config.network.timeout_ms = 2000;
  config.network.reconnect_timeout_ms = 5000;
  config.buffer.size = 512;
  config.buffer.out_size = 512;
  config.outbox.limit = 1024;
  client_ = esp_mqtt_client_init(&config);
  if (!client_) { stop(); return false; }
  if (esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, eventThunk, this) != ESP_OK ||
      esp_mqtt_client_start(client_) != ESP_OK) {
    stop();
    return false;
  }
  return true;
}

void HaMqttTransport::stop() {
  ready_.store(false);
  connected_.store(false);
  if (client_) {
    // Caller must call stop() only from the network control task.
    esp_mqtt_client_stop(client_);
    esp_mqtt_client_destroy(client_);
    client_ = nullptr;
  }
  if (rx_) { vQueueDelete(rx_); rx_ = nullptr; }
  bridge_ = HomeAssistantBridge();
  cmdCount_ = cmdHead_ = 0;
  statePending_ = false;
  std::memset(state_, 0, sizeof(state_));
  cmdSubscribed_ = notifySubscribed_ = false;
  cmdSubId_ = notifySubId_ = -1;
  observedEpoch_ = sessionEpoch_.load();
  noticePolicy_.clear();
}

void HaMqttTransport::eventThunk(void* arg, esp_event_base_t, int32_t, void* data) {
  if (arg && data) static_cast<HaMqttTransport*>(arg)->onEvent(static_cast<esp_mqtt_event_handle_t>(data));
}

void HaMqttTransport::onEvent(esp_mqtt_event_handle_t event) {
  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      connected_.store(true);
      ready_.store(false);
      sessionEpoch_.fetch_add(1);
      cmdSubscribed_ = notifySubscribed_ = false;
      // SUBSCRIBE requests are sent from esp-mqtt's event task, never UI.
      cmdSubId_ = esp_mqtt_client_subscribe_single(client_, bridge_.commandTopic(), 0);
      notifySubId_ = esp_mqtt_client_subscribe_single(client_, bridge_.notifyTopic(), 0);
      break;
    case MQTT_EVENT_SUBSCRIBED:
      if (event->msg_id >= 0 && event->msg_id == cmdSubId_) cmdSubscribed_ = true;
      if (event->msg_id >= 0 && event->msg_id == notifySubId_) notifySubscribed_ = true;
      // Subscription acknowledgement is NOT proof of broker-side topic ACL.
      if (connected_.load() && cmdSubId_ >= 0 && notifySubId_ >= 0 &&
          cmdSubscribed_ && notifySubscribed_) ready_.store(true);
      break;
    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_ERROR:
      connected_.store(false);
      ready_.store(false);
      sessionEpoch_.fetch_add(1);
      break;
    case MQTT_EVENT_DATA: {
      if (!ready_.load() || !rx_ || !event->topic || !event->data ||
          event->retain || event->dup || event->qos != 0 ||
          event->current_data_offset != 0 || event->data_len <= 0 ||
          event->total_data_len != event->data_len ||
          event->data_len > kNoticeMax || event->topic_len <= 0 ||
          event->topic_len >= kTopicMax) {
        dropped_.fetch_add(1);
        break;
      }
      Inbound incoming = {};
      const char* commandTopic = bridge_.commandTopic();
      const char* notifyTopic = bridge_.notifyTopic();
      if (event->topic_len == int(std::strlen(commandTopic)) &&
          std::memcmp(event->topic, commandTopic, event->topic_len) == 0 &&
          event->data_len <= 32) incoming.channel = 1;
      else if (event->topic_len == int(std::strlen(notifyTopic)) &&
          std::memcmp(event->topic, notifyTopic, event->topic_len) == 0)
        incoming.channel = 2;
      if (!incoming.channel) { dropped_.fetch_add(1); break; }
      incoming.length = static_cast<uint8_t>(event->data_len);
      incoming.epoch = sessionEpoch_.load();
      std::memcpy(incoming.payload, event->data, incoming.length);
      if (xQueueSend(rx_, &incoming, 0) != pdTRUE) dropped_.fetch_add(1);
      break;
    }
    default: break;
  }
}

void HaMqttTransport::service() {
  if (!client_ || !rx_) return;
  if (observedEpoch_ != sessionEpoch_.load()) {
    // Commands/notices from a prior TLS session are never replayed.
    cmdHead_ = cmdCount_ = 0;
    bridge_.clear();
    observedEpoch_ = sessionEpoch_.load();
  }
  VerifiedSession verified;
  // esp-mqtt secure TLS certificate and hostname validation, MQTT CONNECT
  // authentication, and independently audited broker topic ACLs are all
  // prerequisites of start(). ready_ is only true after both SUBACKs.
  verified.tlsPeerVerified = verified.hostnameVerified = true;
  verified.clientAuthenticated = verified.commandAclVerified = true;
  Inbound incoming;
  for (unsigned i = 0; i < kRxCapacity && xQueueReceive(rx_, &incoming, 0) == pdTRUE; ++i) {
    if (!ready_.load() || incoming.epoch != sessionEpoch_.load()) continue;
    if (incoming.channel == 1) {
      Command command;
      if (!bridge_.decodeCommand(bridge_.commandTopic(),
          reinterpret_cast<const uint8_t*>(incoming.payload), incoming.length,
          false, 0, verified, command)) { dropped_.fetch_add(1); continue; }
      if (cmdCount_ >= kCommands) { dropped_.fetch_add(1); continue; }
      commands_[(cmdHead_ + cmdCount_) % kCommands] = command;
      ++cmdCount_;
    } else if (!bridge_.receiveNotice(bridge_.notifyTopic(),
               reinterpret_cast<const uint8_t*>(incoming.payload), incoming.length,
               false, 0, verified)) dropped_.fetch_add(1);
  }
  if (ready_.load() && statePending_) {
    const int result = esp_mqtt_client_enqueue(client_, bridge_.stateTopic(),
                                               state_, int(std::strlen(state_)), 0, 0, true);
    if (result >= 0) statePending_ = false;
  }
}

bool HaMqttTransport::queueState(const char* status, uint8_t volume, const char* title) {
  // Call only from the same network control task as service(). No socket I/O.
  char bounded[kPayloadMax + 1] = {};
  if (!bridge_.encodeState(status, volume, title, bounded, sizeof(bounded))) return false;
  std::memcpy(state_, bounded, std::strlen(bounded) + 1);
  statePending_ = true;  // Latest state wins; never accumulate stale states.
  return true;
}

bool HaMqttTransport::queueForward(const char* recipient, const char* url,
                                   uint32_t nowMs) {
  if (!client_ || !ready_.load()) return false;
  const std::time_t issuedAt = std::time(nullptr);
  if (issuedAt < kMinimumTlsTime) return false;
  VerifiedSession verified;
  // This declaration relies on the opt-in TLS+MQTT config and externally
  // audited broker publish/subscribe ACL, not on user-supplied MQTT payloads.
  verified.tlsPeerVerified = verified.hostnameVerified = true;
  verified.clientAuthenticated = verified.commandAclVerified = true;
  char payload[kPayloadMax + 1] = {};
  if (!bridge_.encodeForward(recipient, url, nowMs,
                             static_cast<uint64_t>(issuedAt), verified,
                             payload, sizeof(payload))) return false;
  // ESP-IDF enqueue is outbox-only; no socket writes in the caller task.
  // QoS 0, non-retained and bounded global outbox. Forward is never retried
  // by this module; MQTT library may retain queued QoS0 until disconnection.
  return esp_mqtt_client_enqueue(client_, bridge_.forwardTopic(), payload,
                                int(std::strlen(payload)), 0, 0, true) >= 0;
}

bool HaMqttTransport::popCommand(Command& command) {
  command = Command();
  if (observedEpoch_ != sessionEpoch_.load()) {
    cmdHead_ = cmdCount_ = 0;
    bridge_.clear();
    observedEpoch_ = sessionEpoch_.load();
    return false;
  }
  if (!ready_.load() || !cmdCount_) return false;
  command = commands_[cmdHead_];
  cmdHead_ = (cmdHead_ + 1) % kCommands;
  --cmdCount_;
  return true;
}

bool HaMqttTransport::popNotice(char* dst, size_t capacity) {
  if (observedEpoch_ != sessionEpoch_.load()) {
    cmdHead_ = cmdCount_ = 0;
    bridge_.clear();
    observedEpoch_ = sessionEpoch_.load();
    return false;
  }
  return ready_.load() && bridge_.popNotice(dst, capacity);
}

bool HaMqttTransport::popNoticeWithPolicy(char* dst, size_t capacity,
                                          uint32_t nowMs, bool muted,
                                          NoticePolicy& decision) {
  decision = NoticePolicy();
  // Consume exactly one queued plain-text notice; excess bursts are dropped,
  // not deferred for playback after the user has stopped expecting them.
  if (!popNotice(dst, capacity)) return false;
  if (!noticePolicy_.decide(nowMs, muted, decision)) {
    if (dst && capacity) dst[0] = 0;
    dropped_.fetch_add(1);
    return false;
  }
  return true;
}

}} // namespace pocketjazz::ha
#endif

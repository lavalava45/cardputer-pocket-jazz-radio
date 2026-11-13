// SPDX-License-Identifier: MIT
#pragma once
// Experimental, opt-in ESP-IDF esp-mqtt transport for M5Stack ESP32 core 3.3.9.
// Define POCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT to compile the transport.
// No application integration, broker credentials, or certificate is supplied.
#if defined(ARDUINO_ARCH_ESP32) && defined(POCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT)
#include "HomeAssistantBridge.h"
#include <atomic>
#include <cstdint>
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace pocketjazz { namespace ha {

// All pointers must remain valid until stop(); never persist/log credentials.
// Caller must independently audit broker ACLs: this protocol cannot prove ACL
// policy via SUBACK. Provision distinct MQTT credentials with subscribe to
// /cmd and /notify and publish to /state and /notice, no wildcard privileges.
struct HaMqttOptions {
  const char* hostname;
  const char* caPem;
  const char* username;
  const char* password;
  const char* clientId;
  const char* deviceId;
  const char* const* approvedRecipients;
  size_t approvedRecipientCount;
  const char* const* approvedStationHosts;
  size_t approvedStationHostCount;
  bool brokerAclAudited;
  HaMqttOptions() : hostname(nullptr), caPem(nullptr), username(nullptr),
      password(nullptr), clientId(nullptr), deviceId(nullptr),
      approvedRecipients(nullptr), approvedRecipientCount(0),
      approvedStationHosts(nullptr), approvedStationHostCount(0),
      brokerAclAudited(false) {}
};

class HaMqttTransport {
 public:
  HaMqttTransport();
  ~HaMqttTransport();
  HaMqttTransport(const HaMqttTransport&) = delete;
  HaMqttTransport& operator=(const HaMqttTransport&) = delete;

  // Invoke start/stop/service ONLY on a dedicated networking control task;
  // never on decoder/render loops. start initiates asynchronous MQTT connect.
  bool start(const HaMqttOptions& options);
  void stop();
  void service();  // Drain <=4 inbound MQTT messages; queue <=1 state/outbox.

  // Local, bounded, non-network methods; no credentials in state or commands.
  bool queueState(const char* status, uint8_t volume, const char* title);
  // Only from dedicated network task. Queues at most one bounded forward
  // attempt; HA maps approved logical recipient to a hard-coded entity ID.
  bool queueForward(const char* approvedRecipient, const char* httpsStationUrl,
                    uint32_t nowMs);
  bool popCommand(Command& command);
  bool popNotice(char* dst, size_t capacity);
  // Audio duck is a *suggestion* for an external renderer, never applied here.
  bool popNoticeWithPolicy(char* dst, size_t capacity, uint32_t nowMs,
                           bool audioMuted, NoticePolicy& decision);
  bool ready() const { return ready_.load(); }
  uint32_t droppedPackets() const { return dropped_.load(); }

 private:
  enum { kRxCapacity = 4, kCommands = 4 };
  struct Inbound {
    uint8_t channel;  // 1: command, 2: notice
    uint8_t length;
    uint32_t epoch;
    char payload[kNoticeMax + 1];
  };
  static void eventThunk(void* arg, esp_event_base_t base, int32_t eventId, void* data);
  void onEvent(esp_mqtt_event_handle_t event);
  HomeAssistantBridge bridge_;
  esp_mqtt_client_handle_t client_;
  QueueHandle_t rx_;
  std::atomic<bool> connected_, ready_;
  std::atomic<uint32_t> dropped_;
  std::atomic<uint32_t> sessionEpoch_;
  uint32_t observedEpoch_;
  int cmdSubId_, notifySubId_;
  bool cmdSubscribed_, notifySubscribed_;
  Command commands_[kCommands];
  uint8_t cmdHead_, cmdCount_;
  char state_[kPayloadMax + 1];
  bool statePending_;
  NoticeRatePolicy noticePolicy_;
};
}} // namespace pocketjazz::ha
#endif

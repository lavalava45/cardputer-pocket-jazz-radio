// Bounded two-profile Wi-Fi selection policy. The actual NVS transactional
// double bank and Wi-Fi association live in the UI task, not the audio task.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pocketjazz {

struct WifiProfile {
  char ssid[33]{};      // SSID is at most 32 bytes, possibly UTF-8
  char password[64]{};  // WPA2 passphrase 8..63 bytes; empty = open network
};

struct WifiScanCandidate {
  const char* ssid;
  int rssi;
};

class WifiProfilePolicy {
 public:
  static constexpr int kCapacity = 2;
  static bool valid(const char* ssid, const char* password) {
    if (!ssid || !password) return false;
    const size_t ssidLength = boundedLength(ssid, 33);
    const size_t passLength = boundedLength(password, 64);
    if (!ssidLength || ssidLength > 32 || passLength > 63 ||
        (passLength != 0 && passLength < 8)) return false;
    for (size_t i = 0; i < ssidLength; ++i)
      if (static_cast<unsigned char>(ssid[i]) < 0x20) return false;
    for (size_t i = 0; i < passLength; ++i)
      if (static_cast<unsigned char>(password[i]) < 0x20 ||
          static_cast<unsigned char>(password[i]) > 0x7e) return false;
    return true;
  }

  bool put(const char* ssid, const char* password) {
    if (!valid(ssid, password)) return false;
    int index = find(ssid);
    if (index < 0) {
      for (int i = 0; i < kCapacity; ++i)
        if (!profiles_[i].ssid[0]) { index = i; break; }
    }
    if (index < 0) return false; // never silently evict saved credentials
    WifiProfile temp{};
    std::memcpy(temp.ssid, ssid, std::strlen(ssid) + 1);
    std::memcpy(temp.password, password, std::strlen(password) + 1);
    profiles_[index] = temp;
    return true;
  }

  int find(const char* ssid) const {
    if (!ssid) return -1;
    for (int i = 0; i < kCapacity; ++i)
      if (profiles_[i].ssid[0] && !std::strcmp(profiles_[i].ssid, ssid)) return i;
    return -1;
  }

  const WifiProfile* at(int index) const {
    return index >= 0 && index < kCapacity && profiles_[index].ssid[0]
        ? &profiles_[index] : nullptr;
  }

  int count() const {
    int n = 0;
    for (const auto& p : profiles_) if (p.ssid[0]) ++n;
    return n;
  }

  void clear() { for (auto& p : profiles_) p = WifiProfile{}; }

  // Only scans while disconnected. Pick strongest known network, ignoring a
  // just-failed slot if another configured network is available. Never try a
  // previously unknown SSID, and never guess a password from scan metadata.
  int choose(const WifiScanCandidate* networks, size_t count,
             int avoid = -1) const {
    if (!networks) return -1;
    int best = -1;
    int bestRssi = -200;
    const size_t bounded = count > 15 ? 15 : count;
    for (size_t n = 0; n < bounded; ++n) {
      const int slot = find(networks[n].ssid);
      if (slot < 0 || slot == avoid) continue;
      if (networks[n].rssi > bestRssi) {
        bestRssi = networks[n].rssi;
        best = slot;
      }
    }
    if (best >= 0 || avoid < 0) return best;
    // One visible known network may be the only viable option on later retry.
    for (size_t n = 0; n < bounded; ++n)
      if (find(networks[n].ssid) == avoid) return avoid;
    return -1;
  }

 private:
  WifiProfile profiles_[kCapacity]{};
  static size_t boundedLength(const char* s, size_t max) {
    size_t n = 0;
    while (n < max && s[n]) ++n;
    return n;
  }
};

} // namespace pocketjazz

// Compile-only Cardputer ADV crypto backend probe; no radio operations / flash.
// The public all-different-byte test key is NOT a deployment secret.
#include "../../experiments/StationExchangeHmac.h"
using pocketjazz::exchange::secure::PeerHmacSha256;
using pocketjazz::exchange::secure::PeerId;
static_assert(PeerHmacSha256::kKeyBytes == 32, "HMAC requires 256-bit key");
static volatile bool hmacProbeVerified = false;
void setup() {
  PeerHmacSha256 auth;
  const PeerId remote{{0x02,0x11,0x22,0x33,0x44,0x55}};
  uint8_t publiclyKnownTestKey[32];
  for (unsigned i=0;i<32;++i) publiclyKnownTestKey[i]=uint8_t(i);
  const uint8_t message[] = {'P','J','S','2','-','t','e','s','t'};
  uint8_t signature[pocketjazz::exchange::secure::kTagBytes] = {};
  if (auth.load(remote, publiclyKnownTestKey, sizeof(publiclyKnownTestKey)) &&
      auth.signTag(message, sizeof(message), signature)) {
    // Keep real mbedtls_md_hmac()/SHA256 code in the ESP32 link; compile-only
    // still does not establish on-device cryptographic correctness.
    hmacProbeVerified = auth.verifyTag(message, sizeof(message), signature);
  }
  auth.clear();
}
void loop() {}

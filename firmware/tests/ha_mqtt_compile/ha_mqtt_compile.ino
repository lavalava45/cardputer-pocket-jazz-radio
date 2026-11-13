// Opt-in compile-only transport probe. Never flashes or connects to a broker.
// Build with -DPOCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT using M5Stack ESP32 3.3.9.
#include "../../PocketJazzRadio/HaMqttTransport.h"
#include "../../PocketJazzRadio/HaMqttTransport.cpp"

#if defined(POCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT)
pocketjazz::ha::HaMqttTransport transport;
#endif

void setup() {}
void loop() {}

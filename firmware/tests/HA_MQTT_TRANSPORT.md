# Experimental opt-in Home Assistant MQTT transport (standalone)

`PocketJazzRadio/HaMqttTransport.h/.cpp` uses the **installed** M5Stack
`m5stack:esp32` 3.3.9 core, which bundles ESP-IDF `esp-mqtt` (ESP32-S3).
No third-party Arduino MQTT library was installed in this project's
`arduino-sketchbook/libraries` when checked on 2026-11-10.

This transport is **disabled by default**. Build-time opt-in requires
`POCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT`. The app does not construct or run it.
The optional `data/ha-mqtt.example.txt` is **not loaded**; do not put passwords
in that example, on the SD card, in the firmware, or in logs.

## Configuration and security contract

The calling application must supply in-memory, lifetime-stable:

- A **DNS hostname** (not raw IP/URL), server CA PEM, username, password,
  unique MQTT client ID and bounded device ID.
- An independently **audited** MQTT broker ACL (opt-in `brokerAclAudited`):
  this device may *subscribe only* to `pocketjazz/<id>/cmd` and
  `pocketjazz/<id>/notify`, and *publish only* to
  `pocketjazz/<id>/state`, `pocketjazz/<id>/forward`, and optionally
  `pocketjazz/<id>/notice`.
  Broker must deny anonymous connections and unauthorized topic access.
  `brokerAclAudited=true` is **an operator declaration, not ACL verification**;
  SUBACK alone does not prove the ACL prohibits other topics.
- Valid synchronized wall-clock time before TLS handshake; secure credential
  provisioning outside this module. No credential storage, NVS writes, file
  writes or password logging is supplied.

Transport uses `MQTT_TRANSPORT_OVER_SSL`/port 8883, explicit CA verification,
`skip_cert_common_name_check=false`, and broker-hostname verification using
the configured host. No plaintext fallback, `setInsecure`, wildcard MQTT
subscriptions, retained inbound commands, QoS1/2 command execution, or
anonymous MQTT connection. MQTT CONNECT acceptance authenticates the user;
the broker itself enforces the provisioned ACL.

The transport runs the ESP-IDF MQTT task asynchronously. `start()`, `stop()` and
`service()` must be called only by a **dedicated networking control task**;
never call them from the UI or audio decoder. `queueState()` stores one
bounded, overwriteable state update and `service()` enqueues it to the bounded
ESP-IDF outbox. Commands and notifications arrive through a four-element
FreeRTOS queue (payload <=120 bytes); overflow is dropped and counted.
Inbound fragmented MQTT messages are rejected, as are retained, duplicated,
unexpected topics and oversized packets. In-memory commands/notices from
earlier disconnected sessions are discarded. Connection and TCP timeouts are
bounded (2 s network timeout, 5 s reconnect, 30 s keepalive); they execute
inside MQTT/network tasks, not in render/decoder callbacks.

## Station URL forwarding (v0.8+ proposal, no radio integration)

`queueForward(recipient, url, monotonicMs)` submits **only** to
`pocketjazz/<id>/forward` (QoS 0, non-retained). It is valid exclusively from
the dedicated network task when the MQTT session is ready; it never calls HA
REST API or accepts an HA admin token. Each `HaMqttOptions` must include
`approvedRecipients` and `approvedRecipientCount` (1–4 human-approved
**logical aliases**, e.g. `living_room`), and `approvedStationHosts` and
`approvedStationHostCount` (1–4 manually approved HTTPS station DNS names,
e.g. `radio.example.org`). Missing either whitelist makes forwarding fail
closed without affecting other MQTT functions. No arbitrary `entity_id` can
be supplied to the Cardputer and no arbitrary URL host is accepted. Both
lists are copied into fixed buffers at `start()`. Recipients and hosts may
only be reconfigured by stopping and restarting the standalone transport.

The bounded payload is, for example:

```json
{"recipient":"living_room","url":"https://radio.example.org/live.mp3","issued_at":1794280000}
```

URL rules: HTTPS only, DNS hostname matching an exact allowlisted hostname,
no username/password in URL, ports, fragments, controls, whitespace, JSON
metacharacters or backslashes; NUL within a **192-byte URL field** (at most
191 non-NUL bytes); total JSON <=256 bytes. v0.7e source hardening similarly
requires NUL inside fixed recipient/hostname/title capacities rather than
reading one byte beyond a non-terminated input. A Windows `PAGE_NOACCESS`
guard-page test covers the boundary; `msvc` host success is not a broker test.
There is a 10-second monotonic minimum interval between attempts, across all
recipients. Each message carries numeric `issued_at` UTC seconds derived from
the network task's validated wall clock; the host contract rejects dates
outside 2024–2100. This is a forwarding request, **not proof of playback** or
a Home Assistant media player state. A queued QoS0 packet can still be in the
ESP-IDF outbox when the connection changes, so the HA automation below drops
messages older than 30 seconds or more than 5 seconds into the future. Exact
replay deduplication still requires separate broker-side/HA state and a nonce.

Sample Home Assistant `automations.yaml` entry (replace `cardputer_1`,
`radio.example.org` and `media_player.REPLACE_ME` with explicitly approved
values; the receiving broker account must have read permission for this
single topic). MQTT broker authentication and TLS credentials are configured
separately in Home Assistant and the device, never in this snippet:

```yaml
- id: pocketjazz_explicit_forward_living_room
  alias: Pocket Jazz - approved living room station forward
  triggers:
    - trigger: mqtt
      topic: pocketjazz/cardputer_1/forward
  conditions:
    - condition: template
      value_template: >-
        {% set raw = trigger.payload_json if trigger.payload_json is defined else {} %}
        {% set p = raw if raw is mapping else {} %}
        {% set age = as_timestamp(now()) - (p.get('issued_at', 0) | int(0)) %}
        {{ p is mapping and p.get('recipient') == 'living_room'
           and p.get('url', '') is string
           and p.get('url', '').startswith('https://radio.example.org/')
           and (p.get('url', '') | length) <= 192
           and age >= -5 and age <= 30 }}
  actions:
    - action: media_player.play_media
      target:
        entity_id: media_player.REPLACE_ME
      data:
        media_content_id: "{{ trigger.payload_json.url }}"
        media_content_type: music
  mode: single
```

For actual deployment, duplicate the automation with a *fixed*, separately
approved entity ID for each alias and add HA-side replay deduplication and
input throttles. HA automations must not take `entity_id` dynamically
from MQTT. Even with allowlisted URLs, keep Home Assistant's permissions and
network isolation in place; HTTPS domain checks alone do not prove the
streaming server's eventual redirect address is safe.

## Inbound notices, rate and muted-audio policy

Incoming `pocketjazz/<id>/notify` accepts only authenticated QoS0,
non-retained, non-duplicated, unfragmented UTF-8 plain text (1–120 bytes).
The inbound queue holds at most four messages; overflow drops and counts.
`popNoticeWithPolicy(..., monotonicMs, audioMuted, decision)` consumes one
notice. It permits **at most one display every 15 seconds** and discards
extra burst messages without holding them for later playback. With
`audioMuted=true`, the returned policy specifies `duckPercent=0` and
`durationMs=0`. With audio unmuted it suggests a **maximum 30% volume duck
for 1000 ms**. These are *host-only instructions*, not changes to actual
audio routing; no audio state, NVS or settings are written by the bridge.
All transport/decoder/UI interactions must be integrated and bench-tested
separately before use on the Cardputer.

## Validation

From `apps/pocket-jazz-radio`:

```powershell
python -m unittest tests/test_ha_mqtt_protocol.py -v
python -m unittest discover -s tests -p 'test_ha_*.py' -v
../../../tools/arduino-cli/arduino-cli.exe compile --config-file arduino-cli.yaml --fqbn m5stack:esp32:m5stack_cardputer --build-path tests/.ha-mqtt-build --build-property compiler.cpp.extra_flags=-DPOCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT tests/ha_mqtt_compile
```

The Arduino sketch is compile-only and has empty `setup()`/`loop()`. The
Python test compiles/exercises C++11 assertions using WSL GCC when Windows
has no host compiler in PATH. No broker integration or on-device Wi-Fi/TLS
handshake has been tested. This module is **not an integrated HA media_player
entity**; discovery/entity mapping, command application, secret provisioning,
broker ACL testing, reconnection behavior on hardware, and real MQTT
interoperability remain integration work.

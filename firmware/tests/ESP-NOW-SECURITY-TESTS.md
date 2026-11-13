# ESP-NOW Station Exchange — host-only authenticated protocol experiment

**Status (2026-11-10):** `experiments/StationExchangeSecure.h` provides a **C++11, no-heap, host-only** state machine wrapping existing `experiments/StationExchangePacket.h`. It does **not** include an ESP-NOW/Wi-Fi driver, cryptographic key exchange, key provisioning, hardware pairing, app UI, station playback, NVS or SD writes. Original `StationExchangePacket.h` was not changed. **CRC32 is not authentication. No v0.8 device-readiness claim.**

## Trust boundary and authenticated wire format

The base PJST v1 packet has name (max 32), stream URL (max 191), nonce and CRC32. The new authenticated envelope binds **every byte** of PJST (including CRC), session epoch, sender identity, receiver identity, version and length to an **externally supplied 16-byte MAC**. Transport remains capped at the legacy **250-byte** ESP-NOW payload; secure framing costs 42 bytes, leaving at most **208 bytes** for the entire embedded PJST. Sending both maximum name and maximum URL must fail, never truncate.

| Field | Bytes |
| --- | ---: |
| Magic `PJS2` | 4 |
| Unique 64-bit session epoch (little endian) | 8 |
| Sender unicast MAC identifier | 6 |
| Recipient unicast MAC identifier | 6 |
| Embedded PJST byte count (little endian) | 2 |
| PJST v1 Offer / ACK, CRC included | 16–208 |
| External MAC tag | 16 |

`Authenticator` injects `sign(context, recipientPeer, fullUnsignedEnvelope, length, tag[16])` and `verify(context, senderPeer, fullUnsignedEnvelope, length, receivedTag[16])`. A future integration **must supply an audited cryptographic implementation**, e.g. peer-specific HMAC-SHA256 truncated to 128 bits, with constant-time tag verification and external, authenticated key provisioning. **No key, password, token or home-grown MAC is stored in firmware by this component.** A nonnull verifier alone does not prove its implementation is secure. ESP-NOW MAC addresses, CRC32 and the supplied session number are *not* proof of identity.

`unframe` rejects invalid lengths (including the >250-byte boundary), wrong magic/peer/session, malformed PJST, tampered payload/tag and zero nonce. It authenticates the **whole** envelope before accepting/decode of PJST. Base `validOfferUrl` checks syntax and some obvious private URLs; it is **not** a full SSRF/DNS/redirect security boundary. Received URLs must never be opened or persisted automatically.

## Explicit pairing and replay model

`StationExchangeSecure::proposePair(localPeer, remotePeer, session, nowMs, auth)` merely creates a *pending local pairing decision*; it is **NOT authenticated key exchange**. Pairing MUST be initiated through a separate trusted channel, show a verified peer fingerprint and obtain human consent. `confirmPair(nowMs, true)` activates this explicitly provisioned single-peer context; proposal expires after 30 s; a denied/expired proposal does not pair. Unicast peer IDs must be different and nonzero; external session epoch must be nonzero, unpredictable, **fresh after every boot/re-pair** and cryptographically bound to pairwise keys. Reusing a prior epoch permits captured traffic replay after restart: this API does not persist a replay database.

Only one outgoing pending Offer, one inbound pending human prompt, and one previously accepted inbound Offer are retained. Every outgoing Offer uses a monotonically increasing nonzero 32-bit nonce; new transmissions are denied at wrap until a new session. Inbound nonces must strictly increase; exact duplicates of a **human-accepted and authenticated** Offer may yield `AckReady` but **never another UI prompt**. At most **four** ACK transmissions per accepted Offer (initial ACK plus three duplicates) prevent unlimited ACK amplification after captured replays. Rejected, stale, mutated and unconfirmed duplicates are rejected. `reset()` invalidates session and clears pending/replay state.

`offer(name, nameLength, url, urlLength, nowMs, output, capacity, used)` accepts explicit input lengths, rejects embedded NUL, control characters, secret-bearing/private literal URLs and overflow; never emits a partially authenticated packet on failure. `retry(nowMs, ...)` returns **exactly the original signed bytes**, at most 3 retransmissions after the initial send with a minimum 700 ms spacing and 5000 ms absolute pending window; expired ACK is ignored. `receive(frame, length, nowMs, received)` returns `Rejected`, `NoAction`, `PresentOffer`, `AckReady` or `DeliveryConfirmed`. Caller may read `received` **only on PresentOffer**, then must request user consent for this specific URL through `decideIncoming(accepted, nowMs)`; human decision expires after 30 s. `makeAck` is available only after positive consent. Replayed ACK cannot trigger another delivery.

The transport/crypto callbacks have **no synchronization guarantees** in this standalone header; a future owner must serialize access and own key lifetimes. It does not defend against RF flooding of unverifiable frames or ensure ESP-NOW radio privacy; those require Wi-Fi/ESP-NOW stack integration and rate limits *before* expensive verification.

## Exact host test

`tests/station_exchange_secure_test.cpp` uses an intentionally **noncryptographic deterministic fake tag** solely to exercise verifier/signature callbacks and error handling. This is **NOT an HMAC/cryptographic security test**. Its Release `assert` statements remain active via `#undef NDEBUG`.

MSVC Build Tools 2022 with `vcvars64.bat` already activated:

`cl /nologo /std:c++14 /EHsc /W4 /WX /O2 /DNDEBUG tests\station_exchange_secure_test.cpp /Fe:%TEMP%\pj_station_exchange_secure_test.exe`

`%TEMP%\pj_station_exchange_secure_test.exe`

MSVC has no explicit `/std:c++11` switch; this code uses C++11 language features but is compiled using MSVC's C++14 mode. Test covers: invalid/unconfirmed/expired pairing, wrong peer/key/session, tampering **every byte** of authenticated frame, short/overlength/null packets, invalid names/URLs, source length and MTU boundaries, duplicate/replayed/rejected/expired inbound Offers, human acceptance, ACK identity, bounded retries, stale ACK after deadline, signing/verification failures and reset/re-pair isolation.

**Host PASS does not prove:** HMAC quality or constant-time implementation; authenticated peer provisioning or key-storage security; actual RF packet delivery, pairing UX, Wi-Fi channel coexistence, memory/timing on ESP32, protection against denial-of-service or effects on audio underruns. Before any real integration require real cryptographic known-answer vectors, peer/fingerprint confirmation, independent key management, DNS/redirect checks, RF flood constraints, two Cardputer ADV devices, a no-auto-play rule and long MP3/Wi-Fi stress testing. No firmware build, flashing, SD write or release work occurred in this worker's assignment.

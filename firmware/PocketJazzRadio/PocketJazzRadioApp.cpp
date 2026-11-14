// Pocket Jazz Radio, a standalone M5Stack Cardputer ADV internet-radio sketch.
// Copyright (c) 2026 Pocket Jazz Radio contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// Board reference: https://docs.m5stack.com/en/core/Cardputer-Adv
// MP3 decode: ESP8266Audio 2.4.1; speaker adapter: M5Unified MIT example.
// No merged/full-flash image should be distributed for normal M5Launcher usage.
#include "PocketJazzRadioApp.h"
#include "BuildVersion.h"
#include "DiagnosticAudioCounterWindow.h"
#include "DiagnosticDisplayPolicy.h"
#include "RadioBufferLevelPolicy.h"
#include "AudioIntentPolicy.h"
#include "AudioHotPathTiming.h"
#include "AudioSourceProgress.h"
#include "HeapLifecycleProfile.h"
#include "Utf8BoundedText.h"
#include "CatalogUpdater.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <esp_sntp.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include "Mp3CodecWorkspace.h"
#include "AudioOutputM5Speaker.h"
#include "SecureICYSource.h"
#include "RadioTrustedRoots.h"
#include "PCMLevel.h"
#include "AudioPlayoutTelemetry.h"
#include "ShareStationQR.h"
#include "PrefillBudget.h"
#include "TapTempo.h"
#include "ClockSettings.h"
#include "TrackHistory.h"
#include "StationPeekPolicy.h"
#include "StationAltUrlPool.h"
#include "TuningSoundBed.h"
#include "JazzNotesSearch.h"
#include "JazzEncyclopedia.h"
#include "NotesResultWorkspace.h"
#include "JazzFacts.h"
#include "PreferenceGuards.h"
#include "WifiProfilePolicy.h"
#include "SettingsBackupPolicy.h"
#include "StationHealth.h"
#include "OfflineLibraryPolicy.h"
#include "DailyRadioAlarm.h"
#include "BoundedLog.h"
#include <atomic>
#include <memory>
#include <new>
#include <vector>
#include <cstring>

#if !defined(ARDUINO_USB_CDC_ON_BOOT) || ARDUINO_USB_CDC_ON_BOOT != 1
#error "USB CDC On Boot must be enabled: UART0 TX GPIO43 is ES8311 I2S LRCK on the ADV."
#endif
#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 1
#error "Use USB Mode: Hardware CDC and JTAG (no TinyUSB)."
#endif

namespace pocketjazz {
namespace {
constexpr int MAX_STATIONS = 64;
constexpr int MAX_LISTS = 12;
constexpr int MAX_NETWORKS = 15;
constexpr int PRESET_COUNT = 9;
constexpr int RECENT_COUNT = 5;
constexpr int SD_CS = 12;
constexpr int STREAM_BYTES = 32768;
// With all ADV UI/speaker tasks resident, the real v0.7z6 device reached
// only 8,608 bytes free (largest=1,332) while playing: mbedTLS may then be
// unable to receive another encrypted record. Preserve the 32-KiB ring when
// there is headroom; leave 16 KiB to the network stack on a memory-limited
// Cardputer. This is a memory mitigation, not proof of the first-stall cause.
constexpr int STREAM_BYTES_CONSTRAINED = 16384;
constexpr uint32_t STREAM_FULL_RING_MIN_HEAP = 180000;
constexpr int MP3_CODEC_BYTES = 29192;
static_assert(AudioGeneratorMP3::preAllocSize() == MP3_CODEC_BYTES,
              "ESP32 MP3 workspace size changed; review memory budget");
constexpr uint16_t C_BG = 0x10E4;       // midnight navy
constexpr uint16_t C_PANEL = 0x2188;
constexpr uint16_t C_PANEL2 = 0x2A4B;
constexpr uint16_t C_GOLD = 0xF5E9;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_MUTED = 0xA5D1;
constexpr uint16_t C_GREEN = 0x7EA6;
constexpr uint16_t C_RED = 0xE105;
constexpr uint32_t WIFI_TIMEOUT_MS = 16000;
constexpr time_t MIN_TLS_TIME = 1704067200; // 2024-01-01 UTC; cert validation requires a clock.
constexpr uint32_t WIFI_RETRY_MS = 16000;
constexpr uint32_t WIFI_STABLE_MS = 2000;
constexpr uint32_t PREF_SAVE_DEBOUNCE_MS = 2500;
// Opening a stream (DNS/TCP/TLS/HTTP) has its own bounded timeouts in the
// HTTP client. Never subtract that time from the MP3-buffer priming deadline.
constexpr uint32_t MP3_PREFILL_TIMEOUT_MS = 5000;
constexpr uint32_t SPARK_AUTO_INTERVAL_MS = 12u * 60u * 1000u;
constexpr uint32_t SPARK_AUTO_VISIBLE_MS = 10000u;

struct Station {
  char name[56];
  char url[192];
  uint16_t altOffset = pocketjazz::StationAltUrlPool::kNone;
  int8_t moodEnergy = -1;   // 0..4; -1 means no subjective mood label
  int8_t moodAcoustic = -1; // 0..4; never guess tags for arbitrary streams
};
struct Playlist {
  char name[36];
  uint16_t first = 0;
  uint16_t count = 0;
};
struct AudioCommand {
  bool play;
  bool offline; // explicit local MP3, never inferred from URL or Wi-Fi state
  uint8_t volume;
  uint8_t stationId;
  uint32_t telemetrySequence;
  char url[192];
  char altUrl[192];
};
struct SavedStation {
  char name[56];
  char url[192];
};
struct TrackMetadata {
  uint8_t stationId;
  char title[96];
};
enum AudioState : int { STOPPED, WAIT_WIFI, WAIT_TIME, CONNECTING, BUFFERING, PLAYING, ERROR_STREAM, ERROR_MEMORY };
enum Screen : uint8_t { RADIO, WIFI_LIST, WIFI_PASSWORD, WIFI_JOIN, HELP, DIAGNOSTICS, RECENTS, SEARCH, MOOD, NOTES, SHARE, TEMPO, CLOCK, SOUND_FX, TRACKS, BROWSE, NOTES_SEARCH, SPARK, SETTINGS_BACKUP, OFFLINE_LIBRARY, DAILY_ALARM, CATALOG_UPDATE };
void loadArtistNotes();
void searchOfflineNotes();
void pageOfflineNotes(size_t offset);
void openSelectedOfflineNote();
bool readNotesFile(void* context, size_t offset, uint8_t* dst, size_t length);
void showJazzSpark(int8_t direction = 0);
void showNotice(const char* message);
void drawCatalogUpdate();
void invalidateStationPeek(bool playlistReload);

Station stations[MAX_STATIONS]{};
pocketjazz::StationAltUrlPool stationAltUrls;
Playlist lists[MAX_LISTS]{};
int listCount = 0;
int stationCount = 0;
int currentList = 0;
int currentOffset = 0;
bool sdAvailable = false;
// Captured from *the actual SD reload*, not inferred from downloaded files.
// A mount failure or built-in fallback must never be reported as activation.
uint8_t catalogDefaultListsLoadedMask = 0;
uint8_t catalogDefaultStationCounts[5]{};
// Only the first successful card mount needs boot recovery, before any
// playlists are opened. Subsequent manual updates recover synchronously
// under an explicit STOP guard.
bool catalogRecoveryChecked = false;
pocketjazz::CatalogUpdateResult catalogUpdateStatus =
    pocketjazz::CatalogUpdateResult::NoChange;
bool catalogUpdateAttempted = false;
bool catalogUpdateInProgress = false;
// Manual local-only MP3 player: no OAuth, bundled music or background failover.
// A static bounded catalog is populated only when radio is explicitly STOPPED.
pocketjazz::OfflineLibraryPolicy<24> offlineCatalog;
int offlineSelected = 0;
char offlinePath[192]{};
bool offlineMode = false;
bool offlineLeaving = false;  // wait for audio task to release SD before RADIO
std::atomic<bool> offlineSourceOpen{false};
std::atomic<bool> offlineFinished{false};
Screen screen = RADIO;
uint8_t shareQrPage = 0; // 0=station, 1=last liked Spotify, 2=last liked web
String wifiSsid;
String wifiPassword;
String savedSsid;
String savedPassword;
pocketjazz::WifiProfilePolicy wifiProfiles;
int wifiActiveProfile = -1; // index in bounded RAM profile set
int wifiFailedProfile = -1;
bool wifiMigrationPending = false;
bool wifiMigrationAttempted = false;
bool futureWifiProfileFormat = false; // preserve unknown profile-bank revisions
bool wifiAutoScanPending = false;
uint32_t lastWifiAutoScanAt = 0;
SavedStation presets[PRESET_COUNT]{};
SavedStation recent[RECENT_COUNT]{};
int recentCount = 0;
int recentSelected = 0;
String stationSearch;
int searchSelected = 0;
String notesArtist;
String notesDetail;
String notesAlbum;
String notesSource;
bool notesFound = false;
String notesSearchQuery;
// Only one of these result formats is displayed at a time. Share one
// fixed page to avoid retaining two large SD-search buffers during TLS.
pocketjazz::NotesResultWorkspace notesSearchPage;
bool notesSearchEncyclopedia = false;
size_t notesSearchOffset = 0;
int notesDetailPage = 0;
bool jazzSparkReady = false;
pocketjazz::JazzFact jazzSpark{};
size_t jazzSparkIndex = 0;
size_t jazzSparkTotal = 0;
bool jazzSparkAutoEnabled = true;
bool jazzSparkAutoVisible = false;
uint32_t lastAutoSparkAt = 0;
uint32_t autoSparkUntil = 0;
size_t notesSearchCount = 0;
size_t notesSearchTotal = 0;
int notesSearchSelected = 0;
bool notesSearchRan = false;
bool notesOpenedFromSearch = false;
uint8_t moodEnergyCursor = 2;
uint8_t moodAcousticCursor = 2;
pocketjazz::TapTempo manualTempo;
char clockTz[80] = "UTC0";
bool clockConfigLoaded = false;
int clockOffsetMinutes = 0; // Positive = east of UTC, e.g. Israel +120.
bool clockUserOverride = false;
// User-facing daily alarm; a powered-on radio only, never RTC deep-sleep wake.
pocketjazz::DailyRadioAlarmConfig dailyAlarmConfig{};
pocketjazz::DailyRadioAlarmConfig dailyAlarmEdit{};
int32_t dailyAlarmLastFiredDay = -1;
bool dailyAlarmFutureFormat = false;
bool dailyAlarmSounding = false;
bool dailyAlarmRamping = false;
uint32_t dailyAlarmRequestedAt = 0;
uint32_t dailyAlarmRampAt = 0;
uint32_t dailyAlarmFramesAtRequest = 0;
uint8_t dailyAlarmOutputVolume = 0;
uint8_t dailyAlarmTargetVolume = 57;
uint32_t lastRecentAt = 0;
char lastRecentUrl[192] = {};
bool muted = false;
// Independent opt-in settings. Their atomics are read by audioWorker; NVS is
// accessed only from the UI task and never during a network/decode loop.
std::atomic<bool> vinylCrackleEnabled{false};
std::atomic<bool> tuningHissEnabled{false};
std::atomic<uint8_t> requestedOutputVolume{0};
// The single radio visualization is the live PCM VU meter. Unlike the
// former 64x64 RGB565 logo it needs no dedicated 8192-byte pixel cache.
// V toggles its visibility, never playback or MUTE (Enter/M own those).
// VU is permanently shown in the radio footer. Full-width text above it.
char currentTitle[96] = {};
// Session-only cache, updated ONLY after a successful K append to liked.txt.
// We never claim to have read the user's historical favorites from SD.
char lastLikedTitle[96] = {};
pocketjazz::TrackHistory<5> trackHistory;
// Off-channel ICY titles never enter the playing station's history. Fixed RAM,
// no network polling until the user explicitly opts in via a local SD config.
pocketjazz::StationPeekPolicy<6> stationPeek;
int browseSelected = 0;
int browseList = 0; // inspecting a playlist never retunes the active decoder
bool peekOptIn = false;
uint32_t peekUnderflowsAtOpen = 0;
char lastSelectedUrl[192] = {};
char lastRequestedAudioUrl[192] = {};
char lastRequestedAudioAltUrl[192] = {};
bool lastRequestedAudioPlay = false;
bool lastRequestedAudioOffline = false;
uint32_t telemetryNextSequence = 0;
bool autoPlay = true;
bool futureNvsSchema = false; // never overwrite a newer firmware's NVS format
bool ntpStarted = false;
std::atomic<bool> ntpSynced{false}; // Set ONLY by SNTP callback, not a plausible epoch.
bool safeMode = false;
char restoredStationUrl[192] = {};
uint8_t sleepMinutes = 0;
uint32_t sleepStartedAt = 0;
uint8_t helpPage = 0;
constexpr uint8_t HELP_PAGE_COUNT = 18;
constexpr uint8_t DIAG_PAGE_COUNT = 8;
uint8_t diagPage = 0;
uint8_t backupConfirmation = 0; // 0=none; 1=export; 2=import
// The backup workspace is NEVER permanently allocated. Two staged snapshots
// plus 8 KiB of wire storage formerly reserved ~13 KiB of internal DRAM even
// when the user never opened Backup. Allocate only while radio is STOPPED,
// free on all return paths, and never place this object on the UI stack.
struct SettingsBackupWorkspace {
  pocketjazz::SettingsBackupData stage{};
  pocketjazz::SettingsBackupData rollback{};
  char wire[pocketjazz::SettingsBackupPolicy::kMaxFileBytes]{};
};
bool screenDimmed = false;
uint32_t lastKeyActivity = 0;
int batteryLevel = -1;
int batteryMillivolts = -1;
uint32_t lastBatteryAt = 0;
uint32_t lastVuAt = 0;
uint8_t shownVu = 0;
int previousVu = -1;
uint32_t audioPlayingSince = 0;
uint32_t uiNoticeUntil = 0;
char uiNotice[32] = {};
String networkNames[MAX_NETWORKS];
int16_t networkRssi[MAX_NETWORKS]{};
int networkCount = 0;
int selectedNetwork = 0;
bool wifiScanPending = false;
bool wifiJoinPending = false;
bool wifiJoinFailed = false;
bool wifiShowPassword = false;
bool wantPlay = true;
int volume = 57;  // M5 Speaker range 0..255; deliberately conservative.
bool prefDirty = false;
uint32_t prefTouched = 0;
uint32_t wifiAttempt = 0;
uint32_t lastStatus = 0;
uint32_t lastVolume = 0;
uint32_t wifiFirstIpAt = 0;
uint32_t reportedIpEvents = 0;
uint32_t reportedDisconnects = 0;
uint32_t lastUiSignature = 0;
bool uiPainted = false;
Screen lastPaintedScreen = RADIO;
std::vector<char> lastWord;
bool lastEnter = false;
bool lastDelete = false;
bool lastTab = false;
bool lastArrowUp = false;
bool lastArrowDown = false;
bool lastArrowLeft = false;
bool lastArrowRight = false;

std::atomic<int> audioState{STOPPED};
// The UI publishes playback intent before its one-slot command mailbox. This
// allows an in-flight GET/prefill to discard a stale start even if the mailbox
// has not yet been written. Volume-only messages do not advance this value.
std::atomic<uint32_t> audioRequestedIntentSequence{0};
QueueHandle_t audioQueue = nullptr;  // one-slot mailbox; UI sends copied URLs, no SD race
QueueHandle_t metadataQueue = nullptr;  // copied ICY metadata; no UI String access from decoder task
std::atomic<uint32_t> audioFrames{0};
std::atomic<uint32_t> decodedPcmRateHz{0}; // actual M5 PCM output rate; not MP3 bitrate
std::atomic<uint32_t> decodedMp3FrameBps{0}; // decoded libmad MPEG header, never icy-br
std::atomic<bool> decodedMp3ObservedVbr{false}; // only true after varying valid frames
std::atomic<uint8_t> pcmPeak{0};
std::atomic<uint32_t> audioBufferedBytes{0};
// Actual ring capacity can be 16 or 32 KiB; diagnostics must never
// report 32 KiB for the smaller, memory-protective allocation.
std::atomic<uint32_t> audioRingCapacity{STREAM_BYTES};
std::atomic<uint32_t> audioBufferMinimum{0};
std::atomic<uint32_t> audioBufferMinAtMs{0};
std::atomic<uint32_t> uiLoopMaxGapMs{0};
// Maxima since Diagnostics X-reset. Audio task records durations without
// formatting, allocation or extra sockets; UI only loads the atomics.
std::atomic<uint32_t> audioSourcePullMaxUs{0};
std::atomic<uint32_t> audioDecodeLoopMaxUs{0};
std::atomic<uint32_t> audioPcmFlushMaxUs{0};
std::atomic<uint32_t> audioReopens{0};
std::atomic<uint32_t> audioUnderruns{0};
std::atomic<uint32_t> audioStackWatermark{0};
pocketjazz::AudioPlayoutTelemetry audioPlayout;
pocketjazz::HeapLifecycleProfile heapLifecycle; // ~0.5 KiB, audio writer, UI reader
pocketjazz::DiagnosticAudioCounterWindow diagAudioWindow; // UI task only
pocketjazz::StationHealth<MAX_STATIONS> stationHealth;
// Written only by the audio task; pcmMonitor is invoked synchronously by that
// task on a successful M5.Speaker PCM enqueue (not on actual acoustic output).
uint32_t audioAcceptedSequence = 0;
struct PeekCommand {
  uint8_t stationId = 0;
  uint32_t playlistGeneration = 0;
  uint32_t requestId = 0;
  char url[192]{};
};
struct PeekResult {
  uint8_t stationId = 0;
  uint32_t playlistGeneration = 0;
  uint32_t requestId = 0;
  char title[96]{};
};
QueueHandle_t peekQueue = nullptr;  // 1 request; serial probe, never fan-out
QueueHandle_t peekResultQueue = nullptr; // 1 completed result, UI only
std::atomic<bool> peekWanted{false};
// Generation advances before playlist data can be reused. Request advances on
// every probe, including after a browse screen is closed and reopened.
std::atomic<uint32_t> peekLiveGeneration{1};
std::atomic<uint32_t> peekLiveRequest{0};
uint32_t peekNextRequest = 0; // UI task only; 0 reserved for invalid
// Failure stages: 0=none, 1=stream/HTTP, 2=buffer object, 3=decoder init,
// 4=decoder loop, 5=contiguous MP3 ring allocation, 6=codec workspace,
// 7=source opened but no playable MP3 bytes before prefill deadline.
std::atomic<int> audioFailureStage{0};
std::atomic<int> audioSourceStatusCode{0};
// RAW NetworkClientSecure::lastError(). Unlike its name, Arduino-ESP32 3.3.9
// may store the POSITIVE socket descriptor after a successful handshake.
// Negative is a TLS failure, positive is NOT an error diagnosis.
std::atomic<int> audioTlsErrorCode{0};
// Last HTTPS attempt only: bounded numeric values, no hostname or URL logging.
std::atomic<uint32_t> audioHttpsPhase{0}, audioHttpsHop{0};
std::atomic<int> audioHttpsDnsOk{-1}, audioHttpsSecureOk{-1};
std::atomic<uint32_t> audioHttpsDnsMs{0}, audioHttpsSecureMs{0}, audioHttpsRequestMs{0};
std::atomic<uint32_t> audioHttpsFreeBefore{0}, audioHttpsLargestBefore{0};
std::atomic<uint32_t> audioHttpsFreeAfter{0}, audioHttpsLargestAfter{0};
std::atomic<uint32_t> audioLastOpenDurationMs{0};
std::atomic<bool> wifiReady{false};
std::atomic<uint32_t> wifiGotIpEvents{0};
std::atomic<uint32_t> wifiDisconnectEvents{0};
std::atomic<int> wifiDisconnectReason{-1};
std::atomic<uint32_t> wifiDisconnectAt{0};

// Wi-Fi callbacks run on the Arduino event task, not our UI task.
// Only write atomics here; log IP/RSSI and perform retries in loop().

// Independent UI observer. The audio task only publishes lock-free atomics.
std::atomic<uint32_t> audioLastPcmAt{0};
std::atomic<bool> audioInputWaiting{false};
pocketjazz::AudioRuntimeProbe audioRuntime;
pocketjazz::AudioRuntimeJournal audioJournal; // UI owner only
pocketjazz::AudioRuntimeSnapshot audioLive;
uint32_t audioFaultSeen = 0, audioFaultCoalesced = 0;
void observeAudioRuntime(uint32_t now) {
  static uint32_t sampled = 0, logged = 0, priorReject = 0, priorUnder = 0, priorStop = 0;
  static uint32_t priorDrops = 0, priorOpens = 0;
  static int priorState = -1;
  if (now - sampled < 100) return;
  sampled = now;
  if (!audioRuntime.snapshot(audioLive)) return;
  pocketjazz::AudioFaultSnapshot fault;
  if (!audioRuntime.lastFault(fault)) return;
  const auto pcm = audioPlayout.snapshot();
  const uint32_t pendingFaults = fault.serial - audioFaultSeen;
  const bool newFault = pendingFaults != 0;
  const bool changed = newFault || pcm.pcmHandoffFailures != priorReject ||
      audioUnderruns.load() != priorUnder || pcm.stopCount != priorStop ||
      wifiDisconnectEvents.load() != priorDrops || audioReopens.load() != priorOpens ||
      audioState.load() != priorState;
  if (!changed && now - logged < 1000) return;
  if (!changed && audioLive.stage == pocketjazz::AudioStage::IDLE) return;
  pocketjazz::AudioRuntimeEvent e;
  e.at = now; e.observedAt = now; e.stage = audioLive.stage; e.stageMs = now - audioLive.since;
  e.heartbeatMs = now - audioLive.heartbeat; e.detail = audioLive.detail;
  e.attempt = audioLive.attempt; e.hop = audioLive.hop;
  e.retryAt = audioLive.retryAt; e.outstanding = audioLive.outstanding;
  e.buffer = audioBufferedBytes.load(); e.freeHeap = ESP.getFreeHeap();
  e.largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  e.opens = audioReopens.load(); e.frames = audioFrames.load();
  e.rejects = pcm.pcmHandoffFailures; e.drops = wifiDisconnectEvents.load();
  e.under = audioUnderruns.load(); e.stopMs = pcm.stopLastMs;
  const uint32_t pcmAt = audioLastPcmAt.load();
  e.pcmAgeMs = pcmAt ? now - pcmAt : UINT32_MAX;
  e.inputWaiting = audioInputWaiting.load();
  e.rssi = WiFi.RSSI(); e.wifi = wifiReady.load(); e.state = audioState.load();
  if (newFault) {
    const uint32_t retained = pendingFaults > 16 ? 16 : pendingFaults;
    audioFaultCoalesced += pendingFaults - retained;
    for (uint32_t n = retained; n > 0; --n) {
      pocketjazz::AudioFaultSnapshot item;
      if (!audioRuntime.readFault(fault.serial - n + 1, item)) {
        ++audioFaultCoalesced; continue;
      }
      auto event = e;
      event.at = item.at; event.stage = item.stage; event.stageMs = item.at - item.since;
      event.detail = 0; // do not attach a later PCM wait detail to an old fault
      event.failure = item.failure; event.http = item.http; event.tls = item.tls;
      event.freeHeap = item.freeHeap; event.largest = item.largest;
      event.attempt = item.attempt; event.hop = item.hop;
      event.freeBefore = item.freeBefore; event.largestBefore = item.largestBefore;
      event.retryAt = 0; event.outstanding = 0;
      audioJournal.push(event);
    }
    audioFaultSeen = fault.serial;
  } else audioJournal.push(e);
  logged = now;
  priorReject = pcm.pcmHandoffFailures; priorUnder = e.under; priorStop = pcm.stopCount;
  priorDrops = e.drops; priorOpens = e.opens; priorState = e.state;
}

void wifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiGotIpEvents.fetch_add(1);
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiDisconnectReason.store(info.wifi_sta_disconnected.reason);
    wifiDisconnectAt.store(millis());
    wifiDisconnectEvents.fetch_add(1);
    wifiReady.store(false);
  }
}
void onNtpSync(struct timeval*) {
  // Runs in the SNTP context. Keep callback nonblocking and UI-free.
  ntpSynced.store(true);
}

bool wifiHasIp() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}
void audioStatusCB(void*, int code, const char* description) {
  audioSourceStatusCode.store(code);
  // Audio transport callback: counters only, never Serial or allocation.
  // Some descriptions can include full stream URLs with private query tokens.
  (void)description;
}
void bufferStatusCB(void*, int code, const char*) {
  if (code == AudioFileSourceBuffer::STATUS_UNDERFLOW)
    audioUnderruns.fetch_add(1);
}
void reportAudioFailureToSerial() {
  // USB CDC can reconnect after ESP32 reset; do not spam Serial or block
  // MP3 I/O. Only the UI task emits numeric, secret-free fault summaries.
  static uint32_t lastReportedOpen = UINT32_MAX;
  static int lastReportedStage = 0;
  const int stage = audioFailureStage.load(std::memory_order_relaxed);
  if (!stage) return;
  const uint32_t opens = audioReopens.load(std::memory_order_relaxed);
  if (opens == lastReportedOpen && stage == lastReportedStage) return;
  lastReportedOpen = opens;
  lastReportedStage = stage;
  Serial.printf("[PocketJazz][Audio] version=%s open=%lu stage=%d http=%d tls=%d wifi=%u ntp=%u free=%lu phase=%lu hop=%lu dns=%d secure=%d dnsms=%lu tlsms=%lu reqms=%lu largest_pre=%lu largest_post=%lu buf=%lu bufmin=%lu ui_gap_max=%lu\n",
                POCKETJAZZ_FIRMWARE_VERSION, (unsigned long)opens, stage,
                audioSourceStatusCode.load(std::memory_order_relaxed),
                audioTlsErrorCode.load(std::memory_order_relaxed),
                (unsigned)wifiReady.load(std::memory_order_relaxed),
                (unsigned)ntpSynced.load(std::memory_order_relaxed),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)audioHttpsPhase.load(), (unsigned long)audioHttpsHop.load(),
                audioHttpsDnsOk.load(), audioHttpsSecureOk.load(),
                (unsigned long)audioHttpsDnsMs.load(), (unsigned long)audioHttpsSecureMs.load(),
                (unsigned long)audioHttpsRequestMs.load(),
                (unsigned long)audioHttpsLargestBefore.load(),
                (unsigned long)audioHttpsLargestAfter.load(),
                (unsigned long)audioBufferedBytes.load(),
                (unsigned long)audioBufferMinimum.load(),
                (unsigned long)uiLoopMaxGapMs.load());
}
void pcmMonitor(void* sink, const int16_t* stereo, size_t frames) {
  if (!stereo || !frames) { pcmPeak.store(0); return; }
  // This is a stereo *source* peak meter, not calibrated DAC/speaker output.
  // Averaging channels would indicate silence for loud opposite-phase stereo.
  // No allocation, floating point, or LCD calls in the audio task.
  pcmPeak.store(pocketjazz::stereoSourcePeakPercent(stereo, frames));
  audioLastPcmAt.store(millis());
  audioFrames.fetch_add(frames);
  auto* output = static_cast<AudioOutputM5Speaker*>(sink);
  if (output) {
    const uint32_t rate = output->getRate();
    decodedPcmRateHz.store(rate, std::memory_order_relaxed);
    audioPlayout.pcmEnqueued(audioAcceptedSequence, millis(),
                             static_cast<uint32_t>(frames), rate);
  }
}
void pcmEnqueueFailure(void*) { audioPlayout.rejectedPcm(); }
void sourcePullTimingCB(void*, uint32_t elapsedUs) {
  pocketjazz::observeMaxMicros(audioSourcePullMaxUs, elapsedUs);
}
void pcmFlushTimingCB(void*, uint32_t elapsedUs) {
  pocketjazz::observeMaxMicros(audioPcmFlushMaxUs, elapsedUs);
}
void icyMetadataCB(void* ctx, const char* type, bool, const char* value) {
  if (!metadataQueue || !ctx || !type || strcmp(type, "StreamTitle") || !value) return;
  TrackMetadata event{};
  event.stationId = *static_cast<uint8_t*>(ctx);
  // Shared callback is also used by upstream plain-HTTP ICY. It must not
  // split UTF-8 at byte 95 or pass metadata controls to UI/TSV exports.
  pocketjazz::log::cleanTsvField(value, event.title, sizeof(event.title));
  if (!event.title[0]) return;
  xQueueOverwrite(metadataQueue, &event);
}

template <size_t N> void put(char (&dst)[N], const String& text) {
  strlcpy(dst, text.c_str(), N);
}
bool addStation(const String& name, const String& url, const String& alt = String(),
                int energy = -1, int acoustic = -1) {
  if (stationCount >= MAX_STATIONS || !url.startsWith("https://") && !url.startsWith("http://")) return false;
  if (url.length() >= sizeof(stations[0].url)) return false;
  put(stations[stationCount].name, name.length() ? name : String("Jazz station"));
  put(stations[stationCount].url, url);
  if (alt != url && (alt.startsWith("https://") ||
        (url.startsWith("http://") && alt.startsWith("http://"))) &&
      alt.length() < pocketjazz::StationAltUrlPool::kMaxUrlBytes)
    stations[stationCount].altOffset = stationAltUrls.add(alt.c_str());
  else stations[stationCount].altOffset = pocketjazz::StationAltUrlPool::kNone;
  if (energy >= 0 && energy <= 4 && acoustic >= 0 && acoustic <= 4) {
    stations[stationCount].moodEnergy = energy;
    stations[stationCount].moodAcoustic = acoustic;
  } else {
    stations[stationCount].moodEnergy = -1;
    stations[stationCount].moodAcoustic = -1;
  }
  ++stationCount;
  return true;
}
String niceName(String filename) {
  int slash = filename.lastIndexOf('/');
  if (slash >= 0) filename.remove(0, slash + 1);
  if (filename.endsWith(".m3u")) filename.remove(filename.length() - 4);
  if (filename.endsWith(".M3U")) filename.remove(filename.length() - 4);
  if (filename.endsWith(".pls") || filename.endsWith(".PLS")) filename.remove(filename.length() - 4);
  if (filename.length() > 3 && filename[2] == '-') filename.remove(0, 3);
  filename.replace('-', ' ');
  return filename;
}
int parseMoodValue(const String& tags, const String& key) {
  // Intentional strict 0..4 parsing: toInt() alone would map invalid text to zero.
  int at = tags.indexOf(key + "=");
  if (at < 0 || (at > 0 && tags[at - 1] != ',')) return -1;
  at += key.length() + 1;
  if (at >= (int)tags.length() || tags[at] < '0' || tags[at] > '4') return -1;
  if (at + 1 < (int)tags.length() && tags[at + 1] != ',') return -1;
  return tags[at] - '0';
}
int closestMoodStation() {
  int best = -1, bestDistance = 999;
  for (int i = 0; i < stationCount; ++i) {
    if (stations[i].moodEnergy < 0 || stations[i].moodAcoustic < 0) continue;
    const int dx = (int)moodEnergyCursor - stations[i].moodEnergy;
    const int dy = (int)moodAcousticCursor - stations[i].moodAcoustic;
    const int distance = dx * dx + dy * dy;
    if (distance < bestDistance) { bestDistance = distance; best = i; }
  }
  return best;
}
void addBuiltins() {
  listCount = stationCount = 0;
  stationAltUrls.reset();
  Playlist& p = lists[listCount++];
  put(p.name, String("Jazz essentials"));
  p.first = 0;
  addStation("Radio Swiss Jazz", "https://stream.srg-ssr.ch/srgssr/rsj/mp3/128");
  addStation("Jazz24", "https://knkx-live-a.edge.audiocdn.com/6285_128k");
  addStation("WBGO Jazz", "https://ais-sa8.cdnstream1.com/3629_128.mp3");
  addStation("FIP Jazz", "https://icecast.radiofrance.fr/fipjazz-midfi.mp3");
  p.count = stationCount;
}
// Compare calendar fields rather than tm_gmtoff (not exposed by the pinned
// ESP32 newlib headers). Unlike printing the desired UTC+offset label, this
// exercises the actual libc TZ/localtime_r implementation after tzset().
int effectiveLocalUtcOffsetMinutes(time_t at) {
  tm local{}, utc{};
  if (!localtime_r(&at, &local) || !gmtime_r(&at, &utc)) return -100000;
  return pocketjazz::effectiveOffsetFromCalendars(local, utc);
}
void loadClockSettings() {
  strlcpy(clockTz, "UTC0", sizeof(clockTz));
  clockConfigLoaded = false;
  // The user's explicit NVS choice wins; otherwise preserve the full POSIX
  // SD rule (including DST) with legacy UTC0 fallback.
  if (!clockUserOverride && sdAvailable) {
    File file = SD.open("/PocketJazz/clock.txt", FILE_READ);
    if (file) {
      if (file.size() > 0 && file.size() <= 256) {
        String row = file.readStringUntil('\n');
        row.trim();
        if (row.startsWith("TZ=")) {
          const String candidate = row.substring(3);
          if (pocketjazz::validPosixTimeZone(candidate.c_str())) {
            strlcpy(clockTz, candidate.c_str(), sizeof(clockTz));
            clockConfigLoaded = true;
          }
        }
      }
      file.close();
    }
  }
  if (clockUserOverride &&
      !pocketjazz::fixedUtcOffsetRule(clockOffsetMinutes, clockTz, sizeof(clockTz))) {
    clockUserOverride = false;
    strlcpy(clockTz, "UTC0", sizeof(clockTz));
  }
  setenv("TZ", clockTz, 1);
  tzset();
  Serial.printf("[PocketJazz][Clock] TZ loaded=%d rule=%s\n",
                (int)clockConfigLoaded, clockTz);
  if (clockUserOverride) {
    // Fixed TZ must have identical offset on any date. This independent
    // 2024 UTC anchor needs NO NTP and confirms whether localtime_r actually
    // applies POSIX TZ, not merely whether we formatted a label. Logs only.
    const time_t anchor = 1704067200; // 2024-01-01 00:00:00 UTC.
    const int effective = effectiveLocalUtcOffsetMinutes(anchor);
    Serial.printf("[PocketJazz][Clock] TZ effective=%d expected=%d match=%d\n",
                  effective, clockOffsetMinutes,
                  (int)(effective == clockOffsetMinutes));
  }
}
void saveClockOffset() {
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return; }
  Preferences p;
  if (!p.begin("pocketjazz", false)) { showNotice("TZ save failed"); return; }
  const size_t boolBytes = p.putBool("tzSet", clockUserOverride);
  const size_t minuteBytes = p.putInt("tzMin", clockOffsetMinutes);
  const bool readback = p.getBool("tzSet", !clockUserOverride) == clockUserOverride &&
                        p.getInt("tzMin", -100000) == clockOffsetMinutes;
  Serial.printf("[PocketJazz][Clock] NVS offset=%d override=%d bytes=%u/%u readback=%d\n",
                clockOffsetMinutes, (int)clockUserOverride,
                (unsigned)boolBytes, (unsigned)minuteBytes, (int)readback);
  if (boolBytes != 1 || minuteBytes != sizeof(int32_t) || !readback)
    showNotice("TZ save failed");
  p.end();
}
String clockOffsetText() {
  const int absMinutes = abs(clockOffsetMinutes);
  char label[22] = {};
  snprintf(label, sizeof(label), "UTC%c%02d:%02d", clockOffsetMinutes >= 0 ? '+' : '-',
           absMinutes / 60, absMinutes % 60);
  return String(label);
}
void adjustClockOffset(int stepMinutes) {
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return; }
  // SD TZ may be a DST rule; take the currently effective LOCAL offset before
  // switching to a fixed rule, so pressing right preserves current time zone.
  if (!clockUserOverride && clockConfigLoaded && time(nullptr) >= MIN_TLS_TIME) {
    const int minutes = effectiveLocalUtcOffsetMinutes(time(nullptr));
    if (minutes == -100000) {
      showNotice("Cannot read local TZ");
      return;
    }
    clockOffsetMinutes = constrain(minutes, -720, 840);
    clockOffsetMinutes = (clockOffsetMinutes / 30) * 30;
  }
  const int candidate = clockOffsetMinutes + stepMinutes;
  if (!pocketjazz::validUtcOffsetMinutes(candidate)) {
    showNotice("TZ range -12:00 to +14:00");
    return;
  }
  clockOffsetMinutes = candidate;
  clockUserOverride = true;
  loadClockSettings();
  saveClockOffset();
}
void saveSparkAutoSetting() {
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return; }
  Preferences p;
  if (!p.begin("pocketjazz", false)) { showNotice("Spark save failed"); return; }
  p.putBool("sparkAuto", jazzSparkAutoEnabled);
  p.end();
}
bool loadPlaylists() {
  listCount = stationCount = 0;
  catalogDefaultListsLoadedMask = 0;
  memset(catalogDefaultStationCounts, 0, sizeof(catalogDefaultStationCounts));
  stationAltUrls.reset();
  // Station IDs are reassigned by SD reload; don't attach stale health scores
  // to unrelated new URLs occupying the same array index.
  stationHealth.reset();
  sdAvailable = false;
  // Official ADV microSD pins: SCK=40, MISO=39, MOSI=14, CS=12.
  SPI.begin(40, 39, 14, SD_CS);
  if (!SD.begin(SD_CS, SPI, 10000000)) {
    addBuiltins();
    return false;
  }
  sdAvailable = true;
  if (!catalogRecoveryChecked) {
    // On first card mount from setup() the audio queue does not exist yet.
    // If a card is inserted later during playback, refuse a potentially
    // destructive recovery until STOP, rather than racing the audio worker.
    if (audioQueue &&
        (audioState.load() != STOPPED || wantPlay || offlineSourceOpen.load())) {
      addBuiltins();
      showNotice("Stop radio for SD recovery");
      return false;
    }
    const auto recovered = pocketjazz::recoverCatalogUpdate();
    if (recovered == pocketjazz::CatalogUpdateResult::RecoveryFailed) {
      // Do not load half-swapped default playlists; keep built-in fallback.
      addBuiltins();
      showNotice("Catalog recovery failed");
      return false;
    }
    catalogRecoveryChecked = true;
  }
  File directory = SD.open("/PocketJazz");
  if (!directory || !directory.isDirectory()) {
    addBuiltins();
    if (directory) directory.close();
    return false;
  }
  File file;
  while ((file = directory.openNextFile()) && listCount < MAX_LISTS && stationCount < MAX_STATIONS) {
    String filename = file.name();
    const bool isM3U = filename.endsWith(".m3u") || filename.endsWith(".M3U");
    const bool isPLS = filename.endsWith(".pls") || filename.endsWith(".PLS");
    if (file.isDirectory() || (!isM3U && !isPLS)) {
      file.close();
      continue;
    }
    Playlist p{};
    p.first = stationCount;
    put(p.name, niceName(filename));
    String pendingName, pendingAlt;
    int pendingEnergy = -1, pendingAcoustic = -1;
    if (isM3U) {
      while (file.available() && stationCount < MAX_STATIONS) {
        String line = file.readStringUntil('\n');
        if (line.length() > 512) continue;  // cap potentially malformed input
        line.trim();
        if (line.startsWith("#EXTINF:")) {
          int comma = line.indexOf(',');
          pendingName = comma >= 0 ? line.substring(comma + 1) : String();
          pendingName.trim();
        } else if (line.startsWith("#PJ-ALT:")) {
          pendingAlt = line.substring(8);
          pendingAlt.trim();
        } else if (line.startsWith("#PJ-MOOD:")) {
          const String tags = line.substring(9);
          pendingEnergy = parseMoodValue(tags, "energy");
          pendingAcoustic = parseMoodValue(tags, "acoustic");
        } else if (line.startsWith("http://") || line.startsWith("https://")) {
          addStation(pendingName, line, pendingAlt, pendingEnergy, pendingAcoustic);
          pendingName = "";
          pendingAlt = "";
          pendingEnergy = pendingAcoustic = -1;
        }
      }
    } else {
      // PLS entries may place TitleN before or after FileN.
      constexpr int PLS_MAX = 32;
      String urls[PLS_MAX], titles[PLS_MAX];
      while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() > 512) continue;
        line.trim();
        const int equals = line.indexOf('=');
        if (equals < 5) continue;
        const String key = line.substring(0, equals);
        const bool fileKey = key.startsWith("File");
        const bool titleKey = key.startsWith("Title");
        if (!fileKey && !titleKey) continue;
        const int n = key.substring(fileKey ? 4 : 5).toInt();
        if (n < 1 || n > PLS_MAX) continue;
        const String value = line.substring(equals + 1);
        if (fileKey) urls[n - 1] = value;
        else titles[n - 1] = value;
      }
      for (int i = 0; i < PLS_MAX && stationCount < MAX_STATIONS; ++i) {
        if (urls[i].startsWith("https://") || urls[i].startsWith("http://"))
          addStation(titles[i], urls[i]);
      }
    }
    file.close();
    p.count = stationCount - p.first;
    if (p.count) {
      // Track only successfully parsed official playlists, by exact basename.
      // Staged/backup files are in hidden directories and are never scanned.
      const int separator = filename.lastIndexOf('/');
      const String basename = separator >= 0 ? filename.substring(separator + 1) : filename;
      static const char* const kDefaultNames[5] = {
        "01-essentials.m3u", "02-modern-jazz.m3u", "03-mellow-bossa.m3u",
        "05-swing-vocals.m3u", "06-latin-soul-jazz.m3u"
      };
      for (int i = 0; i < 5; ++i)
        if (basename == kDefaultNames[i]) {
          catalogDefaultListsLoadedMask |= static_cast<uint8_t>(1u << i);
          catalogDefaultStationCounts[i] = static_cast<uint8_t>(p.count);
          break;
        }
      lists[listCount++] = p;
    }
  }
  directory.close();
  if (!listCount) addBuiltins();
  return sdAvailable;
}
void enterOfflineLibrary() {
  // Do not scan FAT while the audio worker still owns an SD file, or while a
  // live decoder could be delayed by a synchronous directory traversal.
  if (wantPlay || audioState.load() != STOPPED || offlineSourceOpen.load()) {
    showNotice("STOP radio before SD");
    return;
  }
  if (!sdAvailable) { showNotice("No microSD card"); return; }
  // Invalidate any pending off-channel ICY peek before selecting SD music.
  invalidateStationPeek(true);
  offlineCatalog.clear();
  File dir = SD.open("/PocketJazz/offline", FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    showNotice("No /PocketJazz/offline/");
    return;
  }
  // Max 96 entries inspected and 24 accepted. Files remain owned by the user;
  // the radio neither creates the directory nor renames/moves audio content.
  for (int checked = 0; checked < 96 && offlineCatalog.count() < 24; ++checked) {
    File file = dir.openNextFile();
    if (!file) break;
    const String fullName = file.name();
    const int slash = fullName.lastIndexOf('/');
    const String leaf = fullName.substring(slash + 1);
    offlineCatalog.add(leaf.c_str(), file.size(), file.isDirectory());
    file.close();
  }
  dir.close();
  if (!offlineCatalog.count()) { showNotice("No supported MP3"); return; }
  offlineSelected = 0;
  offlinePath[0] = 0;
  offlineMode = false;
  offlineLeaving = false;
  screen = OFFLINE_LIBRARY;
}
int stationIndex() {
  if (!listCount || !lists[currentList].count) return -1;
  if (currentOffset >= lists[currentList].count) currentOffset = 0;
  return lists[currentList].first + currentOffset;
}
void sendAudio() {
  if (!audioQueue) return;
  AudioCommand cmd{};
  cmd.play = wantPlay;
  cmd.offline = offlineMode;
  // Only the established audio task applies the volume to M5.Speaker. All
  // alarm fade commands use the same mailbox; no second audio/ES8311 owner.
  cmd.volume = !cmd.play ? 0 : dailyAlarmSounding ? dailyAlarmOutputVolume :
               muted ? 0 : (uint8_t)volume;
  requestedOutputVolume.store(cmd.volume, std::memory_order_release);
  const int idx = offlineMode ? -1 : stationIndex();
  if (offlineMode) {
    strlcpy(cmd.url, offlinePath, sizeof(cmd.url));
    cmd.stationId = 0xff; // never treat offline ID3 as ICY metadata
  } else if (idx >= 0) {
    cmd.stationId = (uint8_t)idx;
    strlcpy(cmd.url, stations[idx].url, sizeof(cmd.url));
    strlcpy(cmd.altUrl, stationAltUrls.get(stations[idx].altOffset),
            sizeof(cmd.altUrl));
  }
  const bool newIntent = (cmd.play != lastRequestedAudioPlay ||
                          cmd.offline != lastRequestedAudioOffline ||
                          strcmp(cmd.url, lastRequestedAudioUrl) != 0 ||
                          strcmp(cmd.altUrl, lastRequestedAudioAltUrl) != 0);
  if (newIntent) {
    ++telemetryNextSequence;
    if (!telemetryNextSequence) ++telemetryNextSequence;
    lastRequestedAudioPlay = cmd.play;
    lastRequestedAudioOffline = cmd.offline;
    strlcpy(lastRequestedAudioUrl, cmd.url, sizeof(lastRequestedAudioUrl));
    strlcpy(lastRequestedAudioAltUrl, cmd.altUrl, sizeof(lastRequestedAudioAltUrl));
    audioPlayout.request(telemetryNextSequence, millis(), cmd.play);
    audioRequestedIntentSequence.store(telemetryNextSequence,
                                       std::memory_order_release);
  }
  cmd.telemetrySequence = telemetryNextSequence;
  if (strcmp(lastSelectedUrl, cmd.url) != 0) {
    strlcpy(lastSelectedUrl, cmd.url, sizeof(lastSelectedUrl));
    currentTitle[0] = 0;
  }
  xQueueOverwrite(audioQueue, &cmd);
  lastVolume = volume;
}
void markPrefs() {
  prefTouched = millis();
  prefDirty = true;
}
void showNotice(const char* message) {
  strlcpy(uiNotice, message, sizeof(uiNotice));
  uiNoticeUntil = millis() + 2200;
}
enum ExportWriteStatus { EXPORT_WRITTEN, EXPORT_NO_SD, EXPORT_FULL,
                         EXPORT_FAILED, EXPORT_INCOMPLETE };
ExportWriteStatus appendUiExport(const char* path, const char* data, size_t length,
                                 size_t maxFileBytes = 16384) {
  // Deliberately user-initiated, never invoked by the audio worker or timer.
  // Files are app-owned under /PocketJazz/. When full we DO NOT delete or
  // truncate anything the user might wish to preserve.
  if (!sdAvailable) return EXPORT_NO_SD;
  if (!path || !data || !length || length > 2048) return EXPORT_FAILED;
  File old = SD.open(path, FILE_READ);
  const size_t existing = old ? old.size() : 0;
  if (old) {
    // A previous partial SD write can leave an unterminated TSV row. Never
    // append new rows to a damaged tail and hide it from later diagnostics.
    if (existing && (!old.seek(existing - 1) || old.read() != '\n')) {
      old.close();
      return EXPORT_INCOMPLETE;
    }
    old.close();
  }
  if (!pocketjazz::log::canAppend(existing, length, maxFileBytes)) return EXPORT_FULL;
  File file = SD.open(path, FILE_APPEND);
  if (!file) return EXPORT_FAILED;
  if (!pocketjazz::log::canAppend(file.size(), length, maxFileBytes)) {
    file.close();
    return EXPORT_FULL;
  }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(data), length);
  file.flush(); // FS::File::flush returns void; verify by reading back below.
  file.close();
  if (written != length) return EXPORT_FAILED;
  // The Arduino FS API exposes no flush/close success code. Verify the
  // entire appended segment after reopen, rather than trusting byte count.
  // This is NOT a power-loss durability guarantee for the physical SD media.
  File verify = SD.open(path, FILE_READ);
  if (!verify) return EXPORT_FAILED;
  if (verify.size() != existing + length || !verify.seek(existing)) {
    verify.close();
    return EXPORT_FAILED;
  }
  uint8_t check[64];
  size_t checked = 0;
  while (checked < length) {
    const size_t count = min(sizeof(check), length - checked);
    if (verify.read(check, count) != count ||
        memcmp(check, data + checked, count) != 0) {
      verify.close();
      return EXPORT_FAILED;
    }
    checked += count;
  }
  verify.close();
  return EXPORT_WRITTEN;
}
void showExportStatus(ExportWriteStatus status, const char* success) {
  switch (status) {
    case EXPORT_WRITTEN: showNotice(success); break;
    case EXPORT_NO_SD: showNotice("SD unavailable"); break;
    case EXPORT_FULL: showNotice("SD log full (16KB)"); break;
    case EXPORT_INCOMPLETE: showNotice("SD log tail damaged"); break;
    default: showNotice("SD write failed"); break;
  }
}

void exportAudioRuntime() {
  char row[768];
  for (size_t n = audioJournal.count(); n > 0; --n) {
    const auto& e = *audioJournal.newest(n - 1);
    const int count = snprintf(row, sizeof(row),
      "%lu\tstage=%u\tage=%lu\thb=%lu\tdetail=%lu\tbuf=%lu\tfree=%lu\tlargest=%lu\topens=%lu\tframes=%lu\treject=%lu\tdrops=%lu\tunder=%lu\tstop=%lu\trssi=%ld\twifi=%u\tstate=%ld\tfail=%ld\thttp=%ld\ttls=%ld\tcoalesced=%lu\tpcm_age=%lu\twaiting=%u\tobserved=%lu\tattempt=%lu\thop=%lu\tretry_at=%lu\toutstanding=%lu\tfree_before=%lu\tlargest_before=%lu\n",
      (unsigned long)e.at, (unsigned)e.stage, (unsigned long)e.stageMs,
      (unsigned long)e.heartbeatMs, (unsigned long)e.detail, (unsigned long)e.buffer,
      (unsigned long)e.freeHeap, (unsigned long)e.largest, (unsigned long)e.opens,
      (unsigned long)e.frames, (unsigned long)e.rejects, (unsigned long)e.drops,
      (unsigned long)e.under, (unsigned long)e.stopMs, (long)e.rssi, (unsigned)e.wifi,
      (long)e.state, (long)e.failure, (long)e.http, (long)e.tls, (unsigned long)audioFaultCoalesced,
      (unsigned long)e.pcmAgeMs, (unsigned)e.inputWaiting, (unsigned long)e.observedAt,
      (unsigned long)e.attempt, (unsigned long)e.hop, (unsigned long)e.retryAt,
      (unsigned long)e.outstanding, (unsigned long)e.freeBefore, (unsigned long)e.largestBefore);
    if (count < 0 || (size_t)count >= sizeof(row)) { showNotice("Runtime row too large"); return; }
    const auto result = appendUiExport("/PocketJazz/audio-runtime.tsv", row, (size_t)count);
    if (result != EXPORT_WRITTEN) { showExportStatus(result, ""); return; }
  }
  showNotice("Runtime events saved");
}
void exportHeapLifecycle() {
  // SD I/O competes with live audio. The snapshots were taken in RAM during
  // network/decode stages; export ONLY after STOP has released the stream.
  if (audioState.load() != STOPPED) {
    showNotice("STOP radio before RAM export");
    return;
  }
  if (!heapLifecycle.attempt()) { showNotice("No RAM samples yet"); return; }
  char row[320];
  size_t countExported = 0;
  for (size_t i = 0; i < pocketjazz::HeapLifecycleProfile::kCount + 1; ++i) {
    pocketjazz::HeapCheckpoint snap;
    const bool firstEver = i == pocketjazz::HeapLifecycleProfile::kCount;
    const bool found = firstEver
        ? heapLifecycle.firstStallEver(snap)
        : heapLifecycle.read(static_cast<pocketjazz::HeapMoment>(i), snap);
    if (!found) continue;
    const char* name = firstEver ? "FIRST_STALL_EVER"
        : pocketjazz::heapMomentName(static_cast<pocketjazz::HeapMoment>(i));
    const int written = snprintf(row, sizeof(row),
        "attempt=%lu\tms=%lu\tphase=%s\tfree8=%lu\tlargest8=%lu\tdma_free=%lu\tdma_largest=%lu\tring=%lu\tbuffer=%lu\tsource_bytes=%lu\tsource_idle_ms=%lu\tsource_gap_max_ms=%lu\n",
        (unsigned long)snap.attempt, (unsigned long)snap.atMs, name,
        (unsigned long)snap.free8, (unsigned long)snap.largest8,
        (unsigned long)snap.freeDma, (unsigned long)snap.largestDma,
        (unsigned long)snap.ringBytes, (unsigned long)snap.bufferedBytes,
        (unsigned long)snap.sourceBytes, (unsigned long)snap.sourceIdleMs,
        (unsigned long)snap.sourceMaxGapMs);
    if (written < 0 || (size_t)written >= sizeof(row)) {
      showNotice("RAM row too large");
      return;
    }
    const auto result = appendUiExport("/PocketJazz/heap-lifecycle.tsv",
                                       row, (size_t)written);
    if (result != EXPORT_WRITTEN) {
      showExportStatus(result, "");
      return;
    }
    ++countExported;
  }
  showNotice(countExported ? "RAM stages saved to SD" : "No RAM stages");
}

void exportDiagnostics() {
  // Numeric operating counters only. Do NOT log Wi-Fi credentials, SSID,
  // direct stream URLs, IP addresses or potentially private QR links.
  char timestamp[26] = {};
  const time_t t = time(nullptr);
  if (ntpSynced.load() && t >= MIN_TLS_TIME) {
    tm utc{};
    gmtime_r(&t, &utc);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
  } else {
    snprintf(timestamp, sizeof(timestamp), "uptime:%lus", (unsigned long)(millis() / 1000UL));
  }
  const auto pcm = audioPlayout.snapshot();
  char row[832];
  const int count = snprintf(row, sizeof(row),
    "%s\twifi=%u\trssi=%d\tip=%u\tdrops=%lu\treason=%d\tstate=%d\tbuffer=%lu\tunderrun=%lu\treopen=%lu\tframes=%lu\theap=%lu\tlargest=%lu\tminheap=%lu\tstack=%lu\tbattery_mv=%d\treset=%d"
    "\tpcm_gap_count=%lu\tpcm_gap_max_ms=%lu\tpcm_gap_total_ms=%lu\tpcm_enqueue_reject=%lu\tpcm_rate_hz=%lu\tstop_count=%lu\tstop_last_ms=%lu\tstop_p50_upper_ms=%lu\tstop_p95_upper_ms=%lu\tstop_max_ms=%lu\tswitch_count=%lu\tswitch_last_ms=%lu\tswitch_p50_upper_ms=%lu\tswitch_p95_upper_ms=%lu\tswitch_max_ms=%lu\n",
    timestamp, (unsigned)wifiReady.load(), wifiHasIp() ? WiFi.RSSI() : -127,
    (unsigned)wifiHasIp(), (unsigned long)wifiDisconnectEvents.load(),
    wifiDisconnectReason.load(), audioState.load(),
    (unsigned long)audioBufferedBytes.load(),
    (unsigned long)diagAudioWindow.under(audioUnderruns.load()),
    (unsigned long)diagAudioWindow.opens(audioReopens.load()),
    (unsigned long)diagAudioWindow.frames(audioFrames.load()),
    (unsigned long)ESP.getFreeHeap(),
    (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
    (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
    (unsigned long)audioStackWatermark.load(), batteryMillivolts,
    (int)esp_reset_reason(),
    (unsigned long)pcm.pcmGapEpisodes, (unsigned long)pcm.pcmGapMaxMs,
    (unsigned long)pcm.pcmGapTotalMs, (unsigned long)pcm.pcmHandoffFailures,
    (unsigned long)decodedPcmRateHz.load(), (unsigned long)pcm.stopCount,
    (unsigned long)pcm.stopLastMs, (unsigned long)pcm.stopP50UpperMs,
    (unsigned long)pcm.stopP95UpperMs,
    (unsigned long)pcm.stopMaxMs, (unsigned long)pcm.switchCount,
    (unsigned long)pcm.switchLastMs, (unsigned long)pcm.switchP50UpperMs,
    (unsigned long)pcm.switchP95UpperMs,
    (unsigned long)pcm.switchMaxMs);
  if (count < 0 || (size_t)count >= sizeof(row)) {
    showNotice("Diag export too large");
    return;
  }
  showExportStatus(appendUiExport("/PocketJazz/diagnostics.tsv", row, (size_t)count),
                   "Diagnostics saved to SD");
}
void exportIcyHistory() {
  if (!trackHistory.size()) { showNotice("No ICY titles to save"); return; }
  // Export each complete TSV *row* as a separate bounded append. A short SD
  // write cannot then commit one complete row plus half of the next and leave
  // an ambiguous snapshot that a later E silently repeats. Existing complete
  // rows are deliberately preserved if a subsequent row fails.
  size_t exported = 0;
  const size_t total = trackHistory.size();
  for (size_t i = trackHistory.size(); i > 0; --i) {
    const auto* item = trackHistory.newest(i - 1);
    char station[56], title[96];
    pocketjazz::log::cleanTsvField(item->station, station, sizeof(station));
    pocketjazz::log::cleanTsvField(item->title, title, sizeof(title));
    char row[256];
    const int added = snprintf(row, sizeof(row), "%lu\t%s\t%s\n",
                               (unsigned long)item->observedAtMs, station, title);
    if (added < 0 || (size_t)added >= sizeof(row)) {
      showNotice("ICY export too large");
      return;
    }
    const ExportWriteStatus status = appendUiExport("/PocketJazz/icy-history.tsv",
                                                    row, (size_t)added);
    if (status != EXPORT_WRITTEN) {
      if (exported) showNotice("ICY export partial: SD");
      else showExportStatus(status, "ICY history saved to SD");
      return;
    }
    ++exported;
  }
  if (exported == total) showNotice("ICY history saved to SD");
}
void likeCurrentTrack() {
  const int idx = stationIndex();
  if (idx < 0 || !sdAvailable) { showNotice("SD unavailable"); return; }
  char clock[24] = "time-unknown";
  if (ntpSynced.load() && time(nullptr) >= MIN_TLS_TIME) {
    time_t t = time(nullptr);
    tm utc{};
    gmtime_r(&t, &utc);
    strftime(clock, sizeof(clock), "%Y-%m-%dT%H:%MZ", &utc);
  }
  char safeName[56], safeTrack[96];
  pocketjazz::log::cleanTsvField(stations[idx].name, safeName, sizeof(safeName));
  pocketjazz::log::cleanTsvField(currentTitle[0] ? currentTitle : "track-unknown",
                                 safeTrack, sizeof(safeTrack));
  const String name(safeName[0] ? safeName : "station-unknown");
  const String track(safeTrack[0] ? safeTrack : "track-unknown");
  // Preserve the original K action while fixing its historic size check:
  // checking only the old file size allowed one final line to exceed cap.
  const String row = String(clock) + "\t" + name + "\t" + track + "\n";
  const ExportWriteStatus result = appendUiExport("/PocketJazz/liked.txt",
                                                 row.c_str(), row.length(), 65536);
  if (result == EXPORT_WRITTEN) {
    if (pocketjazz::canSearchTrackTitle(safeTrack))
      strlcpy(lastLikedTitle, safeTrack, sizeof(lastLikedTitle));
    else lastLikedTitle[0] = '\0'; // Don't turn 'track-unknown' into a search.
  }
  showNotice(result == EXPORT_WRITTEN ? "Like saved to SD" :
             result == EXPORT_FULL ? "Likes log full" :
             result == EXPORT_NO_SD ? "SD unavailable" : "Likes write failed");
}
int findStationUrl(const char* url) {
  for (int i = 0; i < stationCount; ++i)
    if (strcmp(stations[i].url, url) == 0) return i;
  return -1;
}
bool selectByUrl(const char* url) {
  const int idx = findStationUrl(url);
  if (idx < 0) return false;
  for (int i = 0; i < listCount; ++i)
    if (idx >= lists[i].first && idx < lists[i].first + lists[i].count) {
      currentList = i;
      currentOffset = idx - lists[i].first;
      wantPlay = true;
      markPrefs();
      sendAudio();
      return true;
    }
  return false;
}
int searchMatch(int nth) {
  String query = stationSearch;
  query.toLowerCase();
  int seen = 0;
  for (int i = 0; i < stationCount; ++i) {
    String label = stations[i].name;
    label.toLowerCase();
    if (!query.isEmpty() && label.indexOf(query) < 0) continue;
    if (seen++ == nth) return i;
  }
  return -1;
}
int searchMatchCount() {
  int count = 0;
  while (count < stationCount && searchMatch(count) >= 0) ++count;
  return count;
}
void savePreset(int slot) {
  const int idx = stationIndex();
  if (idx < 0 || slot < 0 || slot >= PRESET_COUNT) return;
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return; }
  strlcpy(presets[slot].name, stations[idx].name, sizeof(presets[slot].name));
  strlcpy(presets[slot].url, stations[idx].url, sizeof(presets[slot].url));
  Preferences p;
  if (p.begin("pocketjazz", false)) {
    String key = "pr" + String(slot);
    p.putString((key + "u").c_str(), presets[slot].url);
    p.putString((key + "n").c_str(), presets[slot].name);
    p.end();
  }
  showNotice("Preset saved");
}
void saveRecent() {
  if (futureNvsSchema) return;
  Preferences p;
  if (p.begin("pocketjazz", false)) {
    p.putUChar("rCount", recentCount);
    for (int i = 0; i < recentCount; ++i) {
      String key = "rc" + String(i);
      p.putString((key + "u").c_str(), recent[i].url);
      p.putString((key + "n").c_str(), recent[i].name);
    }
    p.end();
  }
}
void serviceRecent(uint32_t now) {
  const int idx = stationIndex();
  if (offlineMode || idx < 0 || audioState.load() != PLAYING || !wifiReady.load()) {
    audioPlayingSince = 0;
    return;
  }
  if (strcmp(lastRecentUrl, stations[idx].url) != 0) {
    strlcpy(lastRecentUrl, stations[idx].url, sizeof(lastRecentUrl));
    audioPlayingSince = now;
    return;
  }
  if (!audioPlayingSince || now - audioPlayingSince < 5000 || lastRecentAt == audioPlayingSince) return;
  lastRecentAt = audioPlayingSince;
  int duplicate = -1;
  for (int i = 0; i < recentCount; ++i)
    if (!strcmp(recent[i].url, stations[idx].url)) { duplicate = i; break; }
  if (duplicate == 0) return;
  if (duplicate >= 0) {
    SavedStation saved = recent[duplicate];
    for (int i = duplicate; i > 0; --i) recent[i] = recent[i - 1];
    recent[0] = saved;
  } else {
    for (int i = min(recentCount, RECENT_COUNT - 1); i > 0; --i) recent[i] = recent[i - 1];
    strlcpy(recent[0].url, stations[idx].url, sizeof(recent[0].url));
    strlcpy(recent[0].name, stations[idx].name, sizeof(recent[0].name));
    recentCount = min(recentCount + 1, RECENT_COUNT);
  }
  saveRecent();
}
void savePrefs() {
  if (futureNvsSchema) {
    prefDirty = false;
    return;
  }
  Preferences p;
  if (p.begin("pocketjazz", false)) {
    p.putUChar("list", currentList);
    p.putUChar("station", currentOffset);
    p.putUChar("vol", volume);
    p.putUChar("schema", pocketjazz::kCurrentPreferenceSchema);
    p.putBool("autoplay", autoPlay);
    p.putBool("fxVinyl", vinylCrackleEnabled.load(std::memory_order_relaxed));
    p.putBool("fxHiss", tuningHissEnabled.load(std::memory_order_relaxed));
    const int idx = stationIndex();
    if (idx >= 0) p.putString("lastUrl", stations[idx].url);
    p.end();
  }
  prefDirty = false;
}
bool saveTuningEffect(bool crackle) {
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return false; }
  std::atomic<bool>& flag = crackle ? vinylCrackleEnabled : tuningHissEnabled;
  const bool next = !flag.load(std::memory_order_relaxed);
  const char* key = crackle ? "fxVinyl" : "fxHiss";
  Preferences p;
  if (!p.begin("pocketjazz", false)) { showNotice("FX NVS unavailable"); return false; }
  const bool written = p.putBool(key, next) == 1 && p.getBool(key, !next) == next;
  p.end();
  if (!written) { showNotice("FX save failed"); return false; }
  flag.store(next, std::memory_order_release);
  showNotice(next ? "Tuning FX enabled" : "Tuning FX disabled");
  return true;
}
void restoreDailyRadioAlarm() {
  dailyAlarmConfig = pocketjazz::DailyRadioAlarmConfig{}; // disabled until valid data
  dailyAlarmLastFiredDay = -1;
  dailyAlarmFutureFormat = false;
  if (futureNvsSchema) { dailyAlarmEdit = dailyAlarmConfig; return; }
  Preferences p;
  if (!p.begin("pocketjazz", true)) { dailyAlarmEdit = dailyAlarmConfig; return; }
  const size_t stored = p.getBytesLength("alarmCfg");
  if (stored == pocketjazz::DailyRadioAlarm::kConfigBytes) {
    uint8_t bytes[pocketjazz::DailyRadioAlarm::kConfigBytes]{};
    if (p.getBytes("alarmCfg", bytes, sizeof(bytes)) == sizeof(bytes)) {
      if (!pocketjazz::DailyRadioAlarm::decode(bytes, sizeof(bytes), dailyAlarmConfig) &&
          bytes[0] == 'J' && bytes[1] == 'A' && bytes[2] > 1)
        dailyAlarmFutureFormat = true; // unknown same-length future format: preserve
    }
  } else if (stored) {
    // Do not overwrite unknown future/oversized data with our own layout.
    dailyAlarmFutureFormat = true;
  }
  const int32_t savedDay = p.getInt("alarmDay", -1);
  if (pocketjazz::DailyRadioAlarm::validDayKey(savedDay))
    dailyAlarmLastFiredDay = savedDay;
  p.end();
  dailyAlarmEdit = dailyAlarmConfig;
}
bool saveDailyRadioAlarmConfig() {
  if (futureNvsSchema || dailyAlarmFutureFormat) {
    showNotice("Alarm NVS read-only");
    return false;
  }
  if (!pocketjazz::DailyRadioAlarm::valid(dailyAlarmEdit)) return false;
  uint8_t bytes[pocketjazz::DailyRadioAlarm::kConfigBytes]{};
  pocketjazz::DailyRadioAlarm::encode(dailyAlarmEdit, bytes);
  Preferences p;
  if (!p.begin("pocketjazz", false)) { showNotice("Alarm NVS unavailable"); return false; }
  uint8_t back[sizeof(bytes)]{};
  const bool ok = p.putBytes("alarmCfg", bytes, sizeof(bytes)) == sizeof(bytes) &&
      p.getBytesLength("alarmCfg") == sizeof(bytes) &&
      p.getBytes("alarmCfg", back, sizeof(back)) == sizeof(back) &&
      memcmp(bytes, back, sizeof(bytes)) == 0;
  p.end();
  if (!ok) { showNotice("Alarm save failed"); return false; }
  dailyAlarmConfig = dailyAlarmEdit;
  return true;
}
bool claimDailyRadioAlarmDay(int32_t dayKey) {
  if (futureNvsSchema || dailyAlarmFutureFormat ||
      !pocketjazz::DailyRadioAlarm::validDayKey(dayKey)) return false;
  Preferences p;
  if (!p.begin("pocketjazz", false)) return false;
  const bool ok = p.putInt("alarmDay", dayKey) == sizeof(int32_t) &&
                  p.getInt("alarmDay", -1) == dayKey;
  p.end();
  if (ok) dailyAlarmLastFiredDay = dayKey;
  return ok;
}
void captureBackupSettings(pocketjazz::SettingsBackupData& out) {
  out = pocketjazz::SettingsBackupData{};
  out.volume = (uint8_t)volume;
  out.list = (uint8_t)currentList;
  out.station = (uint8_t)currentOffset;
  out.autoplay = autoPlay;
  out.tzOverride = clockUserOverride;
  out.tzMinutes = clockOffsetMinutes;
  out.sparkAuto = jazzSparkAutoEnabled;
  const int idx = stationIndex();
  if (idx >= 0) strlcpy(out.lastUrl, stations[idx].url, sizeof(out.lastUrl));
  for (int i = 0; i < PRESET_COUNT; ++i) {
    strlcpy(out.presetUrl[i], presets[i].url, sizeof(out.presetUrl[i]));
    strlcpy(out.presetName[i], presets[i].name, sizeof(out.presetName[i]));
  }
}
bool writeBackupSettingsNvs(const pocketjazz::SettingsBackupData& data) {
  if (futureNvsSchema || !pocketjazz::SettingsBackupPolicy::valid(data)) return false;
  Preferences p;
  if (!p.begin("pocketjazz", false)) return false;
  // Not a single NVS transaction: validate the full SD input first and keep a
  // RAM rollback snapshot. Power-loss durability cannot be promised here.
  bool ok = p.putUChar("vol", data.volume) == 1 &&
            p.getUChar("vol", 255) == data.volume;
  ok = (p.putUChar("list", data.list) == 1 &&
        p.getUChar("list", 255) == data.list) && ok;
  ok = (p.putUChar("station", data.station) == 1 &&
        p.getUChar("station", 255) == data.station) && ok;
  ok = (p.putBool("autoplay", data.autoplay) == 1 &&
        p.getBool("autoplay", !data.autoplay) == data.autoplay) && ok;
  ok = (p.putBool("tzSet", data.tzOverride) == 1 &&
        p.getBool("tzSet", !data.tzOverride) == data.tzOverride) && ok;
  ok = (p.putInt("tzMin", data.tzMinutes) == sizeof(int32_t) &&
        p.getInt("tzMin", -100000) == data.tzMinutes) && ok;
  ok = (p.putBool("sparkAuto", data.sparkAuto) == 1 &&
        p.getBool("sparkAuto", !data.sparkAuto) == data.sparkAuto) && ok;
  p.putString("lastUrl", data.lastUrl);
  ok = (p.getString("lastUrl", "") == data.lastUrl) && ok;
  for (int i = 0; i < PRESET_COUNT; ++i) {
    const String key = "pr" + String(i);
    p.putString((key + "u").c_str(), data.presetUrl[i]);
    p.putString((key + "n").c_str(), data.presetName[i]);
    ok = (p.getString((key + "u").c_str(), "") == data.presetUrl[i] &&
          p.getString((key + "n").c_str(), "") == data.presetName[i]) && ok;
  }
  // Do not touch Wi-Fi profiles/credentials, recents, liked.txt, SD playlists.
  p.end();
  return ok;
}
bool settingsIoAllowed() {
  if (!sdAvailable) { showNotice("No SD for backup"); return false; }
  if (futureNvsSchema) { showNotice("NVS newer: read-only"); return false; }
  if (wantPlay || audioState.load() != STOPPED) {
    showNotice("STOP radio first");
    return false;
  }
  return true;
}
bool exportSettingsBackup() {
  if (!settingsIoAllowed()) return false;
  std::unique_ptr<SettingsBackupWorkspace> workspace(new (std::nothrow) SettingsBackupWorkspace);
  if (!workspace) { showNotice("Backup memory low"); return false; }
  auto& settingsStage = workspace->stage;
  char* const settingsBackupWire = workspace->wire;
  captureBackupSettings(settingsStage);
  size_t length = 0;
  if (!pocketjazz::SettingsBackupPolicy::encode(
          settingsStage, settingsBackupWire, sizeof(workspace->wire), length)) {
    showNotice("Backup format error"); return false;
  }
  const char* const temp = "/PocketJazz/settings.tmp";
  const char* const final = "/PocketJazz/settings.bak";
  if (SD.exists(temp) && !SD.remove(temp)) {
    showNotice("Old temp locked"); return false;
  }
  File file = SD.open(temp, FILE_WRITE);
  if (!file) { showNotice("Backup open failed"); return false; }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(settingsBackupWire), length);
  file.flush();
  file.close();
  if (written != length) { showNotice("Backup SD short write"); return false; }
  File verify = SD.open(temp, FILE_READ);
  if (!verify || verify.size() != length) {
    if (verify) verify.close();
    showNotice("Backup verify failed"); return false;
  }
  bool verified = true;
  uint8_t chunk[128];
  for (size_t offset = 0; offset < length && verified; offset += sizeof(chunk)) {
    const size_t n = min(sizeof(chunk), length - offset);
    if (verify.read(chunk, n) != (int)n ||
        memcmp(chunk, settingsBackupWire + offset, n)) verified = false;
  }
  verify.close();
  if (!verified) { showNotice("Backup readback failed"); return false; }
  // Preserve the previous backup under an unused suffix; never delete any
  // existing previous version. A torn FAT rename remains power-loss sensitive.
  char previous[48] = {};
  bool movedPrevious = false;
  if (SD.exists(final)) {
    for (int i = 0; i < 10; ++i) {
      snprintf(previous, sizeof(previous), "/PocketJazz/settings.prev%d", i);
      if (!SD.exists(previous)) break;
      previous[0] = 0;
    }
    if (!previous[0] || !SD.rename(final, previous)) {
      showNotice("Backup history full"); return false;
    }
    movedPrevious = true;
  }
  if (!SD.rename(temp, final)) {
    if (movedPrevious) SD.rename(previous, final); // best-effort recovery
    showNotice("Backup rename failed"); return false;
  }
  showNotice("Backup saved to SD");
  return true;
}
bool importSettingsBackup() {
  if (!settingsIoAllowed()) return false;
  std::unique_ptr<SettingsBackupWorkspace> workspace(new (std::nothrow) SettingsBackupWorkspace);
  if (!workspace) { showNotice("Backup memory low"); return false; }
  auto& settingsStage = workspace->stage;
  auto& settingsRollback = workspace->rollback;
  char* const settingsBackupWire = workspace->wire;
  File file = SD.open("/PocketJazz/settings.bak", FILE_READ);
  if (!file || !file.size() || file.size() > sizeof(workspace->wire)) {
    if (file) file.close();
    showNotice("Backup missing/oversize"); return false;
  }
  const size_t length = file.size();
  const size_t got = file.readBytes(settingsBackupWire, length);
  file.close();
  if (got != length || !pocketjazz::SettingsBackupPolicy::decode(
          settingsBackupWire, length, settingsStage)) {
    showNotice("Backup CRC/schema BAD"); return false;
  }
  // Never write even one setting before the complete checksum and whitelist
  // pass. NVS multi-key writes aren't atomic; rollback is best effort.
  captureBackupSettings(settingsRollback);
  if (!writeBackupSettingsNvs(settingsStage)) {
    if (!writeBackupSettingsNvs(settingsRollback))
      showNotice("NVS rollback uncertain");
    else showNotice("NVS restore failed");
    return false;
  }
  volume = settingsStage.volume;
  autoPlay = settingsStage.autoplay;
  currentList = min((int)settingsStage.list, max(0, listCount - 1));
  currentOffset = min((int)settingsStage.station,
                      max(0, (int)lists[currentList].count - 1));
  clockUserOverride = settingsStage.tzOverride;
  clockOffsetMinutes = settingsStage.tzMinutes;
  jazzSparkAutoEnabled = settingsStage.sparkAuto;
  for (int i = 0; i < PRESET_COUNT; ++i) {
    strlcpy(presets[i].url, settingsStage.presetUrl[i], sizeof(presets[i].url));
    strlcpy(presets[i].name, settingsStage.presetName[i], sizeof(presets[i].name));
  }
  if (settingsStage.lastUrl[0]) {
    const int restored = findStationUrl(settingsStage.lastUrl);
    if (restored >= 0)
      for (int i = 0; i < listCount; ++i)
        if (restored >= lists[i].first && restored < lists[i].first + lists[i].count) {
          currentList = i;
          currentOffset = restored - lists[i].first;
          break;
        }
  }
  loadClockSettings();
  prefDirty = false;
  showNotice("Settings restored");
  return true;
}
void restorePrefs() {
  Preferences p;
  if (p.begin("pocketjazz", true)) {
    const uint8_t storedSchema = p.getUChar("schema", 0); // 0: legacy v0.1
    if (!pocketjazz::canWritePreferenceSchema(storedSchema)) {
      // Keep unknown forward NVS intact. Radio remains usable this session
      // without reading potentially incompatible saved IDs/credentials.
      futureNvsSchema = true;
      autoPlay = false;
      p.end();
      Serial.printf("[PocketJazz][NVS] unsupported future schema %u, preserving data\n",
                    (unsigned)storedSchema);
      showNotice("NVS newer: read-only");
      return;
    }
    savedSsid = p.getString("ssid", "");
    savedPassword = p.getString("pass", "");
    // Wi-Fi profile format v1 is independent of the overall NVS schema v2.
    // Read only the last FULLY committed bank; a power cut during writing the
    // other bank must leave the old bank or the legacy singleton usable.
    const uint8_t wifiFormat = p.getUChar("wfcv", 0);
    futureWifiProfileFormat = wifiFormat > 1;
    const uint8_t bank = p.getUChar("wfa", 255);
    if (wifiFormat == 1 && bank < 2) {
      pocketjazz::WifiProfilePolicy candidate;
      bool validBank = true;
      for (int i = 0; i < pocketjazz::WifiProfilePolicy::kCapacity; ++i) {
        const String ssid = p.getString(("w" + String(bank) + String(i) + "s").c_str(), "");
        const String pass = p.getString(("w" + String(bank) + String(i) + "p").c_str(), "");
        if (ssid.isEmpty() && pass.isEmpty()) continue;
        if (!candidate.put(ssid.c_str(), pass.c_str())) { validBank = false; break; }
      }
      if (validBank && candidate.count()) {
        wifiProfiles = candidate;
        const int last = (int)p.getUChar("wflast", 0);
        wifiActiveProfile = wifiProfiles.at(last) ? last : wifiProfiles.at(0) ? 0 : 1;
        const auto* active = wifiProfiles.at(wifiActiveProfile);
        if (active) { savedSsid = active->ssid; savedPassword = active->password; }
      }
    }
    if (!futureWifiProfileFormat && !wifiProfiles.count() &&
        pocketjazz::WifiProfilePolicy::valid(savedSsid.c_str(), savedPassword.c_str())) {
      // Migration is deferred until this legacy network supplies a stable IP.
      wifiProfiles.put(savedSsid.c_str(), savedPassword.c_str());
      wifiActiveProfile = wifiProfiles.find(savedSsid.c_str());
      wifiMigrationPending = true;
    }
    currentList = p.getUChar("list", 0);
    currentOffset = p.getUChar("station", 0);
    volume = p.getUChar("vol", 57);
    autoPlay = p.getBool("autoplay", true);
    vinylCrackleEnabled.store(p.getBool("fxVinyl", false), std::memory_order_relaxed);
    tuningHissEnabled.store(p.getBool("fxHiss", false), std::memory_order_relaxed);
    clockOffsetMinutes = p.getInt("tzMin", 0);
    clockUserOverride = p.getBool("tzSet", false) &&
        pocketjazz::validUtcOffsetMinutes(clockOffsetMinutes);
    jazzSparkAutoEnabled = p.getBool("sparkAuto", true);
    const String rawLastUrl = p.getString("lastUrl", "");
    pocketjazz::copyValidatedSavedStreamUrl(restoredStationUrl,
                                            rawLastUrl.c_str(), rawLastUrl.length());
    for (int i = 0; i < PRESET_COUNT; ++i) {
      String key = "pr" + String(i);
      const String rawPresetUrl = p.getString((key + "u").c_str(), "");
      pocketjazz::copyValidatedSavedStreamUrl(presets[i].url,
                                              rawPresetUrl.c_str(), rawPresetUrl.length());
      put(presets[i].name, p.getString((key + "n").c_str(), ""));
    }
    const int recordedCount = constrain((int)p.getUChar("rCount", 0), 0, RECENT_COUNT);
    for (int i = 0; i < recordedCount; ++i) {
      String key = "rc" + String(i);
      SavedStation candidate{};
      const String rawRecentUrl = p.getString((key + "u").c_str(), "");
      pocketjazz::copyValidatedSavedStreamUrl(candidate.url,
                                              rawRecentUrl.c_str(), rawRecentUrl.length());
      put(candidate.name, p.getString((key + "n").c_str(), ""));
      if (candidate.url[0])
        recent[recentCount++] = candidate;
    }
    p.end();
  }
  if (currentList >= listCount || currentList < 0) currentList = 0;
  if (currentOffset >= lists[currentList].count || currentOffset < 0) currentOffset = 0;
  volume = constrain(volume, 0, 185);
  if (restoredStationUrl[0]) {
    const int idx = findStationUrl(restoredStationUrl);
    if (idx >= 0) {
      for (int i = 0; i < listCount; ++i)
        if (idx >= lists[i].first && idx < lists[i].first + lists[i].count) {
          currentList = i;
          currentOffset = idx - lists[i].first;
          break;
        }
    }
  }
}
bool commitWifiProfiles(const pocketjazz::WifiProfilePolicy& candidate, int activeProfile) {
  if (futureNvsSchema) {
    showNotice("NVS newer: read-only");
    return false;
  }
  if (futureWifiProfileFormat) {
    showNotice("WiFi schema newer");
    return false;
  }
  if (!candidate.at(activeProfile)) return false;
  Preferences p;
  if (!p.begin("pocketjazz", false)) return false;
  const uint8_t former = p.getUChar("wfa", 255);
  const uint8_t target = former == 0 ? 1 : 0;
  bool okay = true;
  for (int i = 0; i < pocketjazz::WifiProfilePolicy::kCapacity; ++i) {
    const String ssidKey = "w" + String(target) + String(i) + "s";
    const String passKey = "w" + String(target) + String(i) + "p";
    const auto* profile = candidate.at(i);
    const char* ssid = profile ? profile->ssid : "";
    const char* password = profile ? profile->password : "";
    // Every staged field is read back before the commit marker changes.
    p.putString(ssidKey.c_str(), ssid);
    p.putString(passKey.c_str(), password);
    if (p.getString(ssidKey.c_str(), "") != ssid ||
        p.getString(passKey.c_str(), "") != password) { okay = false; break; }
  }
  if (okay) {
    okay = p.putUChar("wfcv", 1) == 1 && p.getUChar("wfcv", 0) == 1;
    if (okay) okay = p.putUChar("wflast", (uint8_t)activeProfile) == 1 &&
                     p.getUChar("wflast", 255) == activeProfile;
    // `wfa` is the final commit record. The previous bank stays untouched.
    if (okay) okay = p.putUChar("wfa", target) == 1 &&
                     p.getUChar("wfa", 255) == target;
  }
  if (okay) {
    // Keep old firmware's singleton compatible after the double-bank commit.
    const auto* active = candidate.at(activeProfile);
    p.putString("ssid", active->ssid);
    p.putString("pass", active->password);
  }
  p.end();
  return okay;
}
bool saveWifi() {
  pocketjazz::WifiProfilePolicy candidate = wifiProfiles;
  if (!candidate.put(wifiSsid.c_str(), wifiPassword.c_str())) {
    showNotice("WiFi slots full / invalid");
    return false;
  }
  const int index = candidate.find(wifiSsid.c_str());
  if (!commitWifiProfiles(candidate, index)) {
    showNotice("WiFi NVS write failed");
    return false;
  }
  wifiProfiles = candidate;
  wifiActiveProfile = index;
  wifiMigrationPending = false;
  savedSsid = wifiSsid;
  savedPassword = wifiPassword;
  return true;
}
void connectWifiProfile(int index) {
  const auto* profile = wifiProfiles.at(index);
  if (!profile) return;
  wifiActiveProfile = index;
  WiFi.begin(profile->ssid, profile->password);
  wifiAttempt = millis();
  wifiFirstIpAt = 0;
  wifiReady.store(false);
}
void connectSaved() {
  if (wifiActiveProfile >= 0 && wifiProfiles.at(wifiActiveProfile)) {
    connectWifiProfile(wifiActiveProfile);
  } else if (!savedSsid.isEmpty()) {
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    wifiAttempt = millis();
  }
}

// Everything connected to the audio decoder, source and speaker is owned by one task.
// The UI only sends copied command data, so SD reload never invalidates a playing URL.
void audioWorker(void*) {
  // ~6 KB of audio frame buffers belong in static DRAM, not the FreeRTOS task stack.
  static AudioOutputM5Speaker output(&M5.Speaker, 0);
  output.setup();
  output.setRuntimeProbe(&audioRuntime);
  output.setMonitor(pcmMonitor, &output);
  output.setEnqueueFailureMonitor(pcmEnqueueFailure, nullptr);
  output.setFlushTimingMonitor(pcmFlushTimingCB, nullptr);
  // The 32-KiB ring must be contiguous. The v0.7z4 AFTER-TLS allocation
  // failed on real hardware: largest after TLS was only 31732 B (< 32768).
  // Reserve the ring BEFORE connecting; defer the four libmad workspace
  // regions until prefill. The frame is allocated first to work with a
  // fragmented heap after HTTPS. Release all regions after decoder teardown.
  uint8_t* ring = nullptr;
  uint32_t ringCapacity = STREAM_BYTES;
  Mp3CodecWorkspace codec;
  AudioFileSource* src = nullptr;
  AudioFileSourceBuffer* buffer = nullptr;
  AudioGeneratorMP3* decoder = nullptr;
  bool desired = false;
  char url[192] = {};
  char altUrl[192] = {};
  bool usingAlt = false;
  bool sourceOffline = false;
  uint8_t stationId = 0;
  uint8_t speakerVolume = 57;
  uint32_t currentTelemetrySequence = 0;
  uint32_t stablePlaybackAt = 0;
  uint32_t stableFrameCount = 0;
  uint32_t retryAt = 0;
  // Audio-worker-only, allocation-free input progress observer. It reads a
  // source byte POSITION; it does not call recv(), change buffering or retry.
  pocketjazz::AudioSourceProgress sourceProgress;
  // Optional station-tuning ambience. M5.Speaker still has exactly one
  // application owner (this FreeRTOS task): channel 0=radio, channel 1=FX.
  // The FX starts AFTER synchronous TLS succeeds, never while the worker is
  // blocked in DNS/handshake. No activity on retries or offline MP3.
  bool initialTunePending = false;
  bool profilingStream = false;
  bool firstPcmProfiled = false;
  bool firstStallProfiled = false;
  uint32_t lastHeapPoll = 0;
  bool fxActive = false;
  bool fxFading = false;
  uint32_t fxInitialFrames = 0;
  uint32_t fxFadeStarted = 0;
  uint8_t fxLastVolume = 0;
  const auto sampleHeap = [&](pocketjazz::HeapMoment phase) {
    // Memory queries and locking are done at lifecycle transitions (and at
    // most once every 2 seconds during playback), never in PCM callbacks.
    pocketjazz::HeapCheckpoint snap;
    snap.attempt = audioReopens.load(std::memory_order_relaxed);
    snap.atMs = millis();
    snap.free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snap.largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    snap.freeDma = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    snap.largestDma = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    snap.ringBytes = ring ? ringCapacity : 0;
    snap.bufferedBytes = buffer ? buffer->getFillLevel() : 0;
    snap.sourceBytes = sourceOffline ? 0 : sourceProgress.bytes();
    snap.sourceIdleMs = sourceOffline || !src ? 0 : sourceProgress.idleMs(snap.atMs);
    snap.sourceMaxGapMs = sourceOffline ? 0 : sourceProgress.maximumGapMs();
    heapLifecycle.capture(phase, snap);
  };
  const auto stopTuningFx = [&]() {
    if (fxActive) M5.Speaker.stop(pocketjazz::tuning::kChannel);
    fxActive = false;
    fxFading = false;
  };
  pocketjazz::RadioBufferLevelPolicy bufferLevels;
  auto publishBuffer = [&]() {
    if (!buffer) return;
    bufferLevels.observe(buffer->getFillLevel(), millis());
    audioBufferedBytes.store(bufferLevels.current(), std::memory_order_relaxed);
    audioBufferMinimum.store(bufferLevels.minimum(), std::memory_order_relaxed);
    audioBufferMinAtMs.store(bufferLevels.minAtMs(), std::memory_order_relaxed);
  };
  auto cleanup = [&]() {
    if (profilingStream) sampleHeap(pocketjazz::HeapMoment::BEFORE_FREE);
    stopTuningFx(); // also STOP, station change, failed TLS and decode EOF
    pocketjazz::AudioRuntimeProbe::Scope stage(&audioRuntime, pocketjazz::AudioStage::STOP, millis());
    if (decoder) { decoder->stop(); delete decoder; decoder = nullptr; }
    if (buffer) { buffer->close(); delete buffer; buffer = nullptr; }
    if (src) { src->close(); delete src; src = nullptr; }
    // Decoder and buffer can no longer access the external allocations.
    // Release these also on failed opens/station changes, before the NEXT TLS.
    codec.release();
    if (ring) { free(ring); ring = nullptr; }
    if (profilingStream) {
      sampleHeap(pocketjazz::HeapMoment::AFTER_FREE);
      profilingStream = false;
    }
    sourceProgress.reset(millis());
    ringCapacity = STREAM_BYTES;
    audioRingCapacity.store(STREAM_BYTES, std::memory_order_relaxed);
    // Clear even on STOP and failed opens; never show a deleted buffer's 32767.
    audioInputWaiting.store(false);
    bufferLevels.reset();
    audioBufferedBytes.store(0, std::memory_order_relaxed);
    audioBufferMinimum.store(0, std::memory_order_relaxed);
    audioBufferMinAtMs.store(0, std::memory_order_relaxed);
    decodedMp3FrameBps.store(0, std::memory_order_relaxed);
    decodedMp3ObservedVbr.store(false, std::memory_order_relaxed);
    offlineSourceOpen.store(false, std::memory_order_release);
  };
  auto superseded = [&]() {
    return pocketjazz::audioIntentSuperseded(
        currentTelemetrySequence,
        audioRequestedIntentSequence.load(std::memory_order_acquire));
  };
  const auto serviceTuningFx = [&]() {
    if (!fxActive) return;
    if (!desired || sourceOffline || superseded() ||
        requestedOutputVolume.load(std::memory_order_acquire) == 0 ||
        (!vinylCrackleEnabled.load(std::memory_order_relaxed) &&
         !tuningHissEnabled.load(std::memory_order_relaxed))) {
      stopTuningFx();
      return;
    }
    // The callback increments audioFrames ONLY after channel 0 accepted
    // a PCM block. No M5.Speaker calls from that callback or the UI task.
    if (audioFrames.load(std::memory_order_relaxed) > fxInitialFrames &&
        !fxFading) {
      fxFading = true;
      fxFadeStarted = millis();
    }
    if (!fxFading) return;
    const uint32_t elapsed = millis() - fxFadeStarted;
    if (elapsed >= pocketjazz::tuning::kFadeMs) {
      stopTuningFx();
      return;
    }
    const uint8_t volume = static_cast<uint8_t>(
        (static_cast<uint32_t>(pocketjazz::tuning::kQuietChannelVolume) *
         (pocketjazz::tuning::kFadeMs - elapsed)) /
        pocketjazz::tuning::kFadeMs);
    if (volume != fxLastVolume) {
      M5.Speaker.setChannelVolume(pocketjazz::tuning::kChannel, volume);
      fxLastVolume = volume;
    }
  };
  while (true) {
    audioRuntime.enter(pocketjazz::AudioStage::IDLE, millis());
    AudioCommand cmd;
    if (xQueueReceive(audioQueue, &cmd, 0) == pdTRUE) {
      if (pocketjazz::audioIntentSuperseded(
              cmd.telemetrySequence,
              audioRequestedIntentSequence.load(std::memory_order_acquire))) {
        // This command was overwritten logically by a newer STOP/switch,
        // potentially before the UI published the newer mailbox payload.
        if (decoder || src || buffer) cleanup();
        audioState.store(STOPPED);
        vTaskDelay(1);
        continue;
      }
      const bool changed = strcmp(url, cmd.url) != 0 ||
                           strcmp(altUrl, cmd.altUrl) != 0 ||
                           sourceOffline != cmd.offline || desired != cmd.play ||
                           currentTelemetrySequence != cmd.telemetrySequence;
      desired = cmd.play;
      // The decoder's source kind changes only AFTER cleanup of the old kind.
      stationId = cmd.stationId;
      speakerVolume = cmd.volume;
      M5.Speaker.setVolume(speakerVolume);
      if (changed) {
        cleanup();
        stablePlaybackAt = 0;
        decodedPcmRateHz.store(0);
        sourceOffline = cmd.offline;
        initialTunePending = cmd.play && !cmd.offline;
        currentTelemetrySequence = cmd.telemetrySequence;
        audioAcceptedSequence = currentTelemetrySequence;
        audioPlayout.command(currentTelemetrySequence);
        strlcpy(url, cmd.url, sizeof(url));
        strlcpy(altUrl, cmd.altUrl, sizeof(altUrl));
        usingAlt = false;
        retryAt = 0;
        audioFailureStage.store(0);
        audioSourceStatusCode.store(0);
        audioTlsErrorCode.store(0);
        audioHttpsPhase.store(0);
        audioState.store(STOPPED);
        if (!desired) audioPlayout.stopped(currentTelemetrySequence, millis());
      }
    }
    if (superseded()) {
      // Do not decode an old station while the UI is still publishing its
      // replacement command, even with an empty mailbox.
      if (decoder || src || buffer) cleanup();
      audioState.store(STOPPED);
      vTaskDelay(1);
      continue;
    }
    if (!desired || !url[0]) {
      if (decoder || src || buffer) cleanup();
      audioState.store(STOPPED);
      vTaskDelay(pdMS_TO_TICKS(30));
      continue;
    }
    // Do not open TLS/MP3 sockets during the transient association/DHCP phase.
    if (!sourceOffline && (!wifiReady.load() || WiFi.status() != WL_CONNECTED)) {
      if (decoder || src || buffer) cleanup();
      audioState.store(WAIT_WIFI);
      vTaskDelay(pdMS_TO_TICKS(75));
      continue;
    }
    if (!sourceOffline && strncmp(usingAlt ? altUrl : url, "https://", 8) == 0 &&
        time(nullptr) < MIN_TLS_TIME) {
      audioState.store(WAIT_TIME);
      vTaskDelay(pdMS_TO_TICKS(150));
      continue;
    }
    if (!decoder && sourceOffline) {
      // Manual MP3 only. SD is borrowed by exactly this audio task while
      // OFFLINE_LIBRARY blocks other user SD operations; no concurrent HTTP,
      // Wi-Fi requirement, ESP-NOW, time-shift or automatic fallback.
      static const char prefix[] = "/PocketJazz/offline/";
      const size_t prefixLen = sizeof(prefix) - 1;
      const bool allowedPath = strncmp(url, prefix, prefixLen) == 0 &&
          pocketjazz::OfflineLibraryPolicy<24>::validLeafName(url + prefixLen);
      audioState.store(BUFFERING);
      audioFailureStage.store(0);
      if (allowedPath && sdAvailable) {
        auto* file = new (std::nothrow) AudioFileSourceSD();
        if (file && file->open(url) && file->isOpen()) {
          src = file;
          offlineSourceOpen.store(true, std::memory_order_release);
          uint8_t head[10]{};
          uint32_t contentOffset = 0;
          const uint32_t bytes = file->getSize();
          if (bytes >= pocketjazz::OfflineLibraryPolicy<24>::kMinMp3Bytes &&
              bytes <= pocketjazz::OfflineLibraryPolicy<24>::kMaxMp3Bytes &&
              file->read(head, sizeof(head)) == sizeof(head) &&
              pocketjazz::OfflineLibraryPolicy<24>::id3Start(
                  head, sizeof(head), bytes, contentOffset) &&
              file->seek((int32_t)contentOffset, SEEK_SET)) {
            // Offline MP3 has no HTTP ring; allocate only its decoder workspace.
            decoder = codec.allocate() ? codec.createDecoder() : nullptr;
            if (decoder && decoder->begin(src, &output) && !superseded()) {
              stablePlaybackAt = millis();
              stableFrameCount = audioFrames.load();
              audioState.store(PLAYING);
            } else audioFailureStage.store(codec.ready() ? 3 : 6);
          } else audioFailureStage.store(2); // invalid/truncated ID3 or SD seek
        } else {
          if (file) delete file;
          audioFailureStage.store(1);
        }
      } else audioFailureStage.store(1); // path/SD invalid; never read elsewhere
      if (!decoder) {
        cleanup();
        desired = false; // do not spin/reopen a missing local file
        audioState.store(ERROR_STREAM);
        offlineFinished.store(true, std::memory_order_release);
        vTaskDelay(pdMS_TO_TICKS(30));
        continue;
      }
    }
    if (!decoder && !sourceOffline) {
      if ((int32_t)(millis() - retryAt) < 0) {
        audioRuntime.retry(retryAt, retryAt - (usingAlt ? 400u : 8000u), millis());
        vTaskDelay(pdMS_TO_TICKS(80));
        continue;
      }
      audioState.store(CONNECTING);
      const bool firstUserAttempt = initialTunePending && !usingAlt;
      initialTunePending = false; // no new noise on automatic retries
      audioReopens.fetch_add(1);
      audioRuntime.beginAttempt(audioReopens.load(), millis());
      sourceProgress.reset(millis());
      heapLifecycle.begin(audioReopens.load(std::memory_order_relaxed));
      profilingStream = true;
      firstPcmProfiled = false;
      firstStallProfiled = false;
      lastHeapPoll = millis();
      audioFailureStage.store(0);
      audioSourceStatusCode.store(0);
      audioTlsErrorCode.store(0);
      audioHttpsPhase.store(0);
      audioHttpsHop.store(0);
      audioHttpsDnsOk.store(-1);
      audioHttpsSecureOk.store(-1);
      audioHttpsDnsMs.store(0);
      audioHttpsSecureMs.store(0);
      audioHttpsRequestMs.store(0);
      audioHttpsFreeBefore.store(0);
      audioHttpsLargestBefore.store(0);
      audioHttpsFreeAfter.store(0);
      audioHttpsLargestAfter.store(0);
      const uint32_t openStarted = millis();
      const char* endpoint = usingAlt ? altUrl : url;
      // Fail explicitly instead of reporting an ambiguous prefill error.
      // In particular, do not open TLS if there is no room for the MP3 ring.
      // On real ADV a 32-KiB ring left just 8.6 KiB free and a 1.3-KiB
      // largest block during TLS playback. 16 KiB gives mbedTLS/Wi-Fi an
      // additional 16 KiB to process incoming encrypted records. The full
      // ring remains the default for sufficiently provisioned devices.
      const uint32_t freeBeforeRing = ESP.getFreeHeap();
      sampleHeap(pocketjazz::HeapMoment::PRE_RING);
      ringCapacity = freeBeforeRing < STREAM_FULL_RING_MIN_HEAP
                         ? STREAM_BYTES_CONSTRAINED : STREAM_BYTES;
      ring = static_cast<uint8_t*>(malloc(ringCapacity));
      // A fragmented heap may reject 32 KiB despite sufficient total free.
      if (!ring && ringCapacity == STREAM_BYTES) {
        ringCapacity = STREAM_BYTES_CONSTRAINED;
        ring = static_cast<uint8_t*>(malloc(ringCapacity));
      }
      if (ring) audioRingCapacity.store(ringCapacity, std::memory_order_relaxed);
      sampleHeap(pocketjazz::HeapMoment::RING_HELD);
      if (!ring) {
        audioFailureStage.store(5);  // CONTIGUOUS MP3 RING ALLOCATION FAILED
        audioRuntime.fault(millis(), 5, 0, 0, ESP.getFreeHeap(),
                           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        cleanup();
        audioState.store(ERROR_STREAM);
        retryAt = millis() + 8000;
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      // No Serial in the audio task: even a UART printf can block PCM feed.
      // Raw user URLs (possibly containing tokens) never enter diagnostics.
      // HTTPS must use NetworkClientSecure with explicit root trust and
      // hostname verification. The bundled ICY stream uses plain NetworkClient
      // even for https:// on Arduino-ESP32 3.x; never pass HTTPS to that class.
      SecureICYSource* secureSrc = nullptr;
      AudioFileSourceICYStream* plainSrc = nullptr;
      if (strncmp(endpoint, "https://", 8) == 0) {
        secureSrc = new (std::nothrow) SecureICYSource(nullptr, kRadioTrustedRootsPem);
        src = secureSrc;
      }
      else if (strncmp(endpoint, "http://", 7) == 0)
        src = plainSrc = new (std::nothrow) AudioFileSourceICYStream();
      if (src) src->RegisterStatusCB(audioStatusCB, nullptr);
      if (src) src->RegisterMetadataCB(icyMetadataCB, &stationId);
      // Stay at the Arduino loopTask's priority (1) throughout connection,
      // prefill and playback. On this board both tasks run on core 1.
      // Raising audio to 2 after TLS used to give a continuously-fed decoder
      // precedence over every keyboard/LCD pass, potentially making all
      // controls unresponsive on stations that never stop supplying MP3.
      if (secureSrc) secureSrc->setRuntimeProbe(&audioRuntime);
      audioRuntime.enter(pocketjazz::AudioStage::OPEN, millis());
      sampleHeap(pocketjazz::HeapMoment::PRE_OPEN);
      bool opened = src && src->open(endpoint);
      if (!opened && plainSrc && plainSrc->httpsUpgrade()[0] &&
          !superseded()) {
        // http -> https upgrade is NOT automatically supported by SDK
        // HTTPClient::setURL(). Validate Location in plain source, CLOSE its
        // socket, then open ONE verified CA/hostname TLS connection; never a
        // concurrent second network client or an implicit https -> http path.
        char upgradedUrl[192];
        strlcpy(upgradedUrl, plainSrc->httpsUpgrade(), sizeof(upgradedUrl));
        src->close();
        delete src;
        src = nullptr;
        plainSrc = nullptr;
        secureSrc = new (std::nothrow) SecureICYSource(nullptr, kRadioTrustedRootsPem);
        src = secureSrc;
        if (src) {
          src->RegisterStatusCB(audioStatusCB, nullptr);
          src->RegisterMetadataCB(icyMetadataCB, &stationId);
          secureSrc->setRuntimeProbe(&audioRuntime);
          opened = src->open(upgradedUrl);
        }
      }
      const bool alive = opened && src->isOpen();
      sampleHeap(pocketjazz::HeapMoment::OPEN_RESULT);
      if (!alive && !secureSrc) audioRuntime.fault(millis(), 1, audioSourceStatusCode.load(),
          secureSrc ? secureSrc->lastTlsError() : 0, ESP.getFreeHeap(),
          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      // Do NOT elevate the decoder above loopTask after a successful GET.
      // The existing cooperative vTaskDelay(1) at the end of every decode
      // iteration gives the same-priority UI task opportunities to scan keys.
      if (superseded()) {
        // A STOP/station switch may have arrived during synchronous GET().
        // A volume-only update is NOT a reason to discard a valid GET.
        // NetworkClient has no safe cross-task handshake cancellation; this
        // check bounds stale playback but not latency of a still-running GET.
        cleanup();
        vTaskDelay(1);
        continue;
      }
      if (secureSrc) {
        const RadioHttpsTrace trace = secureSrc->lastHttpsTrace();
        audioHttpsPhase.store(trace.phase);
        audioHttpsHop.store(trace.hop);
        audioHttpsDnsOk.store(trace.dnsOk);
        audioHttpsSecureOk.store(trace.secureOk);
        audioHttpsDnsMs.store(trace.dnsMs);
        audioHttpsSecureMs.store(trace.secureMs);
        audioHttpsRequestMs.store(trace.requestMs);
        audioHttpsFreeBefore.store(trace.freeBefore);
        audioHttpsLargestBefore.store(trace.largestBefore);
        audioHttpsFreeAfter.store(trace.freeAfter);
        audioHttpsLargestAfter.store(trace.largestAfter);
        if (opened && !alive) audioHttpsPhase.store(HTTPS_POST_CONNECT);
      }
      if (alive) {
        audioRuntime.enter(pocketjazz::AudioStage::PREFILL, millis());
        // Only the first deliberate station connection may play the quiet
        // effect, after verified HTTPS (or HTTP) and before decoder PCM.
        // Channel 1 is deliberately skipped when the heap cannot afford it.
        if (firstUserAttempt && desired &&
            requestedOutputVolume.load(std::memory_order_acquire) != 0) {
          const bool vinyl = vinylCrackleEnabled.load(std::memory_order_relaxed);
          const bool hiss = tuningHissEnabled.load(std::memory_order_relaxed);
          const int8_t* clip = pocketjazz::tuning::selectClip(vinyl, hiss);
          if (clip && ESP.getFreeHeap() >= pocketjazz::tuning::kMinimumFreeHeap &&
              heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >=
                  pocketjazz::tuning::kMinimumLargestBlock) {
            fxInitialFrames = audioFrames.load(std::memory_order_relaxed);
            fxLastVolume = pocketjazz::tuning::kQuietChannelVolume;
            M5.Speaker.setChannelVolume(pocketjazz::tuning::kChannel, fxLastVolume);
            fxActive = M5.Speaker.playRaw(clip, pocketjazz::tuning::kSamples,
                pocketjazz::tuning::kRateHz, false, 0,
                pocketjazz::tuning::kChannel, true);
          }
        }
        buffer = ring ? new (std::nothrow) AudioFileSourceBuffer(src, ring, ringCapacity)
                      : nullptr;
        if (buffer) {
          // The MP3 task owns a ~35-ms PCM queue, but HTTPS read() may wait
          // 500 ms on empty TCP. Consume available bytes only; defer frames
          // with bounded live-stall watchdog instead of blocking speaker feed.
          buffer->SetCooperativeSourceRead(true);
          buffer->SetSourcePollMonitor(sourcePullTimingCB, nullptr);
          buffer->RegisterStatusCB(bufferStatusCB, nullptr);
          audioState.store(BUFFERING);
          uint8_t dummy;
          buffer->read(&dummy, 0);  // prefill like the M5Unified WebRadio example
          // A 200 OK is not proof that MP3 bytes have arrived. Start the
          // PRE-FILL clock after the successful TLS/HTTP open and initial
          // source-buffer read: a 4-second TLS handshake must not steal the
          // 5-second MP3 arrival window.
          const uint32_t prefillStarted = millis();
          // Same meaningful startup reserve for 16/32-KiB input rings.
          // Previously 32 KiB started at 2 KiB (~125 ms at 128 kbps),
          // even though a bigger ring provides no benefit before it fills.
          // Bound the wait by the pre-existing 5-second startup budget.
          const uint32_t prefillGoal =
              pocketjazz::initialMp3PrefillGoal(ringCapacity);
          while (pocketjazz::shouldKeepPrefilling(millis(), prefillStarted,
                                                  buffer->getFillLevel(), prefillGoal,
                                                  MP3_PREFILL_TIMEOUT_MS) &&
                 wifiReady.load() && !superseded()) {
            serviceTuningFx(); // reacts to MUTE without audio queue dequeue
            buffer->loop();
            vTaskDelay(pdMS_TO_TICKS(15));
          }
          sourceProgress.observe(millis(), src->getPos());
          sampleHeap(pocketjazz::HeapMoment::PREFILL_DONE);
          serviceTuningFx();
          if (superseded()) {
            // STOP or new station arrived during the prefill; never begin
            // decoding the old request just because 512 bytes arrived.
            cleanup();
            vTaskDelay(1);
            continue;
          }
          if (!pocketjazz::hasPlayablePrefill(buffer->getFillLevel(), 512)) {
            audioFailureStage.store(7); // source opened but no MP3 bytes arrived
            cleanup();
          } else {
            publishBuffer(); // first valid minimum, AFTER MP3 prefill
            if (superseded()) {
              // A STOP can arrive after prefill but before decoder setup.
              cleanup();
              vTaskDelay(1);
              continue;
            }
            decoder = codec.allocate() ? codec.createDecoder() : nullptr;
            // Prevent a temporary 500-ms network gap from becoming false EOF
            // inside ESP8266Audio's MAD input. The same explicit 4s bound as
            // SecureICYSource's HTTP I/O timeout avoids unending silence.
            if (decoder) decoder->SetLiveStallTimeoutMs(4000);
            const bool decoderReady = decoder && decoder->begin(buffer, &output);
            sampleHeap(pocketjazz::HeapMoment::CODEC_RESULT);
            if (superseded()) {
              // begin() configures MAD/output but produces no PCM; recheck
              // after it to avoid publishing stale PLAYING and PCM work.
              cleanup();
              vTaskDelay(1);
              continue;
            }
            if (decoderReady) {
              audioFailureStage.store(0);
              audioState.store(PLAYING);
              stablePlaybackAt = millis();
              stableFrameCount = audioFrames.load();
              audioLastOpenDurationMs.store(millis() - openStarted);
            } else {
              audioFailureStage.store(codec.ready() ? 3 : 6); // decoder or codec RAM
              cleanup();
            }
          }
        } else {
          audioFailureStage.store(2);
        }
      } else {
        // Capture mbedTLS status BEFORE cleanup deletes the failed HTTPS client.
        // Zero does not prove or disprove a TCP/DNS failure.
        if (secureSrc)
          audioTlsErrorCode.store(secureSrc->lastTlsError(), std::memory_order_relaxed);
        audioFailureStage.store(1);
      }
      if (!decoder) {
        if (alive) audioRuntime.fault(millis(), audioFailureStage.load(),
            audioSourceStatusCode.load(), audioTlsErrorCode.load(), ESP.getFreeHeap(),
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        audioLastOpenDurationMs.store(millis() - openStarted);
        const bool exhausted = usingAlt || !altUrl[0];
        if (exhausted && wifiReady.load() &&
            uxQueueMessagesWaiting(audioQueue) == 0)
          stationHealth.failed(stationId);
        cleanup();
        audioState.store(ERROR_STREAM);
        if (!usingAlt && altUrl[0]) {
          usingAlt = true;
          retryAt = millis() + 400;
        } else {
          usingAlt = false;
          retryAt = millis() + 8000;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
    }
    if (superseded()) {
      // The UI may have issued STOP between the last prefill check and this
      // decoder loop. No stale frame should enter the speaker if already known.
      cleanup();
      audioState.store(STOPPED);
      vTaskDelay(1);
      continue;
    }
    audioStackWatermark.store(uxTaskGetStackHighWaterMark(nullptr));
    const uint32_t decodeLoopStarted = micros();
    audioRuntime.enter(pocketjazz::AudioStage::DECODE, millis());
    const bool decoderHealthy = decoder->isRunning() && decoder->loop();
    // Read only the source's already-maintained MP3 byte position. No extra
    // network read, packet inspection, SD operation or wakeup is introduced.
    if (src && !sourceOffline)
      sourceProgress.observe(millis(), src->getPos());
    serviceTuningFx(); // fade after FIRST accepted real PCM, never before
    audioRuntime.beat(millis());
    audioInputWaiting.store(decoder->IsWaitingForLiveInput());
    pocketjazz::observeMaxMicros(audioDecodeLoopMaxUs,
                                 pocketjazz::elapsedMicros(decodeLoopStarted, micros()));
    publishBuffer(); // measure drain AFTER decoder->loop, before any cleanup
    if (profilingStream) {
      if (!firstPcmProfiled &&
          audioFrames.load(std::memory_order_relaxed) != stableFrameCount) {
        firstPcmProfiled = true;
        sampleHeap(pocketjazz::HeapMoment::FIRST_PCM);
      }
      if (!firstStallProfiled && decoder->IsWaitingForLiveInput()) {
        firstStallProfiled = true;
        sampleHeap(pocketjazz::HeapMoment::FIRST_STALL);
      }
      const uint32_t nowHeap = millis();
      if ((uint32_t)(nowHeap - lastHeapPoll) >= 2000u) {
        sampleHeap(pocketjazz::HeapMoment::LOW_FREE);
        sampleHeap(pocketjazz::HeapMoment::LOW_BLOCK);
        lastHeapPoll = nowHeap;
      }
    }
    decodedMp3FrameBps.store(decoder->GetDecodedFrameBitrateBps(), std::memory_order_relaxed);
    decodedMp3ObservedVbr.store(decoder->HasObservedVariableBitrate(), std::memory_order_relaxed);
    if (!decoderHealthy) {
      audioRuntime.fault(millis(), 4, audioSourceStatusCode.load(), audioTlsErrorCode.load(),
          ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      audioFailureStage.store(4);
      if (sourceOffline) {
        // Do not mistake corrupted/short SD input for successful MP3 EOF.
        // A false loop() may be a normal finite stream end or a decoder error.
        const bool reachedEnd = src && src->isOpen() &&
                                src->getPos() >= src->getSize();
        cleanup();
        desired = false;
        audioState.store(reachedEnd ? STOPPED : ERROR_STREAM);
        if (reachedEnd) audioFailureStage.store(0);
        offlineFinished.store(true, std::memory_order_release);
        vTaskDelay(pdMS_TO_TICKS(30));
        continue;
      }
      cleanup();
      audioState.store(ERROR_STREAM);
      const bool exhausted = usingAlt || !altUrl[0];
      if (exhausted && wifiReady.load() &&
          uxQueueMessagesWaiting(audioQueue) == 0)
        stationHealth.failed(stationId);
      if (!usingAlt && altUrl[0]) {
        usingAlt = true;
        retryAt = millis() + 400;
      } else {
        usingAlt = false;
        retryAt = millis() + 8000;
      }
    } else {
      // This is a bounded, genuine live-input stall, not a socket disconnect.
      // Expose BUFFERING instead of falsely reporting continuous PLAYING.
      audioState.store(decoder->IsWaitingForLiveInput() ? BUFFERING : PLAYING);
      // Never clear a station's failure warning merely because HTTP returned
      // 200. Require sustained decoding with actual PCM accepted by speaker.
      if (stablePlaybackAt && !decoder->IsWaitingForLiveInput() &&
          (uint32_t)(millis() - stablePlaybackAt) >= 5000 &&
          (uint32_t)(audioFrames.load() - stableFrameCount) >= 8192) {
        stationHealth.recovered(stationId);
        stablePlaybackAt = 0;
      }
    }
    audioPlayout.poll(currentTelemetrySequence, millis());
    vTaskDelay(1);
  }
}

void loadPeekConfig() {
  peekOptIn = false;
  if (!sdAvailable) return;
  File file = SD.open("/PocketJazz/peek.cfg", FILE_READ);
  if (!file) return;
  if (file.size() > 12) { file.close(); return; }
  String line = file.readStringUntil('\n');
  file.close();
  line.trim();
  // Exact deliberate opt-in. No broker passwords, URLs, secrets or NVS writes.
  peekOptIn = line == "PEEK=1";
}

struct PeekCapture {
  char title[96]{};
  bool received = false;
};
void peekMetadataCB(void* context, const char* type, bool, const char* value) {
  PeekCapture* capture = static_cast<PeekCapture*>(context);
  if (!capture || capture->received || !type || strcmp(type, "StreamTitle") || !value) return;
  pocketjazz::log::cleanTsvField(value, capture->title, sizeof(capture->title));
  capture->received = capture->title[0] != 0;
}
bool peekRequestCurrent(const PeekCommand& command) {
  return peekWanted.load() &&
         command.playlistGeneration == peekLiveGeneration.load() &&
         command.requestId != 0 && command.requestId == peekLiveRequest.load();
}

// Explicitly optional, low-priority, ONE connection at a time. No LCD/SD,
// playback callbacks or audio-buffer ownership. A blocked TLS open can finish
// after the browse screen closes; its result is not acted upon as a track.
void stationPeekWorker(void*) {
  PeekCommand command{};
  while (true) {
    if (xQueueReceive(peekQueue, &command, portMAX_DELAY) != pdTRUE) continue;
    PeekResult result{};
    result.stationId = command.stationId;
    result.playlistGeneration = command.playlistGeneration;
    result.requestId = command.requestId;
    if (!peekRequestCurrent(command)) continue;
    const uint32_t priorUnderruns = audioUnderruns.load();
    const int state = audioState.load();
    const bool audioSafe = state == STOPPED ||
        (state == PLAYING && audioBufferedBytes.load() >= 24576u);
    if (peekRequestCurrent(command) && wifiReady.load() && audioSafe &&
        ESP.getFreeHeap() >= pocketjazz::StationPeekPolicy<>::kMinFreeHeap &&
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >=
            pocketjazz::StationPeekPolicy<>::kMinLargestBlock) {
      AudioFileSource* source = nullptr;
      if (strncmp(command.url, "https://", 8) == 0 && time(nullptr) >= MIN_TLS_TIME) {
        SecureICYSource* secure = new (std::nothrow) SecureICYSource(nullptr, kRadioTrustedRootsPem);
        if (secure) {
          secure->setPeekLimits(); // verified CA/hostname; at most one redirect
          source = secure;
        }
      }
      // The upstream plain-HTTP ICY decoder can loop indefinitely on an
      // incomplete metadata block. Never use it for optional background
      // probes: only the bounded, verified HTTPS implementation is eligible.
      // Plain HTTP radio playback itself remains unchanged.
      if (source) {
        PeekCapture capture{};
        source->RegisterMetadataCB(peekMetadataCB, &capture);
        if (peekRequestCurrent(command) && source->open(command.url) &&
            source->isOpen() && peekRequestCurrent(command)) {
          // Read just enough MP3 bytes for the first metadata block. Never
          // decode these bytes, save them or append them to the main history.
          uint8_t scratch[256];
          size_t bytesRead = 0;
          const uint32_t started = millis();
          while (!capture.received && bytesRead < 65536u &&
                 (uint32_t)(millis() - started) < 3500u &&
                 peekRequestCurrent(command) && wifiReady.load() &&
                 audioUnderruns.load() == priorUnderruns && source->isOpen()) {
            const int currentState = audioState.load();
            if (currentState != STOPPED &&
                (currentState != PLAYING || audioBufferedBytes.load() < 8192u)) break;
            const uint32_t got = source->readNonBlock(scratch, sizeof(scratch));
            bytesRead += got;
            vTaskDelay(pdMS_TO_TICKS(got ? 1 : 20));
          }
        }
        if (capture.received && peekRequestCurrent(command))
          strlcpy(result.title, capture.title, sizeof(result.title));
        source->close();
        delete source;
      }
    }
    // Result queue is UI-only; a late result is permitted but NEVER changes
    // the currently playing station or its metadata/history.
    if (peekRequestCurrent(command)) xQueueOverwrite(peekResultQueue, &result);
  }
}

void invalidateStationPeek(bool playlistReload) {
  // This invalidation comes BEFORE changing/reusing station IDs. It does not
  // wait for a TLS call to return: the worker discards its old token later.
  peekWanted.store(false);
  peekLiveRequest.store(0);
  if (playlistReload) {
    uint32_t generation = peekLiveGeneration.load() + 1;
    if (!generation) generation = 1;
    peekLiveGeneration.store(generation);
    stationPeek.clear();
  } else stationPeek.cancel();
  // Queues are bounded and do not own any pointers or opened sockets.
  // Draining cannot interrupt an in-flight worker; tokens fence its result.
  if (peekQueue) {
    PeekCommand discarded{};
    while (xQueueReceive(peekQueue, &discarded, 0) == pdTRUE) {}
  }
  if (peekResultQueue) {
    PeekResult discarded{};
    while (xQueueReceive(peekResultQueue, &discarded, 0) == pdTRUE) {}
  }
}

int browseFirstVisible() {
  const int count = listCount ? lists[browseList].count : 0;
  return max(0, min(browseSelected - 1, max(0, count - 4)));
}
void serviceStationPeek(uint32_t now) {
  const bool allowed = screen == BROWSE && peekOptIn;
  if (!allowed && (peekWanted.load() || stationPeek.busy()))
    invalidateStationPeek(false);
  else peekWanted.store(allowed);
  if (!peekOptIn || !peekQueue || !peekResultQueue) return;
  PeekResult completed{};
  if (xQueueReceive(peekResultQueue, &completed, 0) == pdTRUE)
    stationPeek.complete(completed.stationId, completed.title, now,
                         completed.playlistGeneration, completed.requestId);
  if (!peekWanted.load() || !wifiReady.load() || !listCount ||
      audioUnderruns.load() != peekUnderflowsAtOpen) return;
  const int state = audioState.load();
  if (state != STOPPED && (state != PLAYING || audioBufferedBytes.load() < 24576u)) return;
  int visible[4] = {-1, -1, -1, -1};
  const int first = browseFirstVisible();
  const int count = lists[browseList].count;
  for (int i = 0; i < 4 && first + i < count; ++i)
    visible[i] = lists[browseList].first + first + i;
  const int candidate = stationPeek.next(visible, 4, stationIndex(), now,
      true, true, ESP.getFreeHeap(),
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  if (candidate < 0 || candidate >= stationCount) return;
  PeekCommand request{};
  request.stationId = (uint8_t)candidate;
  request.playlistGeneration = peekLiveGeneration.load();
  request.requestId = ++peekNextRequest;
  if (!request.requestId) request.requestId = ++peekNextRequest;
  strlcpy(request.url, stations[candidate].url, sizeof(request.url));
  // Validate the token at both ends of the FreeRTOS queue, never station ID.
  peekLiveRequest.store(request.requestId);
  if (xQueueSend(peekQueue, &request, 0) == pdTRUE) {
    if (!stationPeek.start(candidate, now, request.playlistGeneration,
                           request.requestId)) peekLiveRequest.store(0);
  } else peekLiveRequest.store(0);
}

struct Press {
  String chars;
  bool enter = false;
  bool del = false;
  bool tab = false;
  bool fn = false;
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
};
// Pinned M5Cardputer 1.1.1 does NOT have KeysState.up/down.
// The ADV arrow keycaps occupy ; (up), , (left), . (down), / (right).
// In navigation contexts those keys act as arrows with or without Fn;
// in text-entry contexts punctuation remains available as text.
bool heldArrow(const std::vector<char>& word, char normal, char shifted) {
  for (char c : word) if (c == normal || c == shifted) return true;
  return false;
}
Press readKeys() {
  Press input;
  auto& k = M5Cardputer.Keyboard.keysState();
  const bool arrowUp = heldArrow(k.word, ';', ':');
  const bool arrowDown = heldArrow(k.word, '.', '>');
  const bool arrowLeft = heldArrow(k.word, ',', '<');
  const bool arrowRight = heldArrow(k.word, '/', '?');
  input.up = arrowUp && !lastArrowUp;
  input.down = arrowDown && !lastArrowDown;
  input.left = arrowLeft && !lastArrowLeft;
  input.right = arrowRight && !lastArrowRight;
  for (const char c : k.word) {
    bool old = false;
    for (const char p : lastWord) if (p == c) { old = true; break; }
    if (!old) input.chars += c;
  }
  input.enter = k.enter && !lastEnter;
  input.del = k.del && !lastDelete;
  input.tab = k.tab && !lastTab;
  input.fn = k.fn;
  lastWord = k.word;
  lastEnter = k.enter;
  lastDelete = k.del;
  lastTab = k.tab;
  lastArrowUp = arrowUp;
  lastArrowDown = arrowDown;
  lastArrowLeft = arrowLeft;
  lastArrowRight = arrowRight;
  return input;
}
bool pressed(const Press& k, char c) {
  return k.chars.indexOf(c) >= 0 || k.chars.indexOf((char)toupper(c)) >= 0;
}
void beginWifiScan() {
  networkCount = selectedNetwork = 0;
  screen = WIFI_LIST;
  wifiScanPending = true;
  if (wifiAutoScanPending) {
    // Reuse the pending asynchronous scan instead of launching two scans.
    wifiAutoScanPending = false;
  } else WiFi.scanNetworks(true, false);
}
void serviceWifi() {
  const uint32_t now = millis();
  const uint32_t ipEvents = wifiGotIpEvents.load();
  const uint32_t disconnects = wifiDisconnectEvents.load();
  if (ipEvents != reportedIpEvents) {
    reportedIpEvents = ipEvents;
    Serial.printf("[PocketJazz][WiFi] GOT_IP count=%lu ip=%s rssi=%d elapsed=%lu ms\n",
                  (unsigned long)ipEvents, WiFi.localIP().toString().c_str(),
                  WiFi.RSSI(), (unsigned long)(now - wifiAttempt));
  }
  if (disconnects != reportedDisconnects) {
    reportedDisconnects = disconnects;
    // A disconnect/reconnect between loop iterations must restart stabilization.
    wifiFirstIpAt = 0;
    wifiReady.store(false);
    Serial.printf("[PocketJazz][WiFi] DISCONNECTED count=%lu reason=%d sinceBoot=%lu ms\n",
                  (unsigned long)disconnects, wifiDisconnectReason.load(),
                  (unsigned long)wifiDisconnectAt.load());
  }
  if (wifiHasIp()) {
    if (!wifiFirstIpAt) wifiFirstIpAt = now;
    if (!wifiReady.load() && now - wifiFirstIpAt >= WIFI_STABLE_MS) {
      wifiReady.store(true);
      wifiFailedProfile = -1;
      if (wifiMigrationPending && !wifiMigrationAttempted &&
          wifiActiveProfile >= 0 && !wifiJoinPending) {
        // A legacy singleton is migrated only after actual 2-second stable IP.
        // Retry on the next explicit join/reboot, not every UI tick if NVS fails.
        wifiMigrationAttempted = true;
        if (commitWifiProfiles(wifiProfiles, wifiActiveProfile))
          wifiMigrationPending = false;
      }
      if (!ntpStarted) {
        esp_sntp_set_time_sync_notification_cb(onNtpSync);
        // configTime(0,0,...) calls setTimeZone(0,0) in pinned Arduino-ESP32
        // 3.3.9, silently resetting the user's previously applied TZ to UTC0.
        // Keep the saved NVS fixed offset / SD POSIX TZ (including DST) while
        // starting exactly the same SNTP servers and TLS time sync callback.
        configTzTime(clockTz, "pool.ntp.org", "time.google.com");
        ntpStarted = true;
        Serial.println("[PocketJazz][Time] SNTP started for HTTPS certificate validation");
      }
      Serial.printf("[PocketJazz][WiFi] STABLE ip=%s rssi=%d heap=%lu\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                    (unsigned long)ESP.getFreeHeap());
    }
  } else {
    wifiFirstIpAt = 0;
    wifiReady.store(false);
  }
  if (wifiScanPending || wifiAutoScanPending) {
    const int n = WiFi.scanComplete();
    if (n >= 0) {
      networkCount = min(n, MAX_NETWORKS);
      pocketjazz::WifiScanCandidate discovered[MAX_NETWORKS]{};
      for (int i = 0; i < networkCount; ++i) {
        networkNames[i] = WiFi.SSID(i);
        networkRssi[i] = (int16_t)WiFi.RSSI(i);
        discovered[i] = { networkNames[i].c_str(), networkRssi[i] };
      }
      const bool autoSelection = wifiAutoScanPending && !wifiScanPending;
      wifiScanPending = false;
      wifiAutoScanPending = false;
      const int candidate = autoSelection ? wifiProfiles.choose(
          discovered, (size_t)networkCount, wifiFailedProfile) : -1;
      WiFi.scanDelete();
      if (autoSelection) {
        if (candidate >= 0) {
          wifiFailedProfile = -1;
          connectWifiProfile(candidate);
        } else connectSaved();
      }
    } else if (n == WIFI_SCAN_FAILED) {
      wifiScanPending = false;
      if (wifiAutoScanPending) {
        wifiAutoScanPending = false;
        connectSaved();
      }
    } else if (wifiAutoScanPending && now - lastWifiAutoScanAt >= 12000) {
      // A broken driver scan must not strand the only reconnect path forever.
      wifiAutoScanPending = false;
      wifiScanPending = false;
      WiFi.scanDelete();
      connectSaved();
    }
  }
  if (wifiJoinPending) {
    if (wifiReady.load()) {
      wifiJoinPending = false;
      saveWifi();
      screen = RADIO;
      sendAudio();
    } else if (now - wifiAttempt > WIFI_TIMEOUT_MS &&
               (!wifiHasIp() || now - wifiFirstIpAt > WIFI_STABLE_MS + 1500)) {
      wifiJoinPending = false;
      wifiJoinFailed = true;
      screen = WIFI_PASSWORD;
      Serial.printf("[PocketJazz][WiFi] JOIN TIMEOUT lastReason=%d ip=%s\n",
                    wifiDisconnectReason.load(), WiFi.localIP().toString().c_str());
    }
  } else if (screen == RADIO && wifiProfiles.count() && !wifiHasIp() &&
             !wifiScanPending && !wifiAutoScanPending &&
             now - wifiAttempt >= WIFI_RETRY_MS) {
    if (wifiProfiles.count() >= 2) {
      wifiFailedProfile = wifiActiveProfile;
      wifiAutoScanPending = true;
      lastWifiAutoScanAt = now;
      wifiAttempt = now;
      WiFi.scanNetworks(true, false); // disconnected; never scan continuously during playback
    } else connectSaved();
  }
}
void beginWifiJoin() {
  wifiReady.store(false);
  wifiFirstIpAt = 0;
  wifiJoinFailed = false;
  WiFi.disconnect(false, false);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  wifiAttempt = millis();
  wifiJoinPending = true;
  screen = WIFI_JOIN;
}
void handleKeys(const Press& k) {
  // A single explicit Enter always silences a sounding alarm, even if the
  // owner subsequently navigated to Help, Clock or Wi-Fi screens. No timer
  // may resume it that same civil day after this manual STOP.
  if (dailyAlarmSounding && k.enter) {
    dailyAlarmSounding = false;
    dailyAlarmRamping = false;
    wantPlay = false;
    sendAudio();
    screen = RADIO;
    showNotice("Alarm stopped");
    return;
  }
  // Volume is a global playback control, independent of the current screen.
  // Keep literal '-' and '=' available in password/search text fields; Fn
  // with either key changes volume there instead of inserting punctuation.
  const bool volumeDown = pressed(k, '-') || pressed(k, '_');
  const bool volumeUp = pressed(k, '=') || pressed(k, '+');
  const bool typingText = screen == WIFI_PASSWORD || screen == SEARCH ||
                          (screen == NOTES_SEARCH && !notesSearchRan);
  if (volumeDown != volumeUp && (!typingText || k.fn)) {
    const int step = volumeUp ? 8 : -8;
    if (dailyAlarmSounding) {
      // A sounding alarm has a temporary target separate from normal volume.
      dailyAlarmTargetVolume = (uint8_t)constrain(
          (int)dailyAlarmTargetVolume + step, 0, 80);
      if (dailyAlarmRamping) {
        dailyAlarmOutputVolume = pocketjazz::DailyRadioAlarm::fadeVolume(
            millis() - dailyAlarmRampAt, dailyAlarmTargetVolume);
        sendAudio();
      }
      showNotice("Alarm level adjusted");
    } else {
      const int nextVolume = constrain(volume + step, 0, 185);
      if (nextVolume != volume) {
        volume = nextVolume;
        markPrefs();
        sendAudio(); // volume-only mailbox update; no change of stream intent
      }
    }
    return;
  }
  if (screen == DAILY_ALARM) {
    if (pressed(k, 'x') || (k.del && (futureNvsSchema || dailyAlarmFutureFormat))) {
      // Explicit escape from an unwritable future NVS schema. Never trap the
      // user in settings or mutate an unknown schema.
      dailyAlarmEdit = dailyAlarmConfig;
      screen = RADIO;
      return;
    }
    if (k.del) {
      if (dailyAlarmEdit.enabled == dailyAlarmConfig.enabled &&
          dailyAlarmEdit.hour == dailyAlarmConfig.hour &&
          dailyAlarmEdit.minute == dailyAlarmConfig.minute &&
          dailyAlarmEdit.preset == dailyAlarmConfig.preset) {
        screen = RADIO; return; // no unnecessary NVS rewrite
      }
      if (saveDailyRadioAlarmConfig()) { screen = RADIO; showNotice("Alarm saved"); }
      return;
    }
    if (k.up) dailyAlarmEdit.hour = (dailyAlarmEdit.hour + 1) % 24;
    if (k.down) dailyAlarmEdit.hour = (dailyAlarmEdit.hour + 23) % 24;
    if (k.right) dailyAlarmEdit.minute = (dailyAlarmEdit.minute + 1) % 60;
    if (k.left) dailyAlarmEdit.minute = (dailyAlarmEdit.minute + 59) % 60;
    for (int slot = 0; slot < PRESET_COUNT; ++slot)
      if (pressed(k, char('1' + slot))) {
        if (!presets[slot].url[0]) showNotice("Save preset first");
        else dailyAlarmEdit.preset = slot;
        return;
      }
    if (k.enter) {
      if (!dailyAlarmEdit.enabled &&
          (!presets[dailyAlarmEdit.preset].url[0] ||
           findStationUrl(presets[dailyAlarmEdit.preset].url) < 0)) {
        showNotice("Alarm preset missing");
        return;
      }
      dailyAlarmEdit.enabled = !dailyAlarmEdit.enabled;
      if (!saveDailyRadioAlarmConfig()) dailyAlarmEdit.enabled = !dailyAlarmEdit.enabled;
      else showNotice(dailyAlarmEdit.enabled ? "Daily alarm ON" : "Daily alarm OFF");
    }
    return;
  }
  if (screen == WIFI_LIST) {
    if (k.up) selectedNetwork = max(0, selectedNetwork - 1);
    if (k.down) selectedNetwork = min(max(0, networkCount - 1), selectedNetwork + 1);
    if (k.enter && networkCount) {
      wifiSsid = networkNames[selectedNetwork];
      const int known = wifiProfiles.find(wifiSsid.c_str());
      if (known >= 0 && !k.fn) {
        // Previously confirmed profile: reconnect without exposing the secret.
        wifiPassword = wifiProfiles.at(known)->password;
        beginWifiJoin();
      } else {
        // Fn+Enter lets the owner replace a previously saved password, but
        // never commits the new secret before stable IP is observed.
        wifiPassword = "";
        wifiShowPassword = false;
        screen = WIFI_PASSWORD;
      }
    }
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == WIFI_PASSWORD) {
    if (k.del && wifiPassword.length()) wifiPassword.remove(wifiPassword.length() - 1);
    else if (k.del) screen = WIFI_LIST;
    if (k.tab) wifiShowPassword = !wifiShowPassword;
    if (!k.fn)
      for (const char c : k.chars) if (wifiPassword.length() < 63) wifiPassword += c;
    if (k.enter) beginWifiJoin();
    return;
  }
  if (screen == WIFI_JOIN) {
    if (k.del) {
      wifiJoinPending = false;
      WiFi.disconnect(false, false);
      screen = WIFI_LIST;
    }
    return;
  }
  if (screen == HELP) {
    if (k.up)
      helpPage = (helpPage + HELP_PAGE_COUNT - 1) % HELP_PAGE_COUNT;
    else if (k.down)
      helpPage = (helpPage + 1) % HELP_PAGE_COUNT;
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == CATALOG_UPDATE) {
    if (k.del) { screen = RADIO; return; }
    if (k.enter && !catalogUpdateInProgress) {
      // No other task may hold the SD source or streaming TLS buffers. A
      // manual update is an explicit STOP-only foreground operation.
      const bool stopped = !wantPlay && audioState.load() == STOPPED &&
                           !offlineMode && !offlineLeaving &&
                           !offlineSourceOpen.load(std::memory_order_acquire) &&
                           !stationPeek.busy();
      catalogUpdateInProgress = true;
      drawCatalogUpdate(); // feedback before synchronous network/SD work
      catalogUpdateStatus = pocketjazz::updateCatalogManually(stopped, sdAvailable);
      catalogUpdateAttempted = true;
      catalogUpdateInProgress = false;
      if (catalogUpdateStatus == pocketjazz::CatalogUpdateResult::ReadyToActivate) {
        // Invalidate ID-tied metadata before station IDs are reallocated.
        // Retain the selected station by URL if it is still in the new list.
        char selectedUrl[192]{};
        const int selected = stationIndex();
        if (selected >= 0) strlcpy(selectedUrl, stations[selected].url,
                                  sizeof(selectedUrl));
        invalidateStationPeek(true);
        const bool reloaded = loadPlaylists();
        // Distinguish successfully downloaded+verified SD bytes from genuine
        // activation in the UI: missing SD, builtin fallback and incomplete
        // parsing all fail this gate. Pending transaction is still reversible.
        const bool activated = reloaded && catalogDefaultListsLoadedMask == 0x1fu &&
            pocketjazz::catalogActivationMatches(catalogDefaultStationCounts);
        catalogUpdateStatus = pocketjazz::finalizeCatalogUpdate(activated);
        if (!activated || catalogUpdateStatus != pocketjazz::CatalogUpdateResult::Ok) {
          // On an activation failure the updater rolls back the five originals
          // if SD is accessible. If it is not, pending recovery is retried on
          // next successful SD mount, BEFORE scanning playlists.
          if (catalogUpdateStatus == pocketjazz::CatalogUpdateResult::RecoveryFailed)
            catalogRecoveryChecked = false;
          loadPlaylists();
        }
        currentList = currentOffset = 0;
        if (selectedUrl[0]) {
          const int retained = findStationUrl(selectedUrl);
          if (retained >= 0)
            for (int i = 0; i < listCount; ++i)
              if (retained >= lists[i].first &&
                  retained < lists[i].first + lists[i].count) {
                currentList = i;
                currentOffset = retained - lists[i].first;
                break;
              }
        }
        if (catalogUpdateStatus == pocketjazz::CatalogUpdateResult::Ok)
          markPrefs(); // never rewrite user playlists or presets
      }
      uiPainted = false;
    }
    return;
  }
  if (screen == DIAGNOSTICS) {
    if (pressed(k, 'e')) { if (diagPage == 7) exportAudioRuntime(); else exportDiagnostics(); return; }
    if (pressed(k, 'm') && diagPage == 6) { exportHeapLifecycle(); return; }
    if (pressed(k, 'x')) {
      // X resets the entire *displayed audio history*, not just PCM gaps.
      // Keep the raw atomics monotonic for audio/peek/Serial control paths;
      // do not restart playback or clear instantaneous failures/heap state.
      diagAudioWindow.reset(audioUnderruns.load(), audioReopens.load(),
                            audioFrames.load());
      audioPlayout.resetCounters();
      uiLoopMaxGapMs.store(0);
      audioSourcePullMaxUs.store(0);
      audioDecodeLoopMaxUs.store(0);
      audioPcmFlushMaxUs.store(0);
      showNotice("Audio counters reset");
      return;
    }
    if (k.up)
      diagPage = (diagPage + DIAG_PAGE_COUNT - 1) % DIAG_PAGE_COUNT;
    else if (k.down)
      diagPage = (diagPage + 1) % DIAG_PAGE_COUNT;
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == SETTINGS_BACKUP) {
    if (k.del) { backupConfirmation = 0; screen = RADIO; return; }
    if (pressed(k, 'e')) {
      if (backupConfirmation == 1) {
        backupConfirmation = 0;
        exportSettingsBackup();
      } else {
        backupConfirmation = 1;
        showNotice("Press E again to export");
      }
    } else if (pressed(k, 'i')) {
      if (backupConfirmation == 2) {
        backupConfirmation = 0;
        importSettingsBackup();
      } else {
        backupConfirmation = 2;
        showNotice("Press I again to import");
      }
    } else backupConfirmation = 0;
    return;
  }
  if (screen == OFFLINE_LIBRARY) {
    if (k.del) {
      offlineLeaving = true;
      wantPlay = false;
      sendAudio(); // cleanup on audio task; never close its borrowed File here
      return;
    }
    if (offlineLeaving) return;
    if (k.up) offlineSelected = max(0, offlineSelected - 1);
    if (k.down) offlineSelected = min((int)offlineCatalog.count() - 1, offlineSelected + 1);
    if (k.enter) {
      const auto* item = offlineCatalog.at((size_t)offlineSelected);
      char proposed[192]{};
      if (!item || !pocketjazz::OfflineLibraryPolicy<24>::makePath(
              item->filename, proposed, sizeof(proposed))) {
        showNotice("Invalid local MP3"); return;
      }
      if (offlineMode && wantPlay && strcmp(offlinePath, proposed) == 0) {
        wantPlay = false; // one Enter STOP, next Enter restarts from first frame
      } else {
        strlcpy(offlinePath, proposed, sizeof(offlinePath));
        offlineMode = true;
        wantPlay = true;
      }
      sendAudio();
      return;
    }
    return;
  }
  if (screen == RECENTS) {
    if (k.up) recentSelected = max(0, recentSelected - 1);
    if (k.down) recentSelected = min(max(0, recentCount - 1), recentSelected + 1);
    if (k.enter && recentCount) {
      if (selectByUrl(recent[recentSelected].url)) screen = RADIO;
      else showNotice("Station not in list");
    }
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == BROWSE) {
    if (k.del) {
      invalidateStationPeek(false);
      screen = RADIO;
      return;
    }
    if (pressed(k, 'p') && listCount > 1) {
      invalidateStationPeek(false);
      browseList = (browseList + 1) % listCount;
      browseSelected = 0;
      return;
    }
    if (k.up) {
      invalidateStationPeek(false);
      browseSelected = max(0, browseSelected - 1);
    }
    if (k.down) {
      invalidateStationPeek(false);
      browseSelected = min(max(0, (int)lists[browseList].count - 1), browseSelected + 1);
    }
    if (k.enter && lists[browseList].count) {
      const int id = lists[browseList].first + browseSelected;
      if (id >= 0 && id < stationCount && selectByUrl(stations[id].url)) {
        invalidateStationPeek(false);
        screen = RADIO;
      } else showNotice("Station not found");
    }
    return;
  }
  if (screen == SEARCH) {
    if (k.del) {
      if (!stationSearch.length()) screen = RADIO;
      else stationSearch.remove(stationSearch.length() - 1);
      return;
    }
    if (k.up) searchSelected = max(0, searchSelected - 1);
    else if (k.down)
      searchSelected = min(max(0, searchMatchCount() - 1), searchSelected + 1);
    else if (!k.fn && !k.up && !k.down) {
      for (const char c : k.chars) {
        if (stationSearch.length() >= 24) break;
        if (isPrintable(c) && c != '\t') stationSearch += c;
      }
    }
    searchSelected = min(searchSelected, max(0, searchMatchCount() - 1));
    if (k.enter) {
      const int idx = searchMatch(searchSelected);
      if (idx >= 0 && selectByUrl(stations[idx].url)) screen = RADIO;
      else showNotice("No matching station");
    }
    return;
  }
  if (screen == MOOD) {
    if (k.del) { screen = RADIO; return; }
    if (k.left) moodEnergyCursor = max(0, (int)moodEnergyCursor - 1);
    if (k.right) moodEnergyCursor = min(4, (int)moodEnergyCursor + 1);
    if (k.up) moodAcousticCursor = min(4, (int)moodAcousticCursor + 1);
    if (k.down) moodAcousticCursor = max(0, (int)moodAcousticCursor - 1);
    if (k.enter) {
      const int idx = closestMoodStation();
      if (idx < 0) showNotice("No mood tags in M3U");
      else if (selectByUrl(stations[idx].url)) screen = RADIO;
    }
    return;
  }
  if (screen == NOTES) {
    if (k.up) notesDetailPage = 0;
    if (k.down)
      notesDetailPage = notesDetail.length() > 126 ? 1 : 0;
    if (k.del)
      screen = notesOpenedFromSearch ? NOTES_SEARCH : RADIO;
    return;
  }
  if (screen == NOTES_SEARCH) {
    if (k.del) {
      if (notesSearchRan) {
        notesSearchRan = false;  // Back to text editing, without losing query.
        return;
      }
      if (notesSearchQuery.length()) {
        notesSearchQuery.remove(notesSearchQuery.length() - 1);
        notesSearchRan = false;
        notesSearchCount = notesSearchTotal = 0;
      } else screen = RADIO;
      return;
    }
    if (notesSearchRan && notesSearchTotal &&
        k.up) {
      notesSearchSelected = max(0, notesSearchSelected - 1);
      if (static_cast<size_t>(notesSearchSelected) < notesSearchOffset)
        pageOfflineNotes((static_cast<size_t>(notesSearchSelected) / 3) * 3);
    } else if (notesSearchRan && notesSearchTotal &&
               k.down) {
      notesSearchSelected = min(static_cast<int>(notesSearchTotal - 1),
                                notesSearchSelected + 1);
      if (static_cast<size_t>(notesSearchSelected) >= notesSearchOffset + notesSearchCount)
        pageOfflineNotes((static_cast<size_t>(notesSearchSelected) / 3) * 3);
    }
    else if (!k.fn) {
      if (notesSearchRan) return; // Results mode: W/S navigate, DEL edit.
      for (const char c : k.chars) {
        // Index is intentionally ASCII-only. Reject misleading Unicode or
        // non-printable SD queries rather than inventing transliteration.
        if (notesSearchQuery.length() >= 24) break;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '-') {
          notesSearchQuery += c;
          notesSearchRan = false;
          notesSearchCount = notesSearchTotal = 0;
        }
      }
    }
    if (k.enter) {
      if (!notesSearchRan) searchOfflineNotes();
      else if (notesSearchCount) openSelectedOfflineNote();
      else searchOfflineNotes();
    }
    return;
  }
  if (screen == SPARK) {
    jazzSparkAutoVisible = false; // A user interaction takes control of the view.
    if (k.del) { screen = RADIO; return; }
    if (pressed(k, 't')) {
      if (!futureNvsSchema) {
        jazzSparkAutoEnabled = !jazzSparkAutoEnabled;
        saveSparkAutoSetting();
      } else showNotice("NVS newer: read-only");
      return;
    }
    if (k.up) showJazzSpark(-1);
    if (k.down) showJazzSpark(1);
    return;
  }
  if (screen == TRACKS) {
    if (pressed(k, 'e')) { exportIcyHistory(); return; }
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == SHARE) {
    if (k.up) shareQrPage = (shareQrPage + 2) % 3;
    if (k.down) shareQrPage = (shareQrPage + 1) % 3;
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == TEMPO) {
    if (k.del) { screen = RADIO; return; }
    if (pressed(k, 'c')) { manualTempo.reset(); return; }
    if (pressed(k, 'j')) manualTempo.tap(millis());
    return;
  }
  if (screen == CLOCK) {
    if (k.up) adjustClockOffset(30);
    if (k.down) adjustClockOffset(-30);
    if (pressed(k, '0')) {
      if (!futureNvsSchema) {
        clockUserOverride = false;
        clockOffsetMinutes = 0;
        loadClockSettings();
        saveClockOffset();
      }
    }
    if (k.del) screen = RADIO;
    return;
  }
  if (screen == SOUND_FX) {
    if (k.del) { screen = RADIO; return; }
    if (pressed(k, '1')) { saveTuningEffect(true); return; }
    if (pressed(k, '2')) { saveTuningEffect(false); return; }
    return;
  }
  // The compact radio footer points to help; always open the first page,
  // where station navigation, playback and volume are explained.
  if (pressed(k, 'h')) { helpPage = 0; screen = HELP; return; }
  if (pressed(k, 'd')) {
    catalogUpdateAttempted = false;
    catalogUpdateStatus = pocketjazz::CatalogUpdateResult::NoChange;
    screen = CATALOG_UPDATE;
    return;
  }
  if (pressed(k, 's')) { screen = SOUND_FX; return; }
  if (pressed(k, 'z')) { jazzSparkAutoVisible = false; showJazzSpark(0); return; }
  if (pressed(k, 'i')) { diagPage = 0; screen = DIAGNOSTICS; return; }
  if (pressed(k, 'l')) { recentSelected = 0; screen = RECENTS; return; }
  if (pressed(k, 'f')) {
    invalidateStationPeek(false); // never adopt a reply from an earlier visit
    browseList = currentList;
    browseSelected = currentOffset;
    peekUnderflowsAtOpen = audioUnderruns.load();
    screen = BROWSE;
    return;
  }
  if (pressed(k, 'g')) { screen = MOOD; return; }
  if (pressed(k, 'k')) { likeCurrentTrack(); return; }
  if (pressed(k, 'n')) { notesOpenedFromSearch = false; loadArtistNotes(); screen = NOTES; return; }
  if (pressed(k, 'u')) {
    notesOpenedFromSearch = false;
    notesSearchQuery = "";
    notesSearchCount = notesSearchTotal = 0;
    notesSearchSelected = 0;
    notesSearchRan = false;
    screen = NOTES_SEARCH;
    return;
  }
  if (pressed(k, 'y')) { screen = TRACKS; return; }
  if (pressed(k, 'q')) { shareQrPage = 0; screen = SHARE; return; }
  if (pressed(k, 'j')) {
    manualTempo.reset();
    manualTempo.tap(millis());  // initial J is the first beat, not a mode toggle
    screen = TEMPO;
    return;
  }
  if (pressed(k, 'c')) { screen = CLOCK; return; }
  if (pressed(k, 'b')) { backupConfirmation = 0; screen = SETTINGS_BACKUP; return; }
  if (pressed(k, '0')) { enterOfflineLibrary(); return; }
  if (k.tab) {
    stationSearch = "";
    searchSelected = 0;
    screen = SEARCH;
    return;
  }
  if (pressed(k, 'w')) { beginWifiScan(); return; }
  // The ADV keyboard reports Fn separately, keeping the actual digit in word.
  // Never trigger a preset while entering Wi-Fi credentials (handled above).
  for (int i = 0; i < PRESET_COUNT; ++i) {
    if (!pressed(k, char('1' + i))) continue;
    if (k.fn) savePreset(i);
    else if (presets[i].url[0]) {
      if (!selectByUrl(presets[i].url)) showNotice("Preset URL missing");
      else showNotice("Preset selected");
    } else showNotice("Preset empty");
    return;
  }
  if (dailyAlarmSounding && pressed(k, 'm')) {
    dailyAlarmSounding = false;
    dailyAlarmRamping = false;
    wantPlay = false;
    sendAudio();
    showNotice("Alarm stopped");
    return;
  }
  if (pressed(k, 'm')) { muted = !muted; sendAudio(); showNotice(muted ? "Muted" : "Sound on"); return; }
  if (pressed(k, 'o')) {
    autoPlay = !autoPlay;
    markPrefs();
    showNotice(autoPlay ? "Autoplay ON" : "Autoplay OFF");
    return;
  }
  if (k.fn && pressed(k, 't')) {
    dailyAlarmEdit = dailyAlarmConfig;
    screen = DAILY_ALARM;
    return;
  }
  if (pressed(k, 't')) {
    sleepMinutes = sleepMinutes == 0 ? 30 : sleepMinutes == 30 ? 45 : sleepMinutes == 45 ? 60 : 0;
    sleepStartedAt = millis();
    if (sleepMinutes) {
      char sleepNotice[24]{};
      snprintf(sleepNotice, sizeof(sleepNotice), "SLEEP: %u MIN", (unsigned)sleepMinutes);
      showNotice(sleepNotice);
    } else showNotice("SLEEP: OFF");
    return;
  }
  if (pressed(k, 'r')) {
    const bool wasPlaying = wantPlay;
    char previousUrl[192] = {};
    const int previous = stationIndex();
    if (previous >= 0) strlcpy(previousUrl, stations[previous].url, sizeof(previousUrl));
    wantPlay = false;
    sendAudio();
    invalidateStationPeek(true); // advance generation BEFORE station ID reuse
    loadPlaylists();
    currentList = currentOffset = 0;
    if (previousUrl[0]) {
      const int restored = findStationUrl(previousUrl);
      if (restored >= 0)
        for (int i = 0; i < listCount; ++i)
          if (restored >= lists[i].first && restored < lists[i].first + lists[i].count) {
            currentList = i;
            currentOffset = restored - lists[i].first;
            break;
          }
    }
    wantPlay = wasPlaying;
    markPrefs(); sendAudio();
    return;
  }
  if (pressed(k, 'p') && listCount > 1) {
    currentList = (currentList + 1) % listCount;
    currentOffset = 0;
    markPrefs(); sendAudio();
  }
  if (k.left) {
    const int count = lists[currentList].count;
    if (count) currentOffset = (currentOffset + count - 1) % count;
    markPrefs(); sendAudio();
  }
  if (k.right) {
    const int count = lists[currentList].count;
    if (count) currentOffset = (currentOffset + 1) % count;
    markPrefs(); sendAudio();
  }
  if (k.enter) {
    wantPlay = !wantPlay;
    sendAudio();
  }
}
void serviceDailyRadioAlarm(uint32_t now) {
  if (dailyAlarmSounding) {
    // Manual STOP and SD library retain precedence; do not resurrect playback.
    if (!wantPlay || offlineMode) {
      dailyAlarmSounding = false;
      dailyAlarmRamping = false;
      return;
    }
    const uint32_t frames = audioFrames.load(std::memory_order_relaxed);
    if (!dailyAlarmRamping && audioState.load() == PLAYING &&
        frames != dailyAlarmFramesAtRequest) {
      // Start the audible 60-second envelope only after successful PCM handoff.
      dailyAlarmRamping = true;
      dailyAlarmRampAt = now;
    }
    if (!dailyAlarmRamping &&
        (uint32_t)(now - dailyAlarmRequestedAt) >=
            pocketjazz::DailyRadioAlarm::kStartTimeoutMs) {
      // No automatic switch to a private SD file or indefinite network retry.
      dailyAlarmSounding = false;
      wantPlay = false;
      sendAudio();
      showNotice("Alarm: stream failed");
      return;
    }
    if (dailyAlarmRamping) {
      const uint8_t newGain = pocketjazz::DailyRadioAlarm::fadeVolume(
          now - dailyAlarmRampAt, dailyAlarmTargetVolume);
      if (newGain != dailyAlarmOutputVolume) {
        dailyAlarmOutputVolume = newGain;
        sendAudio(); // volume-only command; existing decoder/ES8311 task
      }
    }
    return;
  }
  if (!dailyAlarmConfig.enabled || safeMode ||
      futureNvsSchema || dailyAlarmFutureFormat ||
      !ntpSynced.load(std::memory_order_acquire)) return;
  const time_t utc = time(nullptr);
  if (utc < MIN_TLS_TIME) return;
  tm local{};
  if (!localtime_r(&utc, &local)) return;
  if (!pocketjazz::DailyRadioAlarm::due(
          dailyAlarmConfig, true, local.tm_year + 1900, local.tm_yday,
          local.tm_hour, local.tm_min, local.tm_sec, dailyAlarmLastFiredDay)) return;
  // Avoid interrupting user-directed SD I/O, credential entry or settings.
  // In those modes the minute is deliberately skipped, not deferred blindly.
  if (offlineMode || offlineLeaving || offlineSourceOpen.load() || !audioQueue ||
      (screen != RADIO && screen != CLOCK)) return;
  const int32_t today = pocketjazz::DailyRadioAlarm::civilDayKey(
      local.tm_year + 1900, local.tm_yday);
  if (!claimDailyRadioAlarmDay(today)) {
    dailyAlarmConfig.enabled = false; // fail closed for this boot
    showNotice("Alarm day save failed");
    return;
  }
  const uint8_t slot = dailyAlarmConfig.preset;
  if (slot >= PRESET_COUNT || !presets[slot].url[0] ||
      findStationUrl(presets[slot].url) < 0) {
    showNotice("Alarm preset missing");
    return;
  }
  // Single owner: all source changes and volume updates go through sendAudio
  // to audioWorker. No direct M5.Speaker operations in the calendar/UI loop.
  dailyAlarmSounding = true;
  dailyAlarmRamping = false;
  dailyAlarmRequestedAt = now;
  dailyAlarmFramesAtRequest = audioFrames.load(std::memory_order_relaxed);
  dailyAlarmOutputVolume = 0;
  dailyAlarmTargetVolume = pocketjazz::DailyRadioAlarm::safeTarget(volume);
  sleepMinutes = 0; // an older sleep timer cannot immediately silence alarm
  if (!selectByUrl(presets[slot].url)) {
    dailyAlarmSounding = false;
    showNotice("Alarm station missing");
    return;
  }
  showNotice("Daily radio alarm");
}
void serviceSleepTimer(uint32_t now) {
  if (!sleepMinutes) return;
  if ((uint32_t)(now - sleepStartedAt) < (uint32_t)sleepMinutes * 60000UL) return;
  sleepMinutes = 0;
  wantPlay = false;
  sendAudio();
  showNotice("Sleep timer ended");
}
void serviceOfflineCompletion() {
  if (offlineFinished.exchange(false, std::memory_order_acq_rel)) {
    wantPlay = false;
    // The task has ended playback without a user command. The next Enter
    // must publish a NEW generation instead of reusing an old latency start.
    lastRequestedAudioPlay = false;
    if (screen == OFFLINE_LIBRARY && !offlineLeaving)
      showNotice(audioState.load() == ERROR_STREAM ? "MP3/SD read error" : "MP3 completed");
  }
  if (offlineLeaving && !offlineSourceOpen.load(std::memory_order_acquire) &&
      audioState.load() == STOPPED) {
    offlineLeaving = false;
    offlineMode = false;
    offlinePath[0] = 0;
    screen = RADIO;
  }
}
void serviceMetadata() {
  if (offlineMode) return; // old network titles must not appear in local MP3
  if (!metadataQueue) return;
  TrackMetadata incoming{};
  if (xQueueReceive(metadataQueue, &incoming, 0) != pdTRUE) return;
  if (incoming.stationId != stationIndex()) return;
  strlcpy(currentTitle, incoming.title, sizeof(currentTitle));
  const int idx = stationIndex();
  if (idx >= 0 && audioState.load() == PLAYING)
    trackHistory.add(stations[idx].name, incoming.title, millis());
}
String normalizeArtistKey(const String& artist) {
  String key;
  bool separated = false;
  for (size_t i = 0; i < artist.length(); ++i) {
    const char c = artist[i];
    if (c >= 'A' && c <= 'Z') { key += (char)(c + ('a' - 'A')); separated = false; }
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      key += c; separated = false;
    } else if (!key.isEmpty() && !separated) { key += '-'; separated = true; }
    if (key.length() > 48) return "";
  }
  while (key.endsWith("-")) key.remove(key.length() - 1);
  return key;
}
uint32_t artistKeyHash(const String& key) {
  uint32_t hash = 0x811c9dc5u;
  for (size_t i = 0; i < key.length(); ++i)
    hash = (hash ^ (uint8_t)key[i]) * 0x01000193u;
  return hash;
}
bool verifyArtistTsvHash(File& file, const String& hexDigest) {
  if (hexDigest.length() != 64 || file.size() > 32768 || !file.seek(0)) return false;
  mbedtls_sha256_context sha{};
  mbedtls_sha256_init(&sha);
  bool okay = mbedtls_sha256_starts(&sha, 0) == 0;
  uint8_t chunk[256];
  while (okay && file.available()) {
    const int n = file.read(chunk, sizeof(chunk));
    if (n <= 0) { okay = false; break; }
    okay = mbedtls_sha256_update(&sha, chunk, n) == 0;
  }
  uint8_t digest[32]{};
  if (okay) okay = mbedtls_sha256_finish(&sha, digest) == 0;
  mbedtls_sha256_free(&sha);
  if (!file.seek(0) || !okay) return false;
  char actual[65];
  for (int i = 0; i < 32; ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
  actual[64] = 0;
  return hexDigest == actual;
}
bool artistIndexOffset(File& file, const String& key, uint32_t& offset) {
  File index = SD.open("/PocketJazz/artist-notes.idx", FILE_READ);
  if (!index) return false;
  if (index.size() > 8192) { index.close(); return false; }
  const String header = index.readStringUntil('\n');
  bool found = false;
  if (header.startsWith("#PJ-NOTES-IDX\t1\tfnv1a32\t")) {
    const int digestAt = 23;  // full prefix length; locate using separators below
    (void)digestAt;
    const int third = header.indexOf('\t', header.indexOf('\t', header.indexOf('\t') + 1) + 1);
    const int fourth = third < 0 ? -1 : header.indexOf('\t', third + 1);
    const String expected = (third >= 0 && fourth > third)
        ? header.substring(third + 1, fourth) : String();
    const int count = fourth < 0 ? -1 : header.substring(fourth + 1).toInt();
    if (count > 0 && count <= 50 && verifyArtistTsvHash(file, expected)) {
      char target[9];
      snprintf(target, sizeof(target), "%08lx", (unsigned long)artistKeyHash(key));
      for (int i = 0; i < count && index.available(); ++i) {
        String row = index.readStringUntil('\n');
        if (row.length() > 90) break;
        row.trim();
        const int first = row.indexOf('\t');
        const int second = first < 0 ? -1 : row.indexOf('\t', first + 1);
        if (first != 8 || second < 0) continue;
        if (row.substring(0, first) != target ||
            row.substring(second + 1) != key) continue; // hash collisions
        const String decimal = row.substring(first + 1, second);
        if (decimal.isEmpty()) continue;
        bool digits = true;
        for (const char digit : decimal)
          if (digit < '0' || digit > '9') digits = false;
        if (!digits) continue;
        const uint32_t parsed = decimal.toInt();
        if (parsed < file.size()) { offset = parsed; found = true; break; }
      }
    }
  }
  index.close();
  return found;
}
void loadArtistNotes() {
  notesFound = false;
  notesDetailPage = 0;
  notesArtist = ""; notesDetail = ""; notesAlbum = ""; notesSource = "";
  if (!sdAvailable || !currentTitle[0]) return;
  String title(currentTitle);
  // Only a sufficiently explicit "Artist - Track" ICY pattern may be used;
  // do not infer an artist from a bare program title or station name.
  const int dash = title.indexOf(" - ");
  if (dash <= 0 || dash > 96) return;
  notesArtist = title.substring(0, dash);
  notesArtist.trim();
  const String key = normalizeArtistKey(notesArtist);
  if (key.isEmpty()) return;
  File file = SD.open("/PocketJazz/artist-notes.tsv", FILE_READ);
  if (!file) {
    // Standalone 1400+ musician database is a separate SD file; legacy TSV
    // remains optional. Only the explicit N action performs this SD lookup.
    File rich = SD.open("/PocketJazz/jazz-encyclopedia.tsv", FILE_READ);
    if (!rich) return;
    pocketjazz::NotesFile reader = {&rich, static_cast<size_t>(rich.size()), readNotesFile};
    pocketjazz::JazzEncyclopedia db;
    pocketjazz::JazzBiography hit{};
    if (db.open(reader) && db.find(key.c_str(), hit)) {
      notesArtist = hit.name;
      notesDetail = hit.summary;
      notesAlbum = hit.album[0] == '-' ? "" : String(hit.album);
      notesSource = hit.source;
      notesFound = true;
    }
    rich.close();
    return;
  }
  uint32_t offset = 0;
  if (artistIndexOffset(file, key, offset)) {
    if (file.seek(offset)) {
      String indexed = file.readStringUntil('\n');
      const int divider = indexed.indexOf('\t');
      if (divider > 0 && indexed.length() <= 640 &&
          indexed.substring(0, divider) == key) {
        // Exact-key verification after seeking protects hash collisions and
        // corrupted offsets. Read the row through the common parser below.
        file.seek(offset);
      } else file.seek(0);
    } else file.seek(0);
  } else file.seek(0);
  int checked = 0;
  while (file.available() && ++checked <= 50) {
    String row = file.readStringUntil('\n');
    if (row.length() > 640) continue;
    row.trim();
    const int first = row.indexOf('\t');
    if (first <= 0 || row.substring(0, first) != key) continue;
    const int second = row.indexOf('\t', first + 1);
    const int third = row.indexOf('\t', second + 1);
    const int fourth = row.indexOf('\t', third + 1);
    if (second < 0 || third < 0 || fourth < 0) continue;
    notesDetail = row.substring(first + 1, second);
    notesAlbum = row.substring(second + 1, third);
    notesSource = row.substring(third + 1, fourth);
    notesFound = true;
    break;
  }
  file.close();
  // Prefer the more informative new encyclopedia if its entry exists. The
  // legacy MusicBrainz card remains a fallback for a missing/corrupt SD DB.
  File rich = SD.open("/PocketJazz/jazz-encyclopedia.tsv", FILE_READ);
  if (rich) {
    pocketjazz::NotesFile reader = {&rich, static_cast<size_t>(rich.size()), readNotesFile};
    pocketjazz::JazzEncyclopedia db;
    pocketjazz::JazzBiography hit{};
    if (db.open(reader) && db.find(key.c_str(), hit)) {
      notesArtist = hit.name;
      notesDetail = hit.summary;
      if (hit.album[0] != '-') notesAlbum = hit.album;
      notesSource = hit.source;
      notesFound = true;
    }
    rich.close();
  }
}
bool readNotesFile(void* context, size_t offset, uint8_t* dst, size_t length) {
  File* file = static_cast<File*>(context);
  if (!file || !dst || offset > file->size() ||
      length > file->size() - offset || !file->seek(offset)) return false;
  return file->read(dst, length) == length;
}
// SD operations are allowed ONLY in explicit user-directed Notes searches,
// never on the radio/ICY/audio worker or as part of periodic display drawing.
void pageOfflineNotes(size_t offset) {
  notesSearchCount = notesSearchTotal = 0;
  notesSearchOffset = offset;
  if (!sdAvailable || !notesSearchQuery.length()) return;
  File encyclopedia = SD.open("/PocketJazz/jazz-encyclopedia.tsv", FILE_READ);
  if (encyclopedia) {
    pocketjazz::NotesFile reader = {&encyclopedia, static_cast<size_t>(encyclopedia.size()), readNotesFile};
    pocketjazz::JazzEncyclopedia db;
    size_t stored = 0, total = 0;
    const bool valid = db.open(reader) &&
        db.search(notesSearchQuery.c_str(), notesSearchPage.useEncyclopedia(), 3,
                  offset, stored, total);
    encyclopedia.close();
    notesSearchEncyclopedia = true;
    if (valid) {
      notesSearchCount = stored;
      notesSearchTotal = total;
    }
    return;  // Corrupt modern corpus: do NOT silently mix invalid rows.
  }
  notesSearchEncyclopedia = false;
  // Legacy 36-card MusicBrainz file as backwards-compatible fallback.
  File tsv = SD.open("/PocketJazz/artist-notes.tsv", FILE_READ);
  if (!tsv) return;
  File idx = SD.open("/PocketJazz/artist-notes.idx", FILE_READ);
  pocketjazz::NotesFile tsvReader = {&tsv, static_cast<size_t>(tsv.size()), readNotesFile};
  pocketjazz::NotesFile idxReader = {&idx, idx ? static_cast<size_t>(idx.size()) : 0,
                                    idx ? readNotesFile : nullptr};
  static pocketjazz::JazzNotesSearch searchIndex; // ~3KB static, not UI task stack
  if (searchIndex.open(tsvReader, idxReader))
    notesSearchCount = searchIndex.search(notesSearchQuery.c_str(),
        notesSearchPage.useLegacy(), 3, &notesSearchTotal, offset);
  idx.close();
  tsv.close();
}
void searchOfflineNotes() {
  notesSearchSelected = 0;
  notesSearchRan = true;
  pageOfflineNotes(0);
}
void openSelectedOfflineNote() {
  if (notesSearchSelected < 0 ||
      static_cast<size_t>(notesSearchSelected) < notesSearchOffset ||
      static_cast<size_t>(notesSearchSelected) >= notesSearchOffset + notesSearchCount) return;
  const size_t row = static_cast<size_t>(notesSearchSelected) - notesSearchOffset;
  if (notesSearchEncyclopedia) {
    const pocketjazz::JazzBiography& entry = notesSearchPage.encyclopedia(row);
    notesArtist = entry.name;
    notesDetail = entry.summary;
    notesAlbum = entry.album[0] == '-' ? "" : String(entry.album);
    notesSource = entry.source;
    notesFound = true;
    notesOpenedFromSearch = true;
    notesDetailPage = 0;
    screen = NOTES;
    return;
  }
  const pocketjazz::JazzNote& result = notesSearchPage.legacy(row);
  notesArtist = String(result.key);
  notesArtist.replace('-', ' ');
  notesDetail = result.description;
  notesAlbum = result.album;
  notesSource = result.artistUrl;
  notesFound = true;
  notesOpenedFromSearch = true;
  notesDetailPage = 0;
  screen = NOTES;
}
void showJazzSpark(int8_t direction) {
  jazzSparkReady = false;
  if (!sdAvailable) { showNotice("Insert jazz-facts SD file"); return; }
  File file = SD.open("/PocketJazz/jazz-facts.tsv", FILE_READ);
  if (!file) { showNotice("Missing jazz-facts.tsv"); return; }
  pocketjazz::NotesFile reader = {&file, static_cast<size_t>(file.size()), readNotesFile};
  pocketjazz::JazzFacts corpus;
  if (corpus.open(reader) && corpus.size()) {
    jazzSparkTotal = corpus.size();
    if (direction == 0) {
      // Quotes are in the final 5 validated rows of the bundled corpus.
      // Choose one around 25% of the time rather than almost never.
      const uint32_t entropy = esp_random();
      jazzSparkIndex = (jazzSparkTotal > 5 && entropy % 4 == 0)
          ? jazzSparkTotal - 5 + ((entropy >> 2) % 5)
          : ((entropy >> 2) % jazzSparkTotal);
    } else if (direction > 0) jazzSparkIndex = (jazzSparkIndex + 1) % jazzSparkTotal;
    else jazzSparkIndex = (jazzSparkIndex + jazzSparkTotal - 1) % jazzSparkTotal;
    jazzSparkReady = corpus.at(jazzSparkIndex, jazzSpark);
  }
  file.close();
  if (!jazzSparkReady) showNotice("Jazz facts SD invalid");
  else screen = SPARK;
}
void serviceJazzSpark(uint32_t now) {
  if (jazzSparkAutoVisible) {
    if (screen != SPARK || static_cast<int32_t>(now - autoSparkUntil) >= 0) {
      jazzSparkAutoVisible = false;
      if (screen == SPARK) screen = RADIO;
    }
    return;
  }
  // Foreground SD access is intentionally infrequent. Never open files during
  // a stream prefill, recent keyboard interaction, or while another UI is used.
  if (!jazzSparkAutoEnabled || !sdAvailable || screen != RADIO ||
      now - lastAutoSparkAt < SPARK_AUTO_INTERVAL_MS ||
      now - lastKeyActivity < 15000u ||
      (audioState.load() == PLAYING && audioBufferedBytes.load() < STREAM_BYTES / 2))
    return;
  lastAutoSparkAt = now;
  showJazzSpark(0);
  if (jazzSparkReady && screen == SPARK) {
    jazzSparkAutoVisible = true;
    autoSparkUntil = millis() + SPARK_AUTO_VISIBLE_MS;
  }
}

String shortName(const char* name, size_t limit) {
  if (!name) return "";
  const size_t nameBytes = strlen(name);
  if (nameBytes <= limit) return String(name);
  if (limit <= 2) return "";
  // String.substring() accepts byte offsets, not UTF-8 code point indices.
  // Avoid leaving a torn multibyte glyph before the ellipsis.
  const size_t prefix = pocketjazz::utf8SafePrefix(name, nameBytes, limit - 2);
  return String(name).substring(0, prefix) + "..";
}
void text(int x, int y, const String& str, uint16_t color, int scale = 1) {
  auto& d = M5Cardputer.Display;
  d.setTextColor(color);
  d.setTextSize(scale);
  d.drawString(str, x, y);
}
constexpr int kRadioTextX = 9;
constexpr int kRadioTextWidth = 222;
// Two pixel-bounded UTF-8-safe text lines with no String, sprite or scrolling
// timer. A glyph is never cut across a multibyte UTF-8 code point.
void drawWrappedRadioText(const char* value, int y, uint16_t color,
                          int maxRows = 2) {
  auto& d = M5Cardputer.Display;
  d.setTextSize(1);
  if (!value || !value[0]) return;
  size_t pos = 0;
  for (int row = 0; row < maxRows && value[pos]; ++row) {
    char line[128]{};
    size_t out = 0;
    size_t lastSpaceBytes = 0;
    while (value[pos + out] && out < sizeof(line) - 5) {
      const uint8_t lead = static_cast<uint8_t>(value[pos + out]);
      size_t cpBytes = (lead & 0x80) == 0 ? 1 :
                       (lead & 0xE0) == 0xC0 ? 2 :
                       (lead & 0xF0) == 0xE0 ? 3 :
                       (lead & 0xF8) == 0xF0 ? 4 : 1;
      bool valid = true;
      for (size_t b = 1; b < cpBytes; ++b)
        if (!value[pos + out + b] ||
            (static_cast<uint8_t>(value[pos + out + b]) & 0xC0) != 0x80)
          valid = false;
      if (!valid) cpBytes = 1;
      if (out + cpBytes >= sizeof(line)) break;
      memcpy(line + out, value + pos + out, cpBytes);
      line[out + cpBytes] = '\0';
      if (d.textWidth(line) > kRadioTextWidth) {
        line[out] = '\0';
        break;
      }
      out += cpBytes;
      if (cpBytes == 1 && line[out - 1] == ' ') lastSpaceBytes = out;
    }
    if (!out) break;
    const bool more = value[pos + out] != '\0';
    size_t consumed = out;
    if (more && lastSpaceBytes > out / 2 && row + 1 < maxRows) {
      out = lastSpaceBytes;
      consumed = out;
      while (out && line[out - 1] == ' ') --out;
      line[out] = '\0';
    } else if (more && row + 1 == maxRows) {
      while (out > 0) {
        line[out] = '\0';
        if (d.textWidth(line) + d.textWidth("..") <= kRadioTextWidth) break;
        do { --out; } while (out && (static_cast<uint8_t>(line[out]) & 0xC0) == 0x80);
      }
      line[out] = '\0';
      strlcat(line, "..", sizeof(line));
    }
    d.setClipRect(kRadioTextX, y + row * 10, kRadioTextWidth, 10);
    d.setTextColor(color);
    d.drawString(line, kRadioTextX, y + row * 10);
    d.clearClipRect();
    pos += consumed;
    while (value[pos] == ' ') ++pos;
  }
}
void splitRadioMetadata(const char* metadata, char* artist, size_t artistSize,
                        char* title, size_t titleSize) {
  if (!artistSize || !titleSize) return;
  artist[0] = title[0] = 0;
  if (!metadata || !*metadata) return;
  const char* delimiter = strstr(metadata, " - ");
  if (!delimiter || delimiter == metadata || !delimiter[3]) {
    pocketjazz::copyUtf8Bounded(title, titleSize, metadata, strlen(metadata));
    return;
  }
  const size_t length = static_cast<size_t>(delimiter - metadata);
  pocketjazz::copyUtf8Bounded(artist, artistSize, metadata, length);
  pocketjazz::copyUtf8Bounded(title, titleSize, delimiter + 3,
                             strlen(delimiter + 3));
}
void drawVu(uint8_t level) {
  auto& d = M5Cardputer.Display;
  d.fillRect(30, 94, 62, 15, C_BG);
  d.drawFastHLine(34, 107, 56, C_MUTED);
  d.drawFastVLine(34, 104, 4, C_MUTED);
  d.drawFastVLine(62, 103, 5, C_MUTED);
  d.drawFastVLine(90, 104, 4, C_MUTED);
  const int needleX = 34 + (level * 56) / 100;
  d.drawLine(62, 107, needleX, 96, C_GOLD);
  d.fillCircle(62, 107, 1, C_GOLD);
}
void drawPlayer(bool partial = false) {
  auto& d = M5Cardputer.Display;
  const int idx = stationIndex();
  int remainingSleep = -1;
  if (sleepMinutes) {
    const uint32_t elapsed = (uint32_t)(millis() - sleepStartedAt) / 60000u;
    remainingSleep = elapsed < sleepMinutes ? sleepMinutes - elapsed : 0;
  }
  if (!partial) {
    d.fillScreen(C_BG);
    text(9, 6, "POCKET JAZZ", C_GOLD);
    d.drawFastHLine(9, 22, 222, C_PANEL2);
    d.drawFastHLine(9, 111, 222, C_PANEL2);
  } else {
    // Clear only changed regions, not the full screen.
    d.fillRect(116, 4, 122, 17, C_BG);
    d.fillRect(8, 27, 225, 83, C_BG);
    d.fillRect(8, 114, 226, 18, C_BG);
  }
  text(161, 6, wifiReady.load() ? "WiFi ON" : (wifiHasIp() ? "WiFi ..." : "WiFi --"),
       wifiReady.load() ? C_GREEN : C_RED);
  if (batteryLevel >= 0) text(123, 6, String(batteryLevel) + "%", C_MUTED);
  // Every text field now uses 222px instead of the old 137px sidebar width.
  const char* station = idx >= 0 ? stations[idx].name : "No stations";
  drawWrappedRadioText(station, 28, C_WHITE, 2);
  char artist[96], song[96];
  splitRadioMetadata(currentTitle, artist, sizeof(artist), song, sizeof(song));
  if (artist[0]) {
    drawWrappedRadioText(artist, 49, C_MUTED, 2);
    drawWrappedRadioText(song, 70, C_GOLD, 2);
  } else if (song[0]) {
    drawWrappedRadioText(song, 52, C_GOLD, 3);
  }
  text(9, 97, "VU", C_MUTED);
  drawVu(shownVu);
  const int visibleVolume = dailyAlarmSounding ? dailyAlarmOutputVolume : volume;
  char volumeLabel[16]{};
  snprintf(volumeLabel, sizeof(volumeLabel), "VOL %u%%",
           (unsigned)((visibleVolume * 100 + 92) / 185));
  text(107, 97, volumeLabel, C_WHITE);
  if (remainingSleep >= 0)
    text(179, 97, "T-" + String(remainingSleep) + "m", C_GOLD);
  const char* label = "STOP";
  switch (audioState.load()) {
    case WAIT_WIFI: label = wifiHasIp() ? "WIFI WAIT" : "WIFI OFFLINE"; break;
    case WAIT_TIME: label = "SYNCING TIME"; break;
    case CONNECTING: label = "CONNECTING"; break;
    case BUFFERING: label = "BUFFERING"; break;
    case PLAYING: label = "ON AIR"; break;
    case ERROR_STREAM: label = "RETRYING"; break;
    case ERROR_MEMORY: label = "LOW MEMORY"; break;
    default: label = "STOP"; break;
  }
  text(9, 116, dailyAlarmSounding ? "ALARM" : label,
       dailyAlarmSounding ? C_GOLD : audioState.load() == PLAYING ? C_GREEN : C_GOLD);
  if (uiNoticeUntil != 0 && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(94, 116, uiNotice, C_GOLD);
  else text(94, 116, dailyAlarmSounding ? "ENTER stops alarm" :
            "H - Help", dailyAlarmSounding ? C_GOLD : C_MUTED);
}
void drawNight(bool partial = false) {
  auto& d = M5Cardputer.Display;
  if (!partial) {
    d.fillScreen(0x0000);
    text(10, 7, "BLUE NOTE CLOCK", C_GOLD);
    text(173, 7, audioState.load() == PLAYING ? "ON AIR" : "PAUSED", C_MUTED);
    text(9, 121, "UP/DN TZ 30m   0 RESET   DEL BACK", C_WHITE);
  } else {
    // Clock face and its metadata only: never paint over the static help
    // line, and never leave old titles or offsets when their length shrinks.
    d.fillRect(9, 26, 225, 90, 0x0000);
    d.fillRect(170, 5, 68, 13, 0x0000);
    text(173, 7, audioState.load() == PLAYING ? "ON AIR" : "PAUSED", C_MUTED);
  }
  char clock[20] = "--:--";
  char zone[12] = "UNSET";
  char date[24] = "";
  if (ntpSynced.load() && time(nullptr) >= MIN_TLS_TIME) {
    time_t t = time(nullptr);
    tm clockTm{};
    localtime_r(&t, &clockTm);
    strftime(clock, sizeof(clock), "%H:%M", &clockTm);
    strftime(zone, sizeof(zone), "%Z", &clockTm);
    strftime(date, sizeof(date), "%a %d %b", &clockTm);
  }
  // Time is the only large element. NTP status lives in Diagnostics:
  // showing both libc %Z and saved UTC offset was redundant/confusing.
  text(75, 26, String(clock), C_WHITE, 3);
  text(75, 60, date[0] ? date : "TIME NOT SET", C_MUTED);
  const int idx = stationIndex();
  text(11, 77, idx >= 0 ? shortName(stations[idx].name, 34) : "No stations", C_GOLD);
  if (currentTitle[0]) text(11, 90, shortName(currentTitle, 36), C_WHITE);
  // One timezone label only, reflecting the *active* manual offset or the
  // SD POSIX TZ abbreviation. Controls are always visible at the bottom.
  text(11, 105, clockUserOverride ? String("TZ ") + clockOffsetText() :
       (clockConfigLoaded ? String("TZ ") + shortName(zone, 10) + " (AUTO)" :
                            String("TZ UTC+0")), C_MUTED);
}
void drawSoundFx() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(10, 6, "TUNING SOUND FX", C_GOLD, 2);
  text(11, 36, String("1 VINYL CRACKLE: ") +
       (vinylCrackleEnabled.load() ? "ON" : "OFF"), C_WHITE);
  text(11, 54, String("2 SOFT WHITE NOISE: ") +
       (tuningHissEnabled.load() ? "ON" : "OFF"), C_WHITE);
  text(11, 77, "AFTER CONNECT, BEFORE PCM", C_MUTED);
  text(11, 92, "NEVER ON RETRIES OR MUTE", C_MUTED);
  text(11, 120, futureNvsSchema ? "NVS READ-ONLY  DEL BACK" :
       "1/2 TOGGLE  DEL BACK", C_GOLD);
}
void drawMood() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 6, "MOOD TUNER", C_GOLD, 2);
  const int left = 22, top = 34;
  for (int acoustic = 0; acoustic <= 4; ++acoustic) {
    for (int energy = 0; energy <= 4; ++energy)
      d.fillCircle(left + energy * 25, top + (4 - acoustic) * 17, 1, C_MUTED);
  }
  for (int i = 0; i < stationCount; ++i) {
    if (stations[i].moodEnergy < 0 || stations[i].moodAcoustic < 0) continue;
    const int x = left + stations[i].moodEnergy * 25;
    const int y = top + (4 - stations[i].moodAcoustic) * 17;
    d.drawCircle(x, y, 4, C_GREEN);
  }
  const int cursorX = left + moodEnergyCursor * 25;
  const int cursorY = top + (4 - moodAcousticCursor) * 17;
  d.drawRect(cursorX - 6, cursorY - 6, 13, 13, C_GOLD);
  text(144, 32, "Energy " + String(moodEnergyCursor) + "/4", C_WHITE);
  text(144, 48, "Acoustic " + String(moodAcousticCursor) + "/4", C_WHITE);
  const int match = closestMoodStation();
  text(144, 70, match >= 0 ? shortName(stations[match].name, 15) : "No tagged", C_GOLD);
  text(144, 84, match >= 0 ? "ENTER tune" : "Edit M3U", C_MUTED);
  text(8, 118, "LEFT/RIGHT energy  UP/DOWN acoustic", C_MUTED);
}
void drawNotes() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "JAZZ NOTES", C_GOLD, 2);
  if (!currentTitle[0] && !notesFound) {
    text(9, 37, "No ICY track metadata", C_WHITE);
    text(9, 53, "This station may not send it", C_MUTED);
  } else if (!notesFound) {
    text(9, 37, notesArtist.length() ? shortName(notesArtist.c_str(), 32)
                                   : String("Artist not identified"), C_WHITE);
    text(9, 58, "No offline entry for artist", C_MUTED);
    text(9, 77, "SD: /PocketJazz/", C_MUTED);
    text(9, 90, "jazz-encyclopedia.tsv", C_MUTED);
  } else {
    text(9, 28, shortName(notesArtist.c_str(), 38), C_WHITE);
    // Four readable biography lines per page. Full details remain accessible
    // via W/S; the former "Source:" line never consumes LCD pixels.
    String rest = notesDetail;
    for (int line = 0; line < 8 && rest.length(); ++line) {
      int split = rest.length() > 36 ? rest.lastIndexOf(' ', 36) : -1;
      if (rest.length() > 36 && split < 12) split = 36;
      String part = split < 0 ? rest : rest.substring(0, split);
      if (line >= notesDetailPage * 4 && line < notesDetailPage * 4 + 4)
        text(9, 44 + (line - notesDetailPage * 4) * 13,
             shortName(part.c_str(), 38), C_WHITE);
      rest = split < 0 ? "" : rest.substring(split);
      rest.trim();
    }
    if (notesAlbum.length() && notesAlbum != "-")
      text(9, 102, "Album: " + shortName(notesAlbum.c_str(), 31), C_GOLD);
    // Sources are retained in TSV/provenance files, NEVER shown in this view.
  }
  text(8, 121, notesFound ? "UP/DOWN page   DEL back" :
       "DEL back", C_MUTED);
}
void drawNotesSearch() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "JAZZ NOTES / SEARCH", C_GOLD);
  text(8, 27, "> " + notesSearchQuery + "_", C_WHITE);
  if (!sdAvailable) text(9, 56, "SD unavailable", C_RED);
  else if (!notesSearchRan) text(9, 56, "Enter: search SD encyclopedia", C_MUTED);
  else if (!notesSearchTotal) text(9, 56, "No matching offline entries", C_MUTED);
  else {
    for (size_t i = 0; i < notesSearchCount; ++i) {
      const int y = 47 + int(i) * 21;
      const bool chosen = notesSearchOffset + i == static_cast<size_t>(notesSearchSelected);
      if (chosen) d.fillRoundRect(6, y - 2, 228, 19, 3, C_PANEL2);
      String name;
      String album;
      if (notesSearchEncyclopedia) {
        name = notesSearchPage.encyclopedia(i).name;
        album = notesSearchPage.encyclopedia(i).album;
      } else {
        name = notesSearchPage.legacy(i).key;
        name.replace('-', ' ');
        album = notesSearchPage.legacy(i).album;
      }
      text(9, y, shortName(name.c_str(), 21), chosen ? C_GOLD : C_WHITE);
      if (album != "-") text(133, y, shortName(album.c_str(), 16), C_MUTED);
    }
    text(8, 112, String(notesSearchSelected + 1) + " / " +
         String(notesSearchTotal) + "  UP/DOWN list", C_MUTED);
  }
  text(8, 125, notesSearchRan ? "UP/DOWN pick Enter open Del edit" :
       "Type + Enter search   DEL back", C_MUTED);
}
void drawSpark() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "JAZZ SPARK", C_GOLD, 2);
  if (!jazzSparkReady) {
    text(9, 50, "Missing /PocketJazz/jazz-facts.tsv", C_MUTED);
    return;
  }
  const bool quote = !strcmp(jazzSpark.type, "QUOTE");
  text(9, 31, shortName(jazzSpark.subject, 37), C_GOLD);
  String rest = quote ? String("\"") + jazzSpark.text + "\"" : String(jazzSpark.text);
  for (int line = 0; line < 6 && rest.length(); ++line) {
    int split = rest.length() > 37 ? rest.lastIndexOf(' ', 37) : -1;
    if (rest.length() > 37 && split < 12) split = 37;
    const String part = split < 0 ? rest : rest.substring(0, split);
    text(9, 46 + line * 12, part, C_WHITE);
    rest = split < 0 ? "" : rest.substring(split);
    rest.trim();
  }
  text(8, 112, jazzSparkAutoEnabled ? "T auto: ON" : "T auto: OFF", C_MUTED);
  text(8, 125, "UP/DOWN facts    DEL back", C_MUTED);
}
void drawTracks() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "ICY / RECENT TITLES", C_GOLD, 2);
  if (!trackHistory.size()) {
    text(9, 42, "No titles received yet", C_WHITE);
    text(9, 61, "Station must send ICY data", C_MUTED);
  } else {
    for (size_t i = 0; i < trackHistory.size(); ++i) {
      const auto* entry = trackHistory.newest(i);
      const int y = 29 + (int)i * 17;
      text(8, y, shortName(entry->station, 9), C_MUTED);
      text(73, y, shortName(entry->title, 26), i == 0 ? C_GOLD : C_WHITE);
    }
  }
  if (uiNoticeUntil && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(8, 119, uiNotice, C_GOLD);
  else text(8, 119, "E export ICY log   DEL back", C_MUTED);
}
void drawBrowse() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 6, "BROWSE STATIONS", C_GOLD, 2);
  if (!listCount) {
    text(9, 49, "No stations", C_WHITE);
  } else {
    text(9, 27, shortName(lists[browseList].name, 25), C_MUTED);
    const int first = browseFirstVisible();
    const int count = lists[browseList].count;
    for (int row = 0; row < 4 && first + row < count; ++row) {
      const int within = first + row;
      const int id = lists[browseList].first + within;
      const int y = 43 + row * 18;
      if (within == browseSelected) d.fillRoundRect(6, y - 2, 228, 17, 2, C_PANEL2);
      const bool healthWarning = stationHealth.warning(id);
      text(10, y, (healthWarning ? "! " : "") +
           shortName(stations[id].name, healthWarning ? 14 : 16),
           healthWarning ? C_RED : within == browseSelected ? C_GOLD : C_WHITE);
      const auto* result = stationPeek.lookup(id);
      if (stationPeek.fresh(result, millis()))
        text(117, y, shortName(result->title, 18), C_GREEN);
      else if (result && result->checked)
        text(117, y, result->hasTitle ? "stale" : "no ICY title", C_MUTED);
      else text(117, y, id == stationIndex() && currentTitle[0] ?
                shortName(currentTitle, 18) : String("unknown"), C_MUTED);
    }
  }
  text(8, 118, !peekOptIn ? "ICY peek OFF (SD opt-in)" :
       stationPeek.busy() ? "ICY peek: one low-priority probe" :
       "ICY peek: bounded, on browse only", C_MUTED);
  text(8, 127, "UP/DOWN P NEXT PLAYLIST ENTER PLAY DEL", C_WHITE);
}
void drawShare() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  if (shareQrPage == 0) {
    text(8, 7, "STATION QR   1/3", C_GOLD);
    const int idx = stationIndex();
    if (idx < 0 || !pocketjazz::drawStationQr(d, stations[idx].url, 10, 26)) {
      text(10, 46, "No safe, short public URL", C_WHITE);
      text(10, 63, "QR unavailable", C_MUTED);
    } else {
      text(108, 34, shortName(stations[idx].name, 20), C_GOLD);
      text(108, 53, "Scan with phone", C_WHITE);
      text(108, 72, "Public stream URL", C_MUTED);
      text(108, 88, "No Wi-Fi passwords", C_MUTED);
    }
  } else {
    const bool spotify = shareQrPage == 1;
    text(8, 7, spotify ? "SPOTIFY LIKE QR   2/3" : "WEB LIKE QR   3/3", C_GOLD);
    bool shortened = false;
    const bool drew = pocketjazz::drawLikedTrackSearchQr(
        d, lastLikedTitle, spotify ? pocketjazz::TrackSearchProvider::Spotify
                                   : pocketjazz::TrackSearchProvider::Web,
        8, 24, &shortened);
    if (!drew) {
      text(8, 48, "NO SEARCHABLE LIKE YET", C_WHITE);
      text(8, 69, "Press K during an ICY track", C_MUTED);
    } else {
      text(113, 32, "LAST LIKE:", C_WHITE);
      text(113, 51, shortName(lastLikedTitle, 18), C_GOLD);
      text(113, 80, shortened ? "QUERY SHORTENED" : "SCAN TO SEARCH", C_WHITE);
      text(113, 96, "NO API LOGIN", C_MUTED);
    }
  }
  text(8, 125, "UP/DOWN QR TYPE    DEL BACK", C_WHITE);
}
void drawTempo() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "TAP TEMPO / JAZZ", C_GOLD, 2);
  const int estimated = manualTempo.bpm(millis());
  if (estimated > 0) {
    text(62, 36, String(estimated), C_WHITE, 3);
    text(165, 54, "BPM", C_GOLD);
  } else {
    text(38, 45, "BPM UNKNOWN", C_MUTED, 2);
  }
  text(12, 89, "Tap J at least 4 times", C_WHITE);
  text(12, 105, "Median of up to 5 intervals", C_MUTED);
  text(8, 121, "J TAP   C CLEAR   DEL BACK", C_GOLD);
}
void serviceVu(uint32_t now) {
  if (screen != RADIO || now - lastVuAt < 90) return;
  lastVuAt = now;
  const int target = pcmPeak.load();
  if (shownVu < target) shownVu = (uint8_t)target;
  else shownVu = (uint8_t)max(0, (int)shownVu - 6);
  if (!uiPainted || (previousVu >= 0 && abs((int)shownVu - previousVu) < 2)) return;
  drawVu(shownVu);
  previousVu = shownVu;
}
void drawWifiList() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(9, 7, "Wi-Fi networks", C_GOLD, 2);
  if (wifiScanPending) text(10, 39, "Scanning...", C_MUTED);
  else if (networkCount == 0) text(10, 39, "No networks. DEL = back", C_MUTED);
  else {
    const int start = max(0, min(selectedNetwork - 2, networkCount - 4));
    for (int i = start; i < min(start + 4, networkCount); ++i) {
      const int y = 38 + (i - start) * 19;
      if (i == selectedNetwork) d.fillRoundRect(7, y - 2, 226, 18, 3, C_PANEL2);
      text(12, y, shortName(networkNames[i].c_str(), 32), i == selectedNetwork ? C_GOLD : C_WHITE);
    }
  }
  text(9, 117, "UP/DOWN select   ENTER join", C_MUTED);
}
void drawWifiPassword() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(9, 7, "Wi-Fi password", C_GOLD, 2);
  text(9, 40, shortName(wifiSsid.c_str(), 32), C_MUTED);
  d.fillRoundRect(8, 58, 222, 24, 4, C_PANEL);
  String visible = wifiShowPassword ? wifiPassword : String();
  if (!wifiShowPassword) for (size_t i=0; i<wifiPassword.length(); ++i) visible += '*';
  text(15, 65, shortName(visible.c_str(), 30), C_WHITE);
  if (wifiJoinFailed) text(9, 87, "Connection timeout. Retry.", C_RED);
  text(9, 100, "TAB show/hide  DEL erase", C_MUTED);
  text(9, 117, "ENTER connect", C_GOLD);
}
void drawWifiJoin() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(9, 8, "Connecting...", C_GOLD, 2);
  text(10, 45, shortName(wifiSsid.c_str(), 32), C_WHITE);
  text(10, 85, "No password is shown/logged", C_MUTED);
}
void drawHelp() {
  // Large 2x font: four distinct actions per 240x135 page, never cram
  // multiple unrelated controls into a tiny line.
  struct HelpPage {
    const char* section;
    const char* rows[4];
  };
  static constexpr HelpPage pages[] = {
    {"RADIO", {
      "LEFT/RIGHT TUNE",
      "P NEXT PLAYLIST",
      "ENTER PLAY/STOP",
      "- / = VOL ANYWHERE"
    }},
    {"PLAYBACK", {
      "M MUTE / UNMUTE",
      "O AUTOPLAY",
      "T SLEEP 30/45/60",
      "VU ALWAYS ON"
    }},
    {"PRESETS & VIEWS", {
      "1-9 RECALL PRESET",
      "FN+1-9 SAVE PRESET",
      "C CLOCK",
      "S TUNING SOUND FX"
    }},
    {"FIND & READ", {
      "F BROWSE STATIONS",
      "TAB STATION SEARCH",
      "U NOTES SEARCH",
      "L RECENT STATIONS"
    }},
    {"JAZZ & FAVORITES", {
      "Z JAZZ SPARK",
      "G MOOD TUNER",
      "K LIKE TO SD",
      "Y ICY HISTORY"
    }},
    {"OTHER TOOLS", {
      "Q SHARE QR",
      "J TAP TEMPO",
      "0 OFFLINE MP3",
      "FN+T DAILY ALARM"
    }},
    {"SYSTEM", {
      "W WI-FI SETUP",
      "R RELOAD SD",
      "I DIAGNOSTICS",
      "B SETTINGS BACKUP"
    }},
    {"BROWSE STATIONS", {
      "UP PREVIOUS",
      "DOWN NEXT",
      "P NEXT PLAYLIST",
      "ENTER TUNE"
    }},
    {"STATION SEARCH", {
      "TYPE STATION NAME",
      "UP/DOWN SELECT",
      "ENTER PLAY",
      "DEL ERASE/BACK"
    }},
    {"NOTES SEARCH", {
      "TYPE ARTIST NAME",
      "ENTER SEARCH",
      "UP/DOWN RESULTS",
      "ENTER OPEN NOTE"
    }},
    {"JAZZ NOTES", {
      "UP/DOWN NOTE PAGE",
      "DEL EDIT/BACK",
      "SD OFFLINE LIBRARY",
      "N CURRENT ARTIST"
    }},
    {"JAZZ SPARK", {
      "UP PREVIOUS FACT",
      "DOWN NEXT FACT",
      "T AUTO CARDS",
      "DEL BACK"
    }},
    {"CLOCK OFFSET", {
      "UP ADD 30 MIN",
      "DOWN LESS 30 MIN",
      "0 RESET TO SD TZ",
      "NVS SAVES OFFSET"
    }},
    {"WI-FI SETTINGS", {
      "UP/DOWN NETWORK",
      "ENTER SELECT",
      "TYPE PASSWORD",
      "TAB SHOW/HIDE"
    }},
    {"DIAGNOSTICS", {
      "UP/DOWN PAGES",
      "E SAVE STATUS",
      "X RESET COUNTERS",
      "DEL BACK TO RADIO"
    }},
    {"TEMPO & MOOD", {
      "J TAP BEAT",
      "C RESET BPM",
      "LEFT/RIGHT ENERGY",
      "UP/DOWN ACOUSTIC"
    }},
    {"SD AND SAFETY", {
      "HOLD S SAFE BOOT",
      "K SAVES LIKED.TXT",
      "E SAVES TSV LOGS",
      "ICY PEEK SD OPT-IN"
    }},
    {"CATALOG UPDATE", {
      "D OPEN FROM RADIO",
      "ENTER DOWNLOAD",
      "DEL BACK",
      "PRIVATE M3U SAFE"
    }}
  };
  static_assert(sizeof(pages) / sizeof(pages[0]) == HELP_PAGE_COUNT,
                "Help page count must match keyboard navigation");
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  const HelpPage& page = pages[helpPage % HELP_PAGE_COUNT];
  text(8, 3, "HELP " + String(helpPage + 1) + "/" +
       String(HELP_PAGE_COUNT) + "  " + page.section, C_WHITE);
  // Version occupies the right end of the fixed header; no extra Help page
  // and no dynamic repaint dependency on network/audio events.
  text(192, 13, POCKETJAZZ_FIRMWARE_VERSION, C_WHITE);
  d.drawFastHLine(8, 24, 224, C_PANEL2);
  for (int line = 0; line < 4; ++line)
    text(8, 29 + line * 20, page.rows[line], C_WHITE, 2);
  d.drawFastHLine(8, 110, 224, C_PANEL2);
  text(8, 121, "UP/DOWN PAGES   DEL BACK", C_WHITE);
}
void drawRecents() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "RECENT STATIONS", C_GOLD, 2);
  if (!recentCount) text(8, 42, "No stations played yet", C_MUTED);
  for (int i = 0; i < recentCount; ++i) {
    const int y = 32 + i * 17;
    if (i == recentSelected) d.fillRoundRect(6, y - 2, 228, 16, 3, C_PANEL2);
    text(11, y, String(i + 1) + " " + shortName(recent[i].name, 31), i == recentSelected ? C_GOLD : C_WHITE);
  }
  text(8, 120, "UP/DOWN select Enter play Del back", C_MUTED);
}
void drawSearch() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 7, "STATION SEARCH", C_GOLD, 2);
  text(9, 28, "> " + stationSearch + "_", C_WHITE);
  const int count = searchMatchCount();
  const int first = max(0, searchSelected - 2);
  for (int row = 0; row < 4; ++row) {
    const int slot = first + row;
    if (slot >= count) break;
    const int idx = searchMatch(slot);
    if (idx < 0) break;
    const int y = 49 + row * 16;
    if (slot == searchSelected) d.fillRoundRect(6, y - 2, 228, 15, 3, C_PANEL2);
    text(10, y, shortName(stations[idx].name, 33),
         slot == searchSelected ? C_GOLD : C_MUTED);
  }
  if (!count) text(9, 55, "No matches", C_RED);
  text(8, 119, "UP/DOWN pick Enter play DEL back", C_MUTED);
}
// All eight pages must use exactly the same font and size for ALL data.
// v0.7z4 still mixed 1x/2x within each page. Fixed 1x is the only size that
// shows the full error codes and free/largest heap pairs on a 240x135 LCD.
// Measurements keep their complete precision in the manually exported TSV.
static constexpr int kDiagFontScale = 1;
void diagRow(int y, const String& line) {
  text(8, y, line, C_WHITE, kDiagFontScale);
}
const char* diagFailName(int failure) {
  switch (failure) {
    case 0: return "NONE";
    case 1: return "NETWORK OPEN";
    case 2: return "BUFFER SETUP";
    case 3: return "MP3 DECODER";
    case 4: return "MP3 PLAYBACK";
    case 5: return "NO ROOM FOR RING";
    case 6: return "NO ROOM FOR CODEC";
    case 7: return "NO MP3 DATA";
    default: return "UNKNOWN";
  }
}
const char* diagRebootName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWER ON";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "CRASH";
    case ESP_RST_INT_WDT: return "INTERRUPT WDT";
    case ESP_RST_TASK_WDT: return "TASK WDT";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "LOW VOLTAGE";
    case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
    default: return "OTHER / UNKNOWN";
  }
}
uint32_t diagNextAttemptSeconds(uint32_t now, uint32_t retryAt) {
  return pocketjazz::retrySecondsRemaining(now, retryAt);
}
String diagLatencyUpper(uint32_t boundMs) {
  // UINT32_MAX is the open-ended >5120ms bucket, or a snapshot during
  // concurrent reset. It is never a literal 49-day latency measurement.
  return boundMs == UINT32_MAX ? String(">5120ms/?")
                               : String(boundMs) + "ms";
}
const char* diagAudioStateName(int state) {
  switch (state) {
    case STOPPED: return "STOP";
    case WAIT_WIFI: return "WIFI WAIT";
    case WAIT_TIME: return "TIME WAIT";
    case CONNECTING: return "CONNECTING";
    case BUFFERING: return "BUFFERING";
    case PLAYING: return "ON AIR";
    case ERROR_STREAM: return "RETRYING";
    case ERROR_MEMORY: return "LOW MEMORY";
    default: return "UNKNOWN";
  }
}
const char* diagHttpsPhaseName(uint32_t phase) {
  switch (phase) {
    case HTTPS_NONE: return "NONE";
    case HTTPS_INVALID: return "INVALID";
    case HTTPS_BEGIN: return "BEGIN";
    case HTTPS_DNS: return "DNS";
    case HTTPS_TCP_TLS: return "TCP+TLS";
    case HTTPS_POST_CONNECT: return "POST CONNECT";
    case HTTPS_HTTP: return "HTTP";
    case HTTPS_READY: return "READY";
    default: return "UNKNOWN";
  }
}
void drawDiagnostics(bool partial = false) {
  auto& d = M5Cardputer.Display;
  d.setTextFont(1);
  d.setTextSize(kDiagFontScale);
  // A diagnostics refresh is always a complete 240x135 repaint. The old
  // "partial" refresh erased y=0..113 and y=116..134 but left the 114..115
  // strip untouched. Text from the dense page 4/8 row at y=108 survives in
  // those two pixels even after navigating to sparse page 1/8.
  // Do not use a 240x135 sprite here: the ADV has no PSRAM and a 16-bit
  // framebuffer would allocate another 64,800 bytes during TLS.
  (void)partial; // compatibility with existing drawDiagnostics(true) caller
  d.fillScreen(C_BG);
  static constexpr const char* titles[DIAG_PAGE_COUNT] = {
    "WI-FI STATUS", "WI-FI DISCONNECTS", "HTTPS CONNECTION",
    "RADIO STREAM", "AUDIO PROCESSING", "SOUND TIMING",
    "FREE MEMORY", "SYSTEM STATUS"
  };
  text(8, 5, String(titles[diagPage]) + " " +
       String(diagPage + 1) + "/" + String(DIAG_PAGE_COUNT), C_GOLD);
  text(188, 5, POCKETJAZZ_FIRMWARE_VERSION, C_WHITE);
  d.drawFastHLine(8, 18, 224, C_PANEL2);
  if (diagPage == 0) {
    diagRow(24, String("Wi-Fi connection: ") +
            (wifiReady.load() ? "READY" : wifiHasIp() ? "IP WAIT" : "OFFLINE"));
    diagRow(38, "IP address: " + (wifiHasIp() ? WiFi.localIP().toString() : String("NOT ASSIGNED")));
    diagRow(52, "Signal strength: " + String(wifiHasIp() ? WiFi.RSSI() : -127) + " dBm");
    diagRow(66, "Internet clock: " + String(ntpSynced.load() ? "SYNCED" :
            (time(nullptr) >= MIN_TLS_TIME ? "TIME SET" : "NOT SET")));
  } else if (diagPage == 1) {
    diagRow(24, "IP assigned (times): " + String(wifiGotIpEvents.load()));
    diagRow(38, "Wi-Fi disconnections: " + String(wifiDisconnectEvents.load()));
    diagRow(52, "Last reason code: " +
            String(wifiDisconnectEvents.load() ? String(wifiDisconnectReason.load()) : String("NONE")));
    diagRow(66, "Last disconnect at: " +
            String(wifiDisconnectEvents.load() ? String(wifiDisconnectAt.load() / 1000u) + "s" : "NEVER"));
  } else if (diagPage == 2) {
    // These atomics describe the LAST completed HTTPS attempt, not a
    // live progress update while a synchronous DNS/TLS/GET call is blocked.
    diagRow(24, "Last HTTPS step: " + String(audioHttpsPhase.load()) +
            " " + diagHttpsPhaseName(audioHttpsPhase.load()));
    diagRow(36, "Redirects followed: " + String(audioHttpsHop.load()));
    const int dns = audioHttpsDnsOk.load();
    const int tls = audioHttpsSecureOk.load();
    diagRow(48, "DNS lookup: " + String(dns == 1 ? "OK" : dns == 0 ? "FAILED" : "N/A") +
            " " + String(audioHttpsDnsMs.load()) + "ms");
    diagRow(60, "Secure connect: " + String(tls == 1 ? "OK" : tls == 0 ? "FAILED" : "N/A") +
            " " + String(audioHttpsSecureMs.load()) + "ms");
    diagRow(72, "HTTP request: " + String(audioHttpsRequestMs.load()) + "ms");
    // Positive TLS raw can be an SDK socket descriptor, not an error.
    const int32_t tlsRaw = audioTlsErrorCode.load();
    diagRow(84, "TLS result code: " + String(tlsRaw) +
            (tlsRaw < 0 ? " (ERROR)" : tlsRaw > 0 ? " (SOCKET)" : " (NO ERROR)"));
  } else if (diagPage == 3) {
    const int active = stationIndex();
    diagRow(24, "Radio status: " + String(diagAudioStateName(audioState.load())));
    diagRow(36, "Open attempts: " + String(diagAudioWindow.opens(audioReopens.load())));
    diagRow(48, "Failure: " + String(audioFailureStage.load()) + " " +
            diagFailName(audioFailureStage.load()));
    diagRow(60, "HTTP response code: " + String(audioSourceStatusCode.load()));
    // Dynamic 16/32-KiB ring capacity was previously visible only on SIGNAL.
    // Show fill / capacity / low-water mark on the existing stream page.
    diagRow(72, "MP3 " + String(audioBufferedBytes.load()) + "/" +
            String(audioRingCapacity.load()) + " min:" +
            String(audioBufferMinimum.load()) + " B");
    diagRow(84, "Audio underruns: " + String(diagAudioWindow.under(audioUnderruns.load())));
    diagRow(96, "Failed in a row: " + String(active >= 0 ? stationHealth.streak(active) : 0));
    diagRow(108, "Open:" + String(audioLastOpenDurationMs.load()) +
            "ms; min at:" + String(audioBufferMinAtMs.load() / 1000u) + "s");
  } else if (diagPage == 4) {
    // Instrumented by the existing audio owner. Maxima are in microseconds.
    diagRow(24, "Network read max: " + String(audioSourcePullMaxUs.load()) + "us");
    diagRow(38, "MP3 decode max: " + String(audioDecodeLoopMaxUs.load()) + "us");
    diagRow(52, "Speaker write max: " + String(audioPcmFlushMaxUs.load()) + "us");
    diagRow(66, "Decoded audio frames: " + String(diagAudioWindow.frames(audioFrames.load())));
    diagRow(80, "Sample rate: " + String(decodedPcmRateHz.load()) + " Hz");
    const uint32_t frameBps = decodedMp3FrameBps.load(std::memory_order_relaxed);
    diagRow(94, "MP3 bitrate: " + (frameBps ?
            String((frameBps + 500u) / 1000u) + "k" +
            (decodedMp3ObservedVbr.load(std::memory_order_relaxed) ? " VBR" : "") :
            String("UNKNOWN")));
  } else if (diagPage == 5) {
    const auto pcm = audioPlayout.snapshot();
    // PCM gap = digital enqueue gap, not proof of audible silence.
    diagRow(24, "Speaker queue gaps: " + String(pcm.pcmGapEpisodes));
    diagRow(36, "Longest queue gap: " + String(pcm.pcmGapMaxMs) + "ms");
    diagRow(48, "Last stop delay: " + String(pcm.stopLastMs) + "ms");
    diagRow(60, "Stop P95 upper: " + diagLatencyUpper(pcm.stopP95UpperMs));
    diagRow(72, "Next sound delay: " + String(pcm.switchLastMs) + "ms");
    diagRow(84, "Sound P95 upper: " + diagLatencyUpper(pcm.switchP95UpperMs));
    diagRow(96, "Speaker queue rejects: " + String(pcm.pcmHandoffFailures));
  } else if (diagPage == 6) {
    diagRow(24, "Free heap: " + String(ESP.getFreeHeap()) + " B");
    diagRow(36, "Largest free block: " +
            String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + " B");
    diagRow(48, "Lowest free heap: " +
            String(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)) + " B");
    diagRow(60, "Audio stack reserve: " + String(audioStackWatermark.load()) + " B");
    diagRow(72, "TLS before free/max: " + String(audioHttpsFreeBefore.load()) +
            "/" + String(audioHttpsLargestBefore.load()));
    diagRow(84, "TLS after free/max: " + String(audioHttpsFreeAfter.load()) +
            "/" + String(audioHttpsLargestAfter.load()));
    diagRow(96, "TLS pair = free / max block");
  } else if (diagPage == 7) {
    const uint32_t now = millis();
    diagRow(24, "Powered on for: " + String(now / 1000UL) + "s");
    diagRow(36, "Audio update age: " + String(now - audioLive.heartbeat) + "ms");
    diagRow(48, "Last restart: " + String(diagRebootName(esp_reset_reason())));
    diagRow(60, batteryMillivolts > 0
            ? "Battery voltage: " + String(batteryMillivolts) + "mV"
            : String("Battery voltage: UNKNOWN"));
    diagRow(72, "Screen max pause: " + String(uiLoopMaxGapMs.load()) + "ms");
    diagRow(84, "Connect attempt: " + String(audioLive.attempt));
    diagRow(96, "HTTPS redirects: " + String(audioLive.hop));
    if (audioState.load() == ERROR_STREAM &&
        (audioLive.stage == pocketjazz::AudioStage::RETRY ||
         diagNextAttemptSeconds(now, audioLive.retryAt) > 0)) {
      diagRow(108, "NEXT ATTEMPT IN: " +
              String(diagNextAttemptSeconds(now, audioLive.retryAt)) + "s");
    } else {
      diagRow(108, "Audio task: " + String(pocketjazz::audioStageName(audioLive.stage)));
    }
  }
  if (uiNoticeUntil && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(8, 120, uiNotice, C_GOLD);
  else text(8, 120, diagPage == 7 ? "E events X counters UP/DOWN" :
       diagPage == 6 ? "M RAM(STOP) E status X reset" :
       "E save X reset  UP/DOWN  DEL", C_WHITE);
}
void drawSettingsBackup() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 6, "SETTINGS BACKUP", C_GOLD, 2);
  d.drawFastHLine(8, 27, 224, C_PANEL2);
  text(10, 38, "E  Export to microSD", C_WHITE);
  text(10, 56, "I  Import from microSD", C_WHITE);
  text(10, 75, "Requires STOP, confirm twice", C_MUTED);
  text(10, 93, "WiFi passwords excluded", C_MUTED);
  if (backupConfirmation == 1) text(10, 111, "E AGAIN TO CONFIRM", C_GOLD);
  else if (backupConfirmation == 2) text(10, 111, "I AGAIN TO CONFIRM", C_GOLD);
  else text(10, 111, "DEL back (no writes)", C_MUTED);
  if (uiNoticeUntil && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(8, 123, uiNotice, C_GOLD);
}
void drawCatalogUpdate() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 6, "STATION CATALOG", C_GOLD, 2);
  d.drawFastHLine(8, 27, 224, C_PANEL2);
  text(9, 34, "Verified Pocket Jazz GitHub", C_WHITE);
  text(9, 49, "5 standard MP3 playlists", C_MUTED);
  text(9, 64, "Personal stations untouched", C_GREEN);
  if (catalogUpdateInProgress) {
    text(9, 85, "Downloading and checking...", C_GOLD);
    text(9, 101, "Do not remove microSD", C_MUTED);
  } else if (catalogUpdateAttempted) {
    text(9, 84, shortName(pocketjazz::catalogUpdateResultText(catalogUpdateStatus), 35),
         catalogUpdateStatus == pocketjazz::CatalogUpdateResult::Ok ? C_GREEN : C_GOLD);
    text(9, 104, "Only after radio STOP", C_MUTED);
  } else {
    text(9, 86, "Only after radio STOP", C_GOLD);
    text(9, 104, "Checks HTTPS / SHA256 / limits", C_MUTED);
  }
  text(9, 122, "ENTER update  DEL back", C_WHITE);
}
void drawOfflineLibrary() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(8, 5, "MY OFFLINE MP3", C_GOLD, 2);
  const int count = static_cast<int>(offlineCatalog.count());
  const int first = max(0, min(offlineSelected - 1, max(0, count - 4)));
  for (int row = 0; row < 4 && first + row < count; ++row) {
    const int index = first + row;
    const auto* track = offlineCatalog.at((size_t)index);
    if (!track) continue;
    const int y = 29 + row * 19;
    if (index == offlineSelected) d.fillRoundRect(6, y - 2, 228, 18, 2, C_PANEL2);
    text(10, y, shortName(track->filename, 24),
         index == offlineSelected ? C_GOLD : C_WHITE);
    text(182, y, String(track->sizeBytes / 1024u) + "k", C_MUTED);
  }
  if (offlineLeaving) text(9, 105, "Stopping SD decoder...", C_GOLD);
  else if (offlineMode && wantPlay)
    text(9, 105, "PLAY " + shortName(offlinePath + sizeof("/PocketJazz/offline/") - 1, 26), C_GREEN);
  else text(9, 105, "STOP  " + String(count) + " MP3 tracks", C_MUTED);
  text(8, 118, "UP/DOWN  ENTER Play/Stop", C_WHITE);
  text(8, 128, "-/+ volume  DEL stop+radio", C_MUTED);
  if (uiNoticeUntil && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(8, 96, uiNotice, C_GOLD);
}
void drawDailyAlarm() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(C_BG);
  text(9, 6, "DAILY RADIO ALARM", C_GOLD, 2);
  char clock[10]{};
  snprintf(clock, sizeof(clock), "%02u:%02u",
           (unsigned)dailyAlarmEdit.hour, (unsigned)dailyAlarmEdit.minute);
  text(9, 34, clock, C_WHITE, 3);
  text(154, 40, dailyAlarmEdit.enabled ? "ENABLED" : "DISABLED",
       dailyAlarmEdit.enabled ? C_GREEN : C_MUTED);
  const uint8_t slot = dailyAlarmEdit.preset;
  text(9, 70, "Preset " + String(slot + 1) + ": " +
       shortName(presets[slot].name[0] ? presets[slot].name : "(empty)", 20), C_WHITE);
  text(9, 87, "UP/DOWN hour LEFT/RIGHT min", C_MUTED);
  text(9, 101, "1-9 preset  ENTER on/off", C_WHITE);
  text(9, 116, "DEL save/back  X cancel", C_MUTED);
  if (dailyAlarmSounding) text(9, 127, "RINGING - ENTER to stop", C_GOLD);
  else if (uiNoticeUntil && (int32_t)(uiNoticeUntil - millis()) > 0)
    text(9, 127, uiNotice, C_GOLD);
}
void drawScreen() {
  switch (screen) {
    case RADIO: drawPlayer(); break;
    case MOOD: drawMood(); break;
    case NOTES: drawNotes(); break;
    case NOTES_SEARCH: drawNotesSearch(); break;
    case SPARK: drawSpark(); break;
    case TRACKS: drawTracks(); break;
    case BROWSE: drawBrowse(); break;
    case SHARE: drawShare(); break;
    case TEMPO: drawTempo(); break;
    case CLOCK: drawNight(); break;
    case SOUND_FX: drawSoundFx(); break;
    case WIFI_LIST: drawWifiList(); break;
    case WIFI_PASSWORD: drawWifiPassword(); break;
    case WIFI_JOIN: drawWifiJoin(); break;
    case HELP: drawHelp(); break;
    case DIAGNOSTICS: drawDiagnostics(); break;
    case SETTINGS_BACKUP: drawSettingsBackup(); break;
    case CATALOG_UPDATE: drawCatalogUpdate(); break;
    case OFFLINE_LIBRARY: drawOfflineLibrary(); break;
    case DAILY_ALARM: drawDailyAlarm(); break;
    case RECENTS: drawRecents(); break;
    case SEARCH: drawSearch(); break;
  }
}
uint32_t uiSignature(uint32_t now) {
  uint32_t hash = 2166136261u;
  auto mix = [&](uint32_t value) { hash = (hash ^ value) * 16777619u; };
  auto mixText = [&](const char* value) {
    if (!value) return;
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(value); *c; ++c)
      mix(*c);
  };
  mix(screen);
  // Redraw only when something VISIBLE on this screen changes. The previous
  // global hash included Wi-Fi reconnect counters, audio state, battery and
  // hidden password fields on ALL screens. For example, HELP's fillScreen()
  // ran repeatedly during Wi-Fi association even though Help was unchanged.
  switch (screen) {
    case DAILY_ALARM:
      mix(dailyAlarmEdit.hour);
      mix(dailyAlarmEdit.minute);
      mix(dailyAlarmEdit.preset);
      mix(dailyAlarmEdit.enabled);
      mix(dailyAlarmSounding);
      mix(uiNoticeUntil);
      mix(uiNoticeUntil && (int32_t)(uiNoticeUntil - now) > 0);
      return hash;
    case OFFLINE_LIBRARY:
      mix(offlineSelected);
      mix(offlineLeaving);
      mix(offlineMode);
      mixText(offlinePath);
      mix(audioState.load());
      mix(volume);
      mix(uiNoticeUntil);
      return hash;
    case SETTINGS_BACKUP:
      mix(backupConfirmation);
      mix(uiNoticeUntil);
      mix(uiNoticeUntil && (int32_t)(uiNoticeUntil - now) > 0);
      return hash;
    case CATALOG_UPDATE:
      mix(catalogUpdateAttempted);
      mix(catalogUpdateInProgress);
      mix(static_cast<uint32_t>(catalogUpdateStatus));
      return hash;
    case HELP:
      mix(helpPage);
      return hash;  // Never repaint Help for background Wi-Fi or MP3 events.
    case SOUND_FX:
      mix(vinylCrackleEnabled.load(std::memory_order_relaxed));
      mix(tuningHissEnabled.load(std::memory_order_relaxed));
      mix(futureNvsSchema);
      return hash;
    case SPARK:
      mix(jazzSparkIndex);
      mix(jazzSparkAutoEnabled);
      return hash; // No background Wi-Fi/audio-triggered full-screen refresh.
    case WIFI_JOIN:
      mixText(wifiSsid.c_str());
      return hash;
    case WIFI_LIST:
      mix(wifiScanPending);
      mix(networkCount);
      mix(selectedNetwork);
      return hash;
    case WIFI_PASSWORD:
      mixText(wifiSsid.c_str());
      mixText(wifiPassword.c_str());
      mix(wifiShowPassword);
      mix(wifiJoinFailed);
      return hash;
    case MOOD:
      mix(moodEnergyCursor);
      mix(moodAcousticCursor);
      return hash;
    case NOTES:
      mixText(currentTitle);
      mix(notesDetailPage);
      return hash;
    case NOTES_SEARCH:
      mixText(notesSearchQuery.c_str());
      mix(notesSearchRan);
      mix(notesSearchSelected);
      mix(static_cast<uint32_t>(notesSearchOffset));
      mix(static_cast<uint32_t>(notesSearchTotal));
      return hash;
    case RECENTS:
      mix(recentCount);
      mix(recentSelected);
      for (int i = 0; i < recentCount; ++i) {
        mixText(recent[i].url);
        mixText(recent[i].name);
      }
      return hash;
    case SEARCH:
      mixText(stationSearch.c_str());
      mix(searchSelected);
      return hash;
    case SHARE:
      mix(currentList);
      mix(currentOffset);
      mix(shareQrPage);
      if (shareQrPage) mixText(lastLikedTitle);
      return hash;
    case TEMPO:
      mix(manualTempo.lastTap());
      mix(manualTempo.intervals());
      mix(manualTempo.bpm(now));
      return hash;
    case TRACKS:
      mix(trackHistory.revision());
      mix(uiNoticeUntil);
      mix(uiNoticeUntil && (int32_t)(uiNoticeUntil - now) > 0);
      return hash;
    case BROWSE:
      mix(browseList);
      mix(browseSelected);
      mix(stationPeek.revision());
      mix(stationPeek.busy());
      mix(currentList);
      mix(currentOffset);
      mixText(currentTitle);
      for (int row = 0; row < 4; ++row) {
        const int id = lists[browseList].first + browseFirstVisible() + row;
        if (id >= 0 && id < stationCount &&
            id < lists[browseList].first + lists[browseList].count)
          mix(stationHealth.streak(id));
      }
      return hash;
    case CLOCK:
      mix((uint32_t)(time(nullptr) / 60));
      mix(ntpSynced.load());
      mix(clockUserOverride);
      mix(static_cast<uint32_t>(clockOffsetMinutes));
      mix(audioState.load());
      mix(currentList);
      mix(currentOffset);
      mixText(currentTitle);
      return hash;
    case DIAGNOSTICS:
      mix(diagPage);
      mix(now / 1000);  // Diagnostic counters update once a second.
      return hash;
    case RADIO:
      mix(currentList);
      mix(currentOffset);
      mix(batteryLevel);
      mix(volume);
      mix(dailyAlarmSounding);
      if (dailyAlarmSounding) mix(dailyAlarmOutputVolume / 8);
      mix(sleepMinutes);
      if (sleepMinutes) mix((now - sleepStartedAt) / 60000);
      mix(uiNoticeUntil);
      mix(uiNoticeUntil && (int32_t)(uiNoticeUntil - now) > 0);
      mixText(currentTitle);
      mix(audioState.load());
      mix(wifiReady.load());
      mix(wifiHasIp());
      return hash;  // Hidden Wi-Fi event counters never repaint radio.
  }
  return hash;
}
} // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.external_imu = false;
  cfg.external_rtc = false;
  cfg.serial_baudrate = 0;
  M5Cardputer.begin(cfg, true);
  auto& d = M5Cardputer.Display;
  d.setRotation(1);
  d.setBrightness(135);
  lastKeyActivity = millis();
  d.setTextFont(1);
  d.fillScreen(C_BG);
  text(10, 15, "POCKET JAZZ", C_GOLD, 2);
  text(10, 45, "Loading stations...", C_WHITE);
  loadPlaylists();
  loadPeekConfig();
  restorePrefs();
  restoreDailyRadioAlarm();
  loadClockSettings(); // Apply saved NVS offset after reading preferences.
  M5Cardputer.update();
  safeMode = M5Cardputer.Keyboard.isKeyPressed('s');
  wantPlay = autoPlay && !safeMode;
  if (safeMode) Serial.println("[PocketJazz] Safe start: playback disabled");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false); // no ESP-IDF Wi-Fi driver NVS writes
  WiFi.setSleep(false);
  WiFi.onEvent(wifiEvent);
  if (!savedSsid.isEmpty()) connectSaved();
  audioQueue = xQueueCreate(1, sizeof(AudioCommand));
  metadataQueue = xQueueCreate(1, sizeof(TrackMetadata));
  if (!audioQueue || !metadataQueue) audioState.store(ERROR_MEMORY);
  else {
    // Separate decoding from LCD/keyboard/Wi-Fi screens.
    if (xTaskCreatePinnedToCore(audioWorker, "pocketjazz_audio", 8192, nullptr, 1, nullptr, 1) != pdPASS)
      audioState.store(ERROR_MEMORY);
  }
  if (peekOptIn) {
    peekQueue = xQueueCreate(1, sizeof(PeekCommand));
    peekResultQueue = xQueueCreate(1, sizeof(PeekResult));
    if (!peekQueue || !peekResultQueue ||
        xTaskCreatePinnedToCore(stationPeekWorker, "pocketjazz_peek", 6144,
                                nullptr, 1, nullptr, 0) != pdPASS) {
      // No opportunistic fallback to unbounded polling or a UI-thread client.
      peekOptIn = false;
      if (peekQueue) { vQueueDelete(peekQueue); peekQueue = nullptr; }
      if (peekResultQueue) { vQueueDelete(peekResultQueue); peekResultQueue = nullptr; }
    }
  }
  sendAudio();
  lastStatus = millis();
  lastAutoSparkAt = millis();
}
void loop() {
  static uint32_t lastLoopStarted = 0;
  const uint32_t loopStarted = millis();
  if (lastLoopStarted) {
    const uint32_t gap = loopStarted - lastLoopStarted;
    if (gap > uiLoopMaxGapMs.load()) uiLoopMaxGapMs.store(gap);
  }
  lastLoopStarted = loopStarted;
  M5Cardputer.update();
  const Press input = readKeys();
  const uint32_t before = millis();
  if (M5Cardputer.Keyboard.isPressed()) {
    lastKeyActivity = before;
    if (screenDimmed) {
      screenDimmed = false;
      M5Cardputer.Display.setBrightness(135);
      // A dimmed screen still honors a volume press on the first key stroke.
      // Never turn the wake-up key into a station change or text insertion.
      const bool isVolumeKey = pressed(input, '-') || pressed(input, '_') ||
                               pressed(input, '=') || pressed(input, '+');
      const bool typingText = screen == WIFI_PASSWORD || screen == SEARCH ||
                              (screen == NOTES_SEARCH && !notesSearchRan);
      if (isVolumeKey && (!typingText || input.fn)) handleKeys(input);
    } else {
      handleKeys(input);
    }
  } else if (!screenDimmed && before - lastKeyActivity >= 60000) {
    screenDimmed = true;
    M5Cardputer.Display.setBrightness(38);
  }
  serviceWifi();
  reportAudioFailureToSerial();
  const uint32_t now = millis();
  if (!lastBatteryAt || now - lastBatteryAt >= 15000) {
    lastBatteryAt = now;
    const int measured = (int)M5Cardputer.Power.getBatteryLevel();
    batteryLevel = (measured >= 0 && measured <= 100) ? measured : -1;
    const int voltage = (int)M5Cardputer.Power.getBatteryVoltage();
    batteryMillivolts = (voltage >= 2500 && voltage <= 4500) ? voltage : -1;
  }
  observeAudioRuntime(now);
  serviceRecent(now);
  serviceSleepTimer(now);
  serviceOfflineCompletion();
  serviceDailyRadioAlarm(now);
  serviceMetadata();
  serviceStationPeek(now);
  serviceJazzSpark(now);
  if (prefDirty && now - prefTouched >= PREF_SAVE_DEBOUNCE_MS &&
      !(offlineMode && audioState.load() == PLAYING)) savePrefs();
  const uint32_t signature = uiSignature(now);
  if (!uiPainted || screen != lastPaintedScreen || signature != lastUiSignature) {
    if (uiPainted && screen == RADIO && lastPaintedScreen == RADIO) {
      drawPlayer(true);
    } else if (uiPainted && screen == CLOCK && lastPaintedScreen == CLOCK) {
      drawNight(true);
    } else if (uiPainted && screen == DIAGNOSTICS && lastPaintedScreen == DIAGNOSTICS) {
      drawDiagnostics(true);
    } else {
      drawScreen();
    }
    lastPaintedScreen = screen;
    lastUiSignature = signature;
    uiPainted = true;
  }
  serviceVu(now);
  delay(12);
}
} // namespace pocketjazz

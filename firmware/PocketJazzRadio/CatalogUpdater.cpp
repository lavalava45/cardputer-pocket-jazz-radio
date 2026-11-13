#include "CatalogUpdater.h"
#include "RadioTrustedRoots.h"
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <StreamString.h>
#include <time.h>
#include <cstring>
#include <cctype>

namespace pocketjazz {
namespace {
// Keep the update endpoint fixed: a downloaded manifest cannot redirect to
// arbitrary hosts or paths. Review trust anchors when GitHub rotates its CA.
constexpr char kRoot[] = "https://raw.githubusercontent.com/lavalava45/cardputer-pocket-jazz-radio/main/";
constexpr char kStage[] = "/PocketJazz/.catalog-stage";
constexpr char kBackup[] = "/PocketJazz/.catalog-backup";
constexpr char kPending[] = "/PocketJazz/.catalog-pending";
constexpr char kCommitted[] = "/PocketJazz/.catalog-committed";
constexpr size_t kMaxManifest = 4096;
constexpr size_t kMaxPlaylist = 32768;
constexpr time_t kMinTime = 1704067200;
const char* const kNames[5] = {
  "01-essentials.m3u", "02-modern-jazz.m3u", "03-mellow-bossa.m3u",
  "05-swing-vocals.m3u", "06-latin-soul-jazz.m3u"
};
struct Entry { size_t bytes = 0; unsigned stations = 0; char sha[65]{}; };
// No transient downloaded manifest remains allocated during playback.
// Five bytes capture the expected parser counts while activation is pending.
uint8_t activationExpectedStations[5]{};
bool expect(Stream& file, const String& expected) {
  String line = file.readStringUntil('\n');
  line.trim();
  return line == expected;
}
bool numberLine(Stream& file, const char* key, unsigned& number, bool trailingComma = true) {
  String line = file.readStringUntil('\n');
  line.trim();
  const String prefix = String("\"") + key + "\": ";
  if (!line.startsWith(prefix)) return false;
  line.remove(0, prefix.length());
  if (trailingComma) {
    if (!line.endsWith(",")) return false;
    line.remove(line.length() - 1);
  }
  if (!line.length() || line.length() > 6) return false;
  uint32_t n = 0;
  for (size_t i = 0; i < line.length(); ++i) {
    if (line[i] < '0' || line[i] > '9') return false;
    n = n * 10 + line[i] - '0';
  }
  number = n;
  return true;
}
bool stringLine(Stream& file, const char* key, String& value, bool comma = true) {
  String line = file.readStringUntil('\n');
  line.trim();
  const String prefix = String("\"") + key + "\": \"";
  if (!line.startsWith(prefix) || !line.endsWith(comma ? "\"," : "\"")) return false;
  value = line.substring(prefix.length(), line.length() - (comma ? 2 : 1));
  return value.length() > 0 && value.indexOf('"') < 0 && value.indexOf('\\') < 0;
}

String path(const char* directory, const char* filename) {
  return String(directory) + "/" + filename;
}
String live(const char* filename) { return path("/PocketJazz", filename); }

bool parseManifest(Stream& file, size_t bytes, Entry (&entries)[5]) {
  if (bytes > kMaxManifest || bytes < 800) return false;
  if (!expect(file, "{") || !expect(file, "\"schema_version\": 1,")) return false;
  String value;
  if (!stringLine(file, "catalog_version", value) ||
      !stringLine(file, "repository", value) ||
      value != "lavalava45/cardputer-pocket-jazz-radio") return false;
  unsigned declaredFiles = 0, declaredStations = 0, alternateUrls = 0;
  if (!numberLine(file, "playlists", declaredFiles) || declaredFiles != 5 ||
      !numberLine(file, "stations", declaredStations) || declaredStations > 64 ||
      !numberLine(file, "alternate_urls", alternateUrls) || alternateUrls > 64 ||
      !expect(file, "\"files\": [")) return false;
  unsigned sumStations = 0;
  for (int i = 0; i < 5; ++i) {
    if (!expect(file, "{") || !stringLine(file, "path", value) ||
        value != String("catalog/") + kNames[i] ||
        !stringLine(file, "sha256", value) || value.length() != 64) return false;
    for (int j = 0; j < 64; ++j) {
      const char c = value[j];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
      entries[i].sha[j] = c;
    }
    entries[i].sha[64] = 0;
    unsigned bytes = 0, count = 0;
    if (!numberLine(file, "bytes", bytes) || !bytes || bytes > kMaxPlaylist ||
        !numberLine(file, "stations", count, false) || !count || count > 64 ||
        !expect(file, i == 4 ? "}" : "},")) return false;
    entries[i].bytes = bytes;
    entries[i].stations = count;
    sumStations += count;
  }
  if (sumStations != declaredStations || !expect(file, "],") ||
      !expect(file, "\"pc_verification\": {")) return false;
  // Extra PC diagnostic metadata is never executable or used for routing.
  // Strictly bound size; the preceding schema fields govern all downloads.
  bool closed = false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line == "}") { closed = true; break; }
    if (line.length() > 256) return false;
  }
  if (!closed || !expect(file, "}") ) return false;
  while (file.available()) if (!isspace((unsigned char)file.read())) return false;
  return true;
}

bool shaMatches(File& file, const Entry& expected) {
  if (!file || file.size() != expected.bytes) return false;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  if (mbedtls_sha256_starts(&ctx, 0) != 0) { mbedtls_sha256_free(&ctx); return false; }
  uint8_t buf[512];
  size_t readBytes = 0;
  while (file.available()) {
    const size_t n = file.read(buf, sizeof(buf));
    if (!n || readBytes + n > expected.bytes || mbedtls_sha256_update(&ctx, buf, n) != 0) {
      mbedtls_sha256_free(&ctx);
      return false;
    }
    readBytes += n;
  }
  uint8_t digest[32]{};
  const bool ok = readBytes == expected.bytes && mbedtls_sha256_finish(&ctx, digest) == 0;
  mbedtls_sha256_free(&ctx);
  if (!ok) return false;
  const char hex[] = "0123456789abcdef";
  char calculated[65]{};
  for (int i = 0; i < 32; ++i) {
    calculated[i * 2] = hex[digest[i] >> 4];
    calculated[i * 2 + 1] = hex[digest[i] & 15];
  }
  return memcmp(calculated, expected.sha, 64) == 0;
}

bool validateM3u(File& file, unsigned& stations, bool strictHttps = false) {
  if (!file || !file.size() || file.size() > kMaxPlaylist) return false;
  stations = 0;
  bool sawHeader = false, hasName = false;
  while (file.available()) {
    String row = file.readStringUntil('\n');
    if (row.length() > 512) return false;
    row.trim();
    if (!row.length()) continue;
    if (!sawHeader) {
      if (row != "#EXTM3U") return false;
      sawHeader = true;
    } else if (row.startsWith("#EXTINF:")) {
      hasName = row.indexOf(',') >= 0;
    } else if (row.startsWith("#PJ-ALT:")) {
      const String alt = row.substring(8);
      if (!(alt.startsWith("https://") ||
            (!strictHttps && alt.startsWith("http://")))) return false;
    } else if (row.startsWith("#PJ-MOOD:")) {
      // The existing parser validates mood values; no extra station created.
    } else if (row[0] != '#') {
      if (!hasName || !(row.startsWith("https://") ||
                        (!strictHttps && row.startsWith("http://"))) ||
          row.length() >= 192 || ++stations > 64) return false;
      hasName = false;
    }
  }
  return sawHeader && (!strictHttps || stations > 0);
}
bool localStationCount(unsigned& total) {
  total = 0;
  File dir = SD.open("/PocketJazz");
  if (!dir || !dir.isDirectory()) return false;
  int files = 0;
  File f;
  while ((f = dir.openNextFile())) {
    String name = f.name();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name.remove(0, slash + 1);
    if (!f.isDirectory() && (name.endsWith(".m3u") || name.endsWith(".M3U") ||
                             name.endsWith(".pls") || name.endsWith(".PLS"))) {
      ++files;
      bool managed = false;
      for (auto* allowed : kNames) if (name == allowed) managed = true;
      if (!managed) {
        if (name.endsWith(".pls") || name.endsWith(".PLS")) {
          // Mirror loadPlaylists() exactly: Arduino String::toInt accepts
          // numeric prefixes (e.g. "File1extra"), and repeated FileN
          // replaces the previous URL. An undercount could silently truncate
          // personal stations when 56 curated entries are activated.
          uint32_t seen = 0;
          while (f.available()) {
            String line = f.readStringUntil('\n');
            if (line.length() > 512) continue;
            line.trim();
            const int eq = line.indexOf('=');
            if (eq < 5 || !line.startsWith("File")) continue;
            String index = line.substring(4, eq);
            const int n = index.toInt();
            if (n < 1 || n > 32) continue;
            String url = line.substring(eq + 1);
            if (url.startsWith("https://") || url.startsWith("http://"))
              seen |= uint32_t(1) << (n - 1);
            else
              seen &= ~(uint32_t(1) << (n - 1));
          }
          total += __builtin_popcount(seen);
        } else {
          unsigned n = 0;
          if (!validateM3u(f, n)) { f.close(); dir.close(); return false; }
          total += n;
        }
      }
    }
    f.close();
  }
  dir.close();
  return files <= 12 && total <= 64;
}

bool download(const String& url, const String& destination, size_t limit) {
  if (SD.exists(destination)) return false;
  WiFiClientSecure tls;
  tls.setCACert(kRadioTrustedRootsPem);
  tls.setHandshakeTimeout(7);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setConnectTimeout(7000);
  http.setTimeout(7000);
  if (!http.begin(tls, url)) return false;
  http.setReuse(false);
  if (http.GET() != HTTP_CODE_OK || http.getSize() > (int)limit) {
    http.end(); return false;
  }
  File out = SD.open(destination, FILE_WRITE);
  if (!out) { http.end(); return false; }
  auto* input = http.getStreamPtr();
  const int expected = http.getSize();
  size_t written = 0;
  uint32_t lastProgress = millis();
  uint8_t buffer[512];
  bool ok = true;
  while (http.connected() || input->available()) {
    const int available = input->available();
    if (!available) {
      if (millis() - lastProgress > 7000) { ok = false; break; }
      delay(5);
      continue;
    }
    const size_t n = input->readBytes(buffer, min((int)sizeof(buffer), available));
    if (!n || written + n > limit || out.write(buffer, n) != n) { ok = false; break; }
    written += n;
    lastProgress = millis();
    if (expected >= 0 && written == (size_t)expected) break;
  }
  out.flush();
  out.close();
  http.end();
  if (!ok || written == 0 || (expected >= 0 && written != (size_t)expected)) {
    SD.remove(destination);
    return false;
  }
  return true;
}

bool fetchManifest(String& body) {
  WiFiClientSecure tls;
  tls.setCACert(kRadioTrustedRootsPem);
  tls.setHandshakeTimeout(7);
  HTTPClient http;
  http.setConnectTimeout(7000);
  http.setTimeout(7000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(tls, String(kRoot) + "manifest.json")) return false;
  const int status = http.GET();
  const int size = http.getSize();
  if (status != HTTP_CODE_OK || size < 800 || size > (int)kMaxManifest) {
    http.end(); return false;
  }
  body = http.getString();
  http.end();
  return body.length() == (size_t)size;
}

bool removeStage() {
  for (auto* name : kNames) {
    const String staged = path(kStage, name);
    if (SD.exists(staged) && !SD.remove(staged)) return false;
  }
  const String obsolete = path(kStage, "manifest.json");
  if (SD.exists(obsolete) && !SD.remove(obsolete)) return false;
  return !SD.exists(kStage) || SD.rmdir(kStage);
}
bool removeBackups() {
  for (auto* name : kNames)
    if (SD.exists(path(kBackup, name)) && !SD.remove(path(kBackup, name))) return false;
  return !SD.exists(kBackup) || SD.rmdir(kBackup);
}
bool removeEmptyBackupDirectory() {
  // A journal creation/write failure happens before ANY live rename.
  // It can strand an EMPTY backup directory. Never delete a nonempty
  // unjournaled backup; it may contain the only copies of user data.
  if (!SD.exists(kBackup)) return true;
  File dir = SD.open(kBackup, FILE_READ);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }
  File entry = dir.openNextFile();
  const bool empty = !entry;
  if (entry) entry.close();
  dir.close();
  return empty && SD.rmdir(kBackup);
}
bool markerContains(const char* marker, const char* expected) {
  File file = SD.open(marker, FILE_READ);
  if (!file) return false;
  const size_t length = strlen(expected);
  if (file.size() != length) { file.close(); return false; }
  for (size_t i = 0; i < length; ++i) {
    if (file.read() != expected[i]) { file.close(); return false; }
  }
  file.close();
  return true;
}
bool rollbackPending() {
  // Files restored in an earlier interrupted rollback have no backup left.
  // Files still in .catalog-backup are always the original versions.
  for (auto* name : kNames) {
    const String backup = path(kBackup, name);
    if (!SD.exists(backup)) {
      // Original already restored OR rename transaction never reached it.
      // Both cases require an existing live copy; never synthesize a file.
      if (!SD.exists(live(name))) return false;
      continue;
    }
    const String destination = live(name);
    if (SD.exists(destination) && !SD.remove(destination)) return false;
    if (!SD.rename(backup, destination)) return false;
  }
  if (!removeBackups() || !removeStage()) return false;
  return SD.remove(kPending);
}
} // namespace

CatalogUpdateResult recoverCatalogUpdate() {
  // Only an exact, fully-written commit marker authorizes deleting backups.
  // An interrupted marker write must trigger rollback, never partial commit.
  if (SD.exists(kCommitted)) {
    if (markerContains(kCommitted, "committed\n")) {
      if (!removeBackups()) return CatalogUpdateResult::RecoveryFailed;
      if (!removeStage()) return CatalogUpdateResult::RecoveryFailed;
      if (SD.exists(kPending) && !SD.remove(kPending))
        return CatalogUpdateResult::RecoveryFailed;
      if (!SD.remove(kCommitted)) return CatalogUpdateResult::RecoveryFailed;
      return CatalogUpdateResult::Ok;
    }
    // If a once-valid commit marker became truncated during backup cleanup,
    // some original backups might already be gone. A blind rollback could
    // produce a mixture of old and new default playlists. Fail CLOSED and
    // leave the journal/files untouched for manual recovery in that case.
    if (SD.exists(kPending) && SD.exists(kBackup)) {
      for (auto* name : kNames)
        if (!SD.exists(path(kBackup, name)))
          return CatalogUpdateResult::RecoveryFailed;
    }
    if (!SD.remove(kCommitted)) return CatalogUpdateResult::RecoveryFailed;
  }
  if (!SD.exists(kPending)) {
    // Power loss while downloading to the isolated staging directory: no
    // live playlist has been touched, so simply discard the abandoned stage.
    // Unexpected backups without a journal are ambiguous: never delete them.
    if (!removeEmptyBackupDirectory()) return CatalogUpdateResult::RecoveryFailed;
    if (!removeStage()) return CatalogUpdateResult::RecoveryFailed;
    return CatalogUpdateResult::NoChange;
  }
  // Pending means activation never committed. Restore original files before
  // app reload on next boot; resumable if power disappears during rollback.
  return rollbackPending() ? CatalogUpdateResult::Ok
                           : CatalogUpdateResult::RecoveryFailed;
}

CatalogUpdateResult updateCatalogManually(bool audioStopped, bool sdMounted) {
  if (!audioStopped || !sdMounted || WiFi.status() != WL_CONNECTED ||
      time(nullptr) < kMinTime) return CatalogUpdateResult::Busy;
  // An outstanding transaction belongs to the activation caller; do not
  // silently undo it if the user presses Update twice before finalize().
  if (SD.exists(kPending) || SD.exists(kCommitted)) return CatalogUpdateResult::Busy;
  if (recoverCatalogUpdate() == CatalogUpdateResult::RecoveryFailed)
    return CatalogUpdateResult::RecoveryFailed;
  // The transaction uses reserved names. Refuse ambiguous remnants from
  // interrupted staging or a previous nonstandard update.
  if (SD.exists(kStage) || SD.exists(kBackup) || SD.exists(kPending) ||
      SD.exists(kCommitted)) return CatalogUpdateResult::StorageError;
  Entry entries[5]{};
  {
    // Explicit lifetime: the 1–4 KiB manifest String and StreamString
    // MUST release their heap before the following sequential HTTPS/TLS
    // downloads. Keeping both alive reduced contiguous memory for TLS.
    String body;
    if (!fetchManifest(body)) return CatalogUpdateResult::DownloadFailed;
    StreamString input;
    if (input.write(reinterpret_cast<const uint8_t*>(body.c_str()),
                    body.length()) != body.length() ||
        !parseManifest(input, body.length(), entries))
      return CatalogUpdateResult::InvalidManifest;
  }
  // Avoid unnecessary SD rename transactions and five HTTPS downloads when
  // the installed official files already have the exact published bytes.
  // A user-edited default file still differs and can be replaced only after
  // the explicit confirmation screen; personal files are never compared.
  bool alreadyCurrent = true;
  for (int i = 0; i < 5; ++i) {
    File installed = SD.open(live(kNames[i]), FILE_READ);
    const bool same = shaMatches(installed, entries[i]);
    installed.close();
    if (!same) { alreadyCurrent = false; break; }
  }
  if (alreadyCurrent) return CatalogUpdateResult::NoChange;
  if (!SD.mkdir(kStage)) return CatalogUpdateResult::StorageError;
  CatalogUpdateResult outcome = CatalogUpdateResult::DownloadFailed;
  do {
    unsigned other = 0, updated = 0;
    if (!localStationCount(other)) { outcome = CatalogUpdateResult::InvalidCatalog; break; }
    bool checked = true;
    for (int i = 0; i < 5; ++i) {
      const String destination = live(kNames[i]);
      if (!SD.exists(destination) ||
          !download(String(kRoot) + "catalog/" + kNames[i],
                    path(kStage, kNames[i]), kMaxPlaylist)) { checked = false; break; }
      File candidate = SD.open(path(kStage, kNames[i]), FILE_READ);
      const bool hashOk = shaMatches(candidate, entries[i]);
      candidate.close();
      candidate = SD.open(path(kStage, kNames[i]), FILE_READ);
      unsigned count = 0;
      const bool playlistOk = validateM3u(candidate, count, true);
      candidate.close();
      if (!hashOk || !playlistOk || count != entries[i].stations) {
        checked = false; outcome = CatalogUpdateResult::InvalidPlaylist; break;
      }
      updated += count;
    }
    if (!checked) break;
    if (other + updated > 64) { outcome = CatalogUpdateResult::InvalidCatalog; break; }
    if (!SD.mkdir(kBackup)) { outcome = CatalogUpdateResult::StorageError; break; }
    File journal = SD.open(kPending, FILE_WRITE);
    if (!journal) { outcome = CatalogUpdateResult::StorageError; break; }
    const bool journalWritten = journal.print("pending\n") == strlen("pending\n");
    journal.flush();
    journal.close();
    if (!journalWritten || !markerContains(kPending, "pending\n")) {
      outcome = CatalogUpdateResult::StorageError;
      break;
    }
    bool moved = true;
    for (auto* name : kNames) {
      if (!SD.rename(live(name), path(kBackup, name)) ||
          !SD.rename(path(kStage, name), live(name))) { moved = false; break; }
    }
    if (!moved) {
      outcome = recoverCatalogUpdate() == CatalogUpdateResult::RecoveryFailed
                ? CatalogUpdateResult::RecoveryFailed : CatalogUpdateResult::StorageError;
      break;
    }
    // Phase 1 complete: keep old files and pending marker until UI reloads
    // and positively confirms that the downloaded catalog is active.
    for (int i = 0; i < 5; ++i)
      activationExpectedStations[i] = static_cast<uint8_t>(entries[i].stations);
    outcome = CatalogUpdateResult::ReadyToActivate;
  } while (false);
  if (!SD.exists(kPending) && !SD.exists(kCommitted)) {
    if (!removeEmptyBackupDirectory() || !removeStage())
      return CatalogUpdateResult::StorageError;
  }
  return outcome;
}

bool catalogActivationMatches(const uint8_t observedStationCounts[5]) {
  if (!observedStationCounts || !markerContains(kPending, "pending\n")) return false;
  for (int i = 0; i < 5; ++i)
    if (!activationExpectedStations[i] ||
        observedStationCounts[i] != activationExpectedStations[i]) return false;
  return true;
}

CatalogUpdateResult finalizeCatalogUpdate(bool activated) {
  if (!SD.exists(kPending) || !markerContains(kPending, "pending\n") ||
      SD.exists(kCommitted)) return CatalogUpdateResult::RecoveryFailed;
  if (!activated) {
    return rollbackPending() ? CatalogUpdateResult::ActivationFailed
                             : CatalogUpdateResult::RecoveryFailed;
  }
  // Guard activation acknowledgement against incomplete rename sequences.
  // Five original backups and five replacement live files must still exist.
  for (auto* name : kNames) {
    if (!SD.exists(live(name)) || !SD.exists(path(kBackup, name)))
      return CatalogUpdateResult::RecoveryFailed;
  }
  File committed = SD.open(kCommitted, FILE_WRITE);
  if (!committed) return CatalogUpdateResult::RecoveryFailed;
  const bool wrote = committed.print("committed\n") == strlen("committed\n");
  committed.flush();
  committed.close();
  if (!wrote || !markerContains(kCommitted, "committed\n")) {
    // Leave pending+backups for safe rollback by the next boot/retry.
    if (SD.exists(kCommitted) && !SD.remove(kCommitted))
      return CatalogUpdateResult::RecoveryFailed;
    return CatalogUpdateResult::RecoveryFailed;
  }
  return recoverCatalogUpdate() == CatalogUpdateResult::Ok
         ? CatalogUpdateResult::Ok : CatalogUpdateResult::RecoveryFailed;
}

const char* catalogUpdateResultText(CatalogUpdateResult result) {
  switch (result) {
    case CatalogUpdateResult::Ok: return "Catalog updated";
    case CatalogUpdateResult::ReadyToActivate: return "Catalog ready to activate";
    case CatalogUpdateResult::ActivationFailed: return "Catalog activation failed; previous catalog restored";
    case CatalogUpdateResult::NoChange: return "Catalog already up to date";
    case CatalogUpdateResult::Busy: return "Stop radio, mount SD, connect Wi-Fi and sync clock";
    case CatalogUpdateResult::InvalidManifest: return "Invalid catalog manifest";
    case CatalogUpdateResult::DownloadFailed: return "Catalog download failed";
    case CatalogUpdateResult::InvalidPlaylist: return "Playlist checksum or contents invalid";
    case CatalogUpdateResult::InvalidCatalog: return "Catalog exceeds 12 lists or 64 stations";
    case CatalogUpdateResult::StorageError: return "Catalog SD transaction failed";
    default: return "Catalog recovery failed";
  }
}
} // namespace pocketjazz

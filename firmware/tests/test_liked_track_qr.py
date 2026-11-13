"""Like/search QR integration and privacy guardrails, not a phone-scan test."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")
QR = (ROOT / "PocketJazzRadio" / "ShareStationQR.h").read_text(encoding="utf-8")
SRC = (ROOT / "PocketJazzRadio" / "TrackSearchUrl.h").read_text(encoding="utf-8")


class LikeQrContracts(unittest.TestCase):
    def test_station_qr_kept_and_search_uses_only_local_last_successful_like(self):
        liked = APP.split("void likeCurrentTrack() {", 1)[1].split("int findStationUrl", 1)[0]
        self.assertIn('appendUiExport("/PocketJazz/liked.txt"', liked)
        self.assertIn("if (result == EXPORT_WRITTEN)", liked)
        self.assertIn("strlcpy(lastLikedTitle, safeTrack", liked)
        self.assertIn('lastLikedTitle[0] =', liked)
        self.assertIn('drawStationQr(d, stations[idx].url', APP)
        self.assertIn("drawLikedTrackSearchQr(", APP)
        self.assertIn("if (k.up) shareQrPage = (shareQrPage + 2) % 3", APP)
        self.assertIn("if (k.down) shareQrPage = (shareQrPage + 1) % 3", APP)
        self.assertIn('if (k.del) screen = RADIO;', APP)
        self.assertIn("if (shareQrPage) mixText(lastLikedTitle)", APP)
        self.assertNotIn("SpotifyClient", APP)
        self.assertNotIn("spotifyToken", APP)

    def test_fixed_known_hosts_no_hidden_api_auth_or_media_capture(self):
        self.assertIn('"https://open.spotify.com/search/"', SRC)
        self.assertIn('"https://www.google.com/search?q="', SRC)
        self.assertIn("kTrackQrMaxUrlBytes = 106", SRC)
        self.assertIn("trackUtf8Length", SRC)
        self.assertIn("if (shortened) *shortened = true", SRC)
        self.assertNotIn("api.spotify.com", SRC)
        self.assertNotIn("client_secret", SRC)
        self.assertIn("constexpr int version = 6", QR)
        self.assertIn("ECC_MEDIUM", QR)
        self.assertIn("storage[240]", QR)


if __name__ == "__main__":
    unittest.main()

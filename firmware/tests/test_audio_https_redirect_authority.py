"""Regression fixtures for HTTPS root-relative Location authority extraction.

These are host URL-policy fixtures plus a source guard. They do not simulate
an ESP32 TLS handshake; TLS certificate/hostname checks remain on device.
"""
import unittest
from pathlib import Path


SOURCE = (Path(__file__).resolve().parents[1] /
          "PocketJazzRadio/SecureICYSource.cpp").read_text(encoding="utf-8")


def redirect_root(base: str, location: str) -> str:
    assert base.startswith("https://") and location.startswith("/")
    assert not location.startswith("//")
    slash = base.find("/", 8)
    query = base.find("?", 8)
    end = (len(base) if query < 0 else query) if slash < 0 else (
        slash if query < 0 else min(slash, query))
    return base[:end] + location


class HttpsRedirectAuthorityTest(unittest.TestCase):
    def test_root_relative_location_preserves_only_authority(self):
        cases = [
            ("https://radio.example?key=old", "/live.mp3", "https://radio.example/live.mp3"),
            ("https://radio.example", "/live.mp3", "https://radio.example/live.mp3"),
            ("https://radio.example/old?key=old", "/live.mp3", "https://radio.example/live.mp3"),
            ("https://radio.example:8443?key=old", "/live?new=1", "https://radio.example:8443/live?new=1"),
            ("https://radio.example/path", "/live.mp3", "https://radio.example/live.mp3"),
        ]
        for base, location, expected in cases:
            with self.subTest(base=base):
                self.assertEqual(redirect_root(base, location), expected)

    def test_production_code_uses_both_authority_delimiters(self):
        resolver = SOURCE.split("bool SecureICYSource::resolveRedirect(", 1)[1].split(
            "bool SecureICYSource::startRequest(", 1)[0]
        self.assertIn("base.indexOf('/', 8)", resolver)
        self.assertIn("base.indexOf('?', 8)", resolver)
        self.assertIn("std::min(slash, query)", resolver)
        self.assertIn("base.substring(0, end) + target", resolver)
        self.assertIn("return safeUrl(next)", resolver)
        self.assertIn('target.startsWith("https://")', resolver)
        self.assertIn('!target.startsWith("//")', resolver)
        self.assertNotIn("setInsecure(", SOURCE)


if __name__ == "__main__":
    unittest.main()

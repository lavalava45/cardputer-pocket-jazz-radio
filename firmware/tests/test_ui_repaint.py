"""Offline regression guards for LCD repaint policy (no hardware required)."""

import re
import unittest
from pathlib import Path


SOURCE = (Path(__file__).resolve().parents[1] / "PocketJazzRadio" / "PocketJazzRadioApp.cpp").read_text(encoding="utf-8")


def function_body(name: str) -> str:
    """Find a C++ function body with balanced braces; independent of its formatting."""
    match = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{", SOURCE)
    if not match:
        raise AssertionError(f"Missing {name}()")
    start, depth = match.end(), 1
    for index in range(start, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[start:index]
    raise AssertionError(f"Unbalanced braces in {name}()")


class LCDRepaintContract(unittest.TestCase):
    def test_diagnostics_partial_refresh_clears_all_pixels(self):
        body = function_body("drawDiagnostics")
        # Both full and incremental diagnostics paths repaint the same 135
        # physical rows. No two-pixel strip may retain data across pages.
        self.assertIn("d.fillScreen(C_BG);", body)
        self.assertNotIn("d.fillRect(0, 0, 240, 114", body)
        self.assertNotIn("d.fillRect(0, 116, 240, 19", body)
        self.assertNotIn("createSprite(", body)

    def test_loop_does_not_redraw_unconditionally_on_fixed_timer(self):
        body = function_body("loop")
        self.assertFalse(
            re.search(r"if\s*\(\s*now\s*-\s*lastFrame\s*>=\s*\d+\s*\)\s*\{\s*lastFrame\s*=\s*now;\s*drawScreen\s*\(\s*\)", body),
            "Unconditional full-screen redraw on a fixed timer causes visible LCD flicker; add invalidation or scoped redraw",
        )

    def test_radio_screen_avoids_full_clear_on_periodic_refresh(self):
        body = function_body("drawPlayer")
        if re.search(r"\bfillScreen\s*\(", body):
            self.assertRegex(
                body,
                r"if\s*\(\s*!partial\s*\)\s*\{[\s\S]*?\bfillScreen\s*\(",
                "The LCD must clear entirely only on an initial scene draw, never on a partial refresh",
            )
            self.assertRegex(body, r"\bfillRect\s*\(", "Partial updates should erase only the regions they repaint")

    def test_ui_signature_is_screen_scoped_instead_of_wifi_global(self):
        # A regression in the global signature previously made full-screen
        # Help/Mood/Wi-Fi views repaint on unrelated Wi-Fi/audio activity.
        body = SOURCE.split("uint32_t uiSignature(uint32_t now) {", 1)[1].split(
            "} // namespace", 1
        )[0]
        branches = re.findall(
            r"\bcase\s+([A-Z_]+):([\s\S]*?)(?=\bcase\s+[A-Z_]+:|\n  \}\n  return hash;)",
            body,
        )
        expected = {
            "RADIO", "WIFI_LIST", "WIFI_PASSWORD", "WIFI_JOIN", "HELP",
            "DIAGNOSTICS", "RECENTS", "SEARCH", "MOOD",
            "NOTES", "SHARE", "TEMPO", "CLOCK", "SOUND_FX", "TRACKS",
            "BROWSE", "NOTES_SEARCH", "SPARK", "SETTINGS_BACKUP", "OFFLINE_LIBRARY",
            "DAILY_ALARM", "CATALOG_UPDATE",
        }
        self.assertEqual(set(dict(branches)), expected, "Each view must declare its visible dependencies")
        self.assertTrue(all("return hash;" in code for _, code in branches))
        self.assertNotIn("mix(wifiDisconnectEvents.load())", body)
        self.assertNotIn("mix(wifiGotIpEvents.load())", body)
        self.assertNotIn("mix(wifiScanPending)", dict(branches)["RADIO"])
        self.assertNotIn("mix(audioState.load())", dict(branches)["HELP"])
        alarm = dict(branches)["DAILY_ALARM"]
        self.assertIn("mix(dailyAlarmEdit.hour)", alarm)
        self.assertIn("mix(dailyAlarmEdit.minute)", alarm)
        self.assertIn("mix(dailyAlarmEdit.preset)", alarm)
        self.assertIn("mix(dailyAlarmEdit.enabled)", alarm)
        self.assertNotIn("mix(wifiDisconnectEvents.load())", alarm)

    def test_help_only_invalidates_when_page_changes(self):
        body = SOURCE.split("uint32_t uiSignature(uint32_t now) {", 1)[1].split(
            "} // namespace", 1
        )[0]
        help_branch = body.split("case HELP:", 1)[1].split("case SOUND_FX:", 1)[0]
        self.assertEqual(re.findall(r"\bmix\(([^)]+)\)", help_branch), ["helpPage"])
        self.assertIn("return hash;", help_branch)

    def test_radio_and_diagnostics_still_show_live_status(self):
        body = SOURCE.split("uint32_t uiSignature(uint32_t now) {", 1)[1].split(
            "} // namespace", 1
        )[0]
        radio = body.split("case RADIO:", 1)[1].split("\n  }\n", 1)[0]
        self.assertIn("mix(audioState.load())", radio)
        self.assertIn("mix(wifiReady.load())", radio)
        self.assertIn("mix(wifiHasIp())", radio)
        self.assertIn("mixText(currentTitle)", radio)
        diagnostic = body.split("case DIAGNOSTICS:", 1)[1].split("case RADIO:", 1)[0]
        self.assertIn("mix(diagPage)", diagnostic)
        self.assertIn("mix(now / 1000)", diagnostic)

    def test_station_navigation_hints_match_current_arrow_controls(self):
        radio = function_body("drawPlayer")
        browse = function_body("drawBrowse")
        controls = function_body("handleKeys")
        self.assertIn('"H - Help"', radio)
        self.assertNotIn('"A/D tune  P list"', radio)
        self.assertIn("if (pressed(k, 'h')) { helpPage = 0; screen = HELP; return; }", controls)
        self.assertIn('"UP/DOWN P NEXT PLAYLIST ENTER PLAY DEL"', browse)
        self.assertLessEqual(len("H - Help"), 24)
        self.assertIn('"ENTER stops alarm"', radio)
        self.assertLessEqual(len("UP/DOWN P NEXT PLAYLIST ENTER PLAY DEL"), 38)
        self.assertIn("if (k.left)", controls)
        self.assertIn("if (k.right)", controls)
        self.assertIn("if (pressed(k, 'p') && listCount > 1)", controls)

    def test_radio_full_width_station_artist_track_with_compact_vu(self):
        body = function_body("drawPlayer")
        self.assertIn('const char* station = idx >= 0 ? stations[idx].name', body)
        self.assertIn("drawWrappedRadioText(station, 28, C_WHITE, 2)", body)
        self.assertNotRegex(body, r'%02d\s*/\s*%02d')
        self.assertNotIn("snprintf(number,", body)
        self.assertIn("splitRadioMetadata(currentTitle, artist", body)
        self.assertIn('drawWrappedRadioText(artist, 49, C_MUTED, 2)', body)
        self.assertIn('drawWrappedRadioText(song, 70, C_GOLD, 2)', body)
        self.assertIn('drawWrappedRadioText(song, 52, C_GOLD, 3)', body)
        self.assertIn('drawVu(shownVu)', body)
        self.assertIn('"VOL %u%%"', body)
        self.assertNotIn("serviceRadioMarquee", SOURCE)
        helper = SOURCE.split("void drawWrappedRadioText(", 1)[1].split(
            "void splitRadioMetadata(", 1
        )[0]
        self.assertIn("setClipRect(kRadioTextX, y + row * 10, kRadioTextWidth, 10)", helper)
        self.assertIn("clearClipRect()", helper)
        self.assertIn("constexpr int kRadioTextWidth = 222", SOURCE)
        self.assertNotIn("VU OFF", SOURCE)

    def test_all_curated_station_names_are_bounded_and_marquee_can_show_all(self):
        root = Path(__file__).resolve().parents[1] / "playlists"
        count = 0
        for playlist in sorted(root.glob("*.m3u")):
            pending_name = None
            for raw in playlist.read_text("utf-8-sig").splitlines():
                if raw.startswith("#EXTINF:"):
                    pending_name = raw.split(",", 1)[1]
                    continue
                if not raw.startswith(("http://", "https://")):
                    continue  # Personal template has a COMMENTED example.
                self.assertIsNotNone(pending_name)
                name, pending_name = pending_name, None
                self.assertLessEqual(len(name.encode("utf-8")), 96)
                count += 1
        self.assertEqual(count, 51)


if __name__ == "__main__":
    unittest.main()

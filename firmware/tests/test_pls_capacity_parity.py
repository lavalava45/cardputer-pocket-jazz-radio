"""Guard personal PLS accounting against silent 64-station truncation.

These are host parser-policy examples and source guards. They do not access
the user's SD card or simulate a physical Cardputer FAT filesystem.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPDATER = (ROOT / "PocketJazzRadio/CatalogUpdater.cpp").read_text("utf-8")
RADIO = (ROOT / "PocketJazzRadio/PocketJazzRadioApp.cpp").read_text("utf-8")


def arduino_to_int_prefix(value: str) -> int:
    match = re.match(r"\s*([+-]?\d+)", value)
    return int(match.group(1)) if match else 0


def interpreted_pls_urls(lines: list[str]) -> int:
    urls = [""] * 32
    for line in lines:
        if len(line) > 512:
            continue
        line = line.strip()
        eq = line.find("=")
        if eq < 5:
            continue
        key = line[:eq]
        if not (key.startswith("File") or key.startswith("Title")):
            continue
        n = arduino_to_int_prefix(key[4:] if key.startswith("File") else key[5:])
        if not 1 <= n <= 32:
            continue
        if key.startswith("File"):
            urls[n - 1] = line[eq + 1:]
    return sum(url.startswith(("https://", "http://")) for url in urls)


class PlsCapacityParity(unittest.TestCase):
    def test_prefix_numbered_personal_pls_must_be_counted_before_catalog_update(self):
        personal = [f"File{i}extra=https://personal.example/{i}.mp3" for i in range(1, 15)]
        self.assertEqual(interpreted_pls_urls(personal), 14)
        self.assertGreater(51 + interpreted_pls_urls(personal), 64)
        app = RADIO.split("// PLS entries may place TitleN before or after FileN.", 1)[1]
        updater = UPDATER.split("if (name.endsWith(\".pls\")", 1)[1].split(
            "total += __builtin_popcount(seen);", 1
        )[0]
        self.assertIn("key.substring(fileKey ? 4 : 5).toInt()", app)
        self.assertIn("const int n = index.toInt();", updater)
        self.assertNotIn("if (!digits)", updater)
        self.assertNotIn("index.length() > 2", updater)

    def test_repeated_file_index_last_write_wins_even_if_invalid(self):
        personal = ["File1=https://personal.example/valid.mp3",
                    "File1=not-a-stream", "File2=http://personal.example/live"]
        self.assertEqual(interpreted_pls_urls(personal), 1)
        updater = UPDATER.split("if (name.endsWith(\".pls\")", 1)[1].split(
            "total += __builtin_popcount(seen);", 1
        )[0]
        self.assertIn("seen &= ~(uint32_t(1) << (n - 1))", updater)

    def test_pls_validator_keeps_personal_files_read_only(self):
        updater = UPDATER.split("bool localStationCount(unsigned& total)", 1)[1].split(
            "bool download(", 1
        )[0]
        self.assertIn("File f;", updater)
        self.assertNotIn("SD.remove(", updater)
        self.assertNotIn("SD.rename(", updater)
        self.assertIn("return files <= 12 && total <= 64", updater)


if __name__ == "__main__":
    unittest.main()

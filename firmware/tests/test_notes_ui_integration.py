"""Source contracts for explicit, bounded local MusicBrainz card search."""
import unittest
from pathlib import Path

SOURCE = (Path(__file__).resolve().parents[1] / 'PocketJazzRadio' / 'PocketJazzRadioApp.cpp').read_text(encoding='utf-8')


class NotesUI(unittest.TestCase):
    def test_search_requires_explicit_enter_and_ui_thread(self):
        self.assertIn('pressed(k, \'u\')', SOURCE)
        self.assertIn('if (!notesSearchRan) searchOfflineNotes();', SOURCE)
        self.assertIn('case NOTES_SEARCH: drawNotesSearch()', SOURCE)
        self.assertIn('notesSearchQuery.length() >= 24', SOURCE)
        self.assertIn('notesSearchResults, 3, &notesSearchTotal', SOURCE)
        self.assertIn('static pocketjazz::JazzNotesSearch searchIndex', SOURCE)
        self.assertIn('notesOpenedFromSearch ? NOTES_SEARCH : RADIO', SOURCE)
        self.assertNotIn('searchOfflineNotes()', SOURCE.split('void audioWorker(void*)', 1)[1].split('void loadPeekConfig()', 1)[0])
        self.assertNotIn('SD.open(', SOURCE.split('void drawNotesSearch()', 1)[1].split('void drawTracks()', 1)[0])

    def test_index_files_never_written(self):
        helper = SOURCE.split('void pageOfflineNotes(size_t offset) {', 1)[1].split('void openSelectedOfflineNote()', 1)[0]
        self.assertIn('SD.open("/PocketJazz/jazz-encyclopedia.tsv", FILE_READ)', helper)
        self.assertIn('SD.open("/PocketJazz/artist-notes.tsv", FILE_READ)', helper)
        self.assertIn('SD.open("/PocketJazz/artist-notes.idx", FILE_READ)', helper)
        self.assertIn('tsv.close()', helper)
        self.assertNotIn('FILE_WRITE', helper)
        self.assertNotIn('FILE_APPEND', helper)

    def test_result_mode_up_down_navigation_and_source_free_lcd(self):
        search = SOURCE.split('if (screen == NOTES_SEARCH) {', 1)[1].split('if (screen == TRACKS)', 1)[0]
        self.assertIn('k.up', search)
        self.assertIn('k.down', search)
        self.assertIn('pageOfflineNotes(', search)
        drawn = SOURCE.split("void drawNotes() {", 1)[1].split("void drawNotesSearch() {", 1)[0]
        self.assertNotIn('Source: MusicBrainz', drawn)
        self.assertIn('jazz-encyclopedia.tsv', drawn)


if __name__ == '__main__':
    unittest.main()

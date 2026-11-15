#!/usr/bin/env python3
"""Build the offline Jazz Journey dataset (original editorial text, sources pinned).

24 interactive stages across four trails; 30 standalone listening exercises.
No original copyrighted passages, lyrics, embedded stream links, or auto-play.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "S": "https://music.si.edu/story/jazz/",
    "J": "https://jazz.org/education/school-programs/let-freedom-swing/what-is-jazz/",
    "N": "https://www.nps.gov/jazz/learn/historyculture/history_early.htm",
    "B": "https://americanhistory.si.edu/explore/projects/smithsonian-jazz/collections-and-archives/latin-jazz/timeline",
}

# title | era | story | artist key | listening task | question | A | B | C | answer | spark topic | source code.
LESSONS = """
New Orleans mix|1900s|Jazz grew from African American life in New Orleans, drawing on blues, ragtime, marches and spirituals.|louis-armstrong|Can you distinguish a steady pulse from a melody moving freely?|Which city is central to early jazz history?|New Orleans|Seattle|Boston|1|Origins|J
The collective sound|Early 1900s|Early ensembles often improvised several contrasting melodic lines at once, not just a single solo.|jelly-roll-morton|Listen for the cornet, clarinet and trombone answering each other.|What does collective improvisation mean?|Only a drummer plays|Several lines improvise|A written solo repeats|2|New Orleans jazz|S
Riverboats and cities|1910s-1920s|Musicians and their records carried New Orleans styles to Chicago, New York and farther afield.|sidney-bechet|Compare how two horns weave together instead of waiting their turn.|Where did many early jazz players relocate?|Northern cities|Only New Orleans|No cities at all|1|Jazz Age|N
Armstrong changes solos|1920s|Louis Armstrong helped put the inventive individual improvised solo at the center of jazz.|louis-armstrong|Follow one horn through an entire phrase; notice changes in rhythm.|What became central in Armstrong's style?|Electric guitars|Long improvising solos|Studio sampling|2|Jazz Age|N
Blues and jazz|1920s|The blues tradition contributed expressive phrasing and repeated harmonic patterns that jazz players kept reshaping.|louis-armstrong|Notice a phrase bending across the beat, not just the notes played.|Which tradition strongly influenced jazz?|Blues|Techno|Opera alone|1|Improvisation|J
The recorded breakthrough|1920s|Records made improvisations repeatable: listeners everywhere could study distinctive solos at home.|louis-armstrong|Listen for small rhythmic variations over a repeated chorus.|What helped jazz travel beyond clubs?|78 rpm records|Social media|Music videos|1|Jazz Age|S
Larger ensembles|1930s|Big bands combined arranged passages, call-and-response sections and short featured solos.|duke-ellington|Hear how a saxophone section answers a brass section.|What defines a big band texture?|One instrument only|Arranged sections and solos|Only percussion|2|Swing|S
Ellington's colors|1930s-1940s|Duke Ellington arranged music for the particular instrumental voices of his orchestra.|duke-ellington|Focus on the difference between blended horns and a single soloist.|What was special about Ellington's approach?|Identical players|Individual timbres|No arrangements|2|Swing|S
Count Basie's pulse|1930s-1940s|Count Basie's orchestra became known for relaxed rhythmic drive and space between musical gestures.|count-basie|Listen for the rhythm section while the horns leave room for it.|Which section maintains the flowing pulse?|Rhythm section|Only brass|Only vocals|1|Swing|S
Swing as dance music|1930s-1940s|Swing became widely popular as dance music, joining band arrangements to a buoyant rhythmic feel.|benny-goodman|Tap along and notice whether accents feel even or slightly off-center.|What was swing strongly associated with?|Dance|Silent film only|Church organ only|1|Swing|J
Songs become standards|1930s-1950s|Jazz musicians repeatedly reinterpret popular melodies, changing harmony, phrasing and ensemble sound.|ella-fitzgerald|Notice whether the singer delays a syllable before the beat.|What is a jazz standard?|A song reinterpreted often|A single fixed solo|A radio frequency|1|Jazz standard|S
Scat and voice|1930s-1950s|Vocalists treated the voice like an improvising instrument, using timing, phrasing and wordless syllables.|ella-fitzgerald|Notice how wordless syllables mimic horns or percussion.|What is scat singing?|Singing improvised syllables|Singing in unison only|A drum rudiment|1|Improvisation|J
Bebop arrives|1940s|Small combos pursued intricate melodies, fast chord movement and adventurous improvisation.|charlie-parker|Track the quick phrases, then listen for spaces between them.|Which setting helped bebop develop?|Small modern combos|Only marching bands|Digital studios|1|Bebop|S
Parker and Gillespie|1940s|Charlie Parker and Dizzy Gillespie helped shape the melodic and harmonic language of bebop.|dizzy-gillespie|Notice the fast, sharply outlined accents in the horn line.|Who is associated with bebop?|Charlie Parker|Jelly Roll Morton alone|Glenn Miller alone|1|Bebop|S
Monk's surprises|1940s-1950s|Thelonious Monk used striking pauses, unusual melodic shapes and unexpected harmonies.|thelonious-monk|Focus on the silences between piano notes as part of the phrase.|What can silence do in jazz?|Shape the rhythm|Prove a song has ended|Remove all harmony|1|Bebop|S
Cool-jazz colors|Late 1940s|The nonet led by Miles Davis experimented with softer ensemble colors and careful arrangements.|miles-davis|Listen for restrained dynamics rather than speed alone.|Which mood often marks cool jazz?|Restrained and arranged|Only maximum volume|No instrumental color|1|Cool jazz|S
Hard bop roots|1950s|Hard bop drew strongly on blues, gospel and rhythmic intensity while retaining modern harmony.|art-blakey|Hear how the drums push the band while a soloist answers.|Which sources fed hard bop?|Blues and gospel|Synth-pop alone|Gregorian chant alone|1|Hard bop|S
Modes open the space|Late 1950s|Modal jazz uses scales and modes as a framework, giving soloists time to explore a smaller set of harmonies.|miles-davis|Notice a long improvisation above a slowly changing harmony.|What anchors modal improvisation?|Modes and scales|Only rapid key changes|A drum solo alone|1|Modal jazz|S
Free jazz questions rules|1960s|Free jazz broadened possibilities for form, ensemble roles, pulse and harmonic freedom.|ornette-coleman|Listen for musicians responding without a predictable chord loop.|What might free jazz question?|Fixed chord patterns|Instrument tuning only|Electricity use|1|Free jazz|S
Coltrane's search|1960s|John Coltrane explored extended improvisation and a searching, sometimes spiritual musical intensity.|john-coltrane|Follow one saxophone line as its energy and density evolve.|Which instrument is Coltrane best known for?|Saxophone|Accordion|Harp|1|Modal jazz|S
Afro-Cuban dialogues|1940s onward|Dizzy Gillespie and Cuban percussionist Chano Pozo helped bring Afro-Cuban rhythm into modern jazz.|dizzy-gillespie|Hear whether layered percussion patterns change the horn phrasing.|Which element helped connect these traditions?|Afro-Cuban percussion|Silent piano|No rhythm|1|Global jazz|S
Bossa crosses borders|1960s|Brazilian bossa nova entered jazz through recordings linking Brazilian musicians and US jazz players.|stan-getz|Listen for a gentle syncopated pulse under the melody.|Where did bossa nova develop?|Brazil|Norway|Canada|1|Global jazz|B
Electric experiments|Late 1960s-1970s|Fusion combined jazz improvisation with rock rhythms and electric instruments.|herbie-hancock|Compare the pulse of electric bass and drums with acoustic swing.|What equipment expanded fusion?|Electric keyboards|Only bagpipes|No amplified instruments|1|Jazz fusion|S
Jazz keeps changing|1980s onward|Jazz keeps renewing itself through historical study, global collaborations and fresh experimentation.|wynton-marsalis|Compare two jazz stations and note one difference in groove.|What is jazz history?|An ongoing conversation|A closed list of records|One uniform sound|1|Global jazz|S
""".strip().splitlines()

# 30 small "listen for" prompts, each linked to a lesson for further reading.
TIPS = """
Pulse vs melody|Tap the steady beat, then hear which instrument moves freely across it.|0|J
Two horns at once|Listen for overlapping trumpet and clarinet melodies instead of a single lead.|1|S
Call and response|Find an answer from another instrument after a short phrase.|1|J
The walking bass|Follow the bass line as it moves one step per beat through changing chords.|6|S
Behind the beat|Notice a soloist sounding slightly late without losing the underlying pulse.|8|S
The rhythm section|Ignore the solo for ten seconds; focus on bass, drums and chordal support.|8|S
Blues inflection|Listen for bent notes and phrases that feel closer to a sung voice.|4|J
Repeat, then vary|Find a phrase repeated with one small rhythmic or melodic change.|5|S
A chorus returns|Listen for the underlying song form when a new solo begins.|5|S
Brass against reeds|Notice a trumpet or trombone line answered by the saxophones.|6|S
Sudden ensemble hits|Hear a band accenting a phrase together before another solo.|7|S
A piano leaves space|Find a few piano notes that leave room for the other musicians.|9|S
The singer bends time|Listen for syllables that float across the beat.|10|J
Scat as an instrument|Notice a vocal phrase imitating a horn or a drum figure.|11|J
Fast melodic turns|Follow a bebop run, then notice its punctuation and breath.|12|S
The drummer interacts|Hear a short drum accent responding to a soloist.|16|S
Unexpected silence|Listen for a pause that changes the meaning of the next note.|14|S
Soft ensemble colors|Find a passage where several instruments blend without dominating.|15|S
Blues inside hard bop|Hear how gospel-like accents or blues phrases ground complex harmony.|16|S
Fewer chord changes|Listen for a solo exploring one tonal color for several measures.|17|S
Long saxophone line|Hear how a saxophonist builds intensity through repeated ideas.|19|S
Collective freedom|Try to hear which player proposes an idea and who replies.|18|S
Unusual meter feel|Count carefully; do the accents line up with the dance pulse?|18|S
Hand percussion layers|Focus on the relationship of congas to drum kit and bass.|20|B
Bossa's gentle pulse|Hear the understated syncopation without assuming the tune is swing.|21|B
Two rhythmic cultures|Compare a Latin rhythmic pattern with a walking jazz bass.|20|B
Electric tone colors|Listen for keyboards or electric guitar changing the band's texture.|22|S
Groove and improvisation|Keep one groove in mind while a soloist moves around it.|22|S
Remember a discovery|Write down one unexpected instrument or phrase you heard today.|23|J
Compare stations|Notice one difference in the rhythm or ensemble of two jazz broadcasts.|23|S
""".strip().splitlines()


def validate_ascii(s: str, limit: int, *, nonempty: bool = True) -> str:
    if nonempty and not s:
        raise ValueError("Empty required field")
    if not s.isascii() or any(ord(c) < 32 for c in s) or len(s) > limit:
        raise ValueError(f"Invalid or too long ({len(s)}/{limit}): {s!r}")
    return s


def render() -> dict[str, str]:
    """Generate course artifacts in memory; no tracked data files are touched."""
    assert len(LESSONS) == 24 and len(TIPS) == 30
    musician_keys = {row.split("\t", 1)[0] for row in
                     (ROOT / "data/jazz-encyclopedia.tsv").read_text("ascii").splitlines()}
    styles = [0] * 6 + [1] * 6 + [2, 2, 2, 3, 4, 5, 6, 5, 7, 8, 9, 10]
    out = []
    for i, line in enumerate(LESSONS):
        parts = line.split("|")
        assert len(parts) == 12, (i, len(parts))
        title, era, story, artist, listen, question, a, b, c, answer, topic, source = parts
        if artist not in musician_keys:
            raise ValueError(f"Course artist missing from encyclopedia: {i} {artist}")
        if source not in SOURCES or answer not in ("1", "2", "3"):
            raise ValueError(f"Bad source/answer {i}")
        row = [
            str(i), str(i // 6), str(styles[i]),
            validate_ascii(title, 36), validate_ascii(era, 28),
            validate_ascii(story, 150), validate_ascii(artist, 64),
            validate_ascii(listen, 130), validate_ascii(question, 104),
            validate_ascii(a, 42), validate_ascii(b, 42), validate_ascii(c, 42),
            answer, validate_ascii(topic, 42), SOURCES[source],
        ]
        assert len("\t".join(row)) < 800
        out.append("\t".join(row))
    tips = []
    for i, line in enumerate(TIPS):
        title, prompt, stage, source = line.split("|")
        assert stage.isdigit() and int(stage) < 24 and source in SOURCES
        tips.append("\t".join([str(i), str(stage),
                                validate_ascii(title, 36),
                                validate_ascii(prompt, 140), SOURCES[source]]))
    provenance = (
        "# Jazz Journey — historical and pedagogical provenance\n\n"
        "Original educational summaries and quiz questions, not transcribed content.\n"
        "The `jazz-journey.tsv` and `jazz-listen.tsv` source column records an "
        "attribution trail, not a song link or permission to redistribute recordings.\n"
        "Recorded jazz history and stylistic periods overlap; do not assume a "
        "broadcast currently plays the genre or the featured musician.\n\n"
        "Sources consulted (all HTTPS):\n" +
        "".join(f"- {u}\n" for u in SOURCES.values()) +
        "\nThese files are optional offline SD data. Course progress and diary "
        "are private files created only when the user explicitly saves.\n"
    )
    return {
        "jazz-journey.tsv": "\n".join(out) + "\n",
        "jazz-listen.tsv": "\n".join(tips) + "\n",
        "JAZZ-JOURNEY-PROVENANCE.md": provenance,
    }


def build() -> None:
    """Explicit generator command: write only our three own data artifacts."""
    for name, content in render().items():
        target = ROOT / "data" / name
        target.write_text(content, encoding="ascii" if name.endswith(".tsv") else "utf-8",
                          newline="\n")
        print(name, len(content.splitlines()), "rows/lines", target.stat().st_size, "bytes")
    print("DATA_GENERATION_PASS")


if __name__ == "__main__":
    build()

# Circuit research preset phrases

These eight original MIDI files are the fixed demonstration inputs for eight of the fourteen native `ceres::presetNames()` research presets, each named by its program number: Init (01), String 1 (02), Brass 1 (04), Flute (06), Electric Piano (07), Clavichord (08), Guitar 1 (10) and Funky 1 (12). They are not recorded performances or recovered Yamaha preset MIDI, and the preset names inherited from the research table do not establish calibrated Yamaha voices.

Each phrase is written for its preset's role, at 120 BPM with 960 ticks/quarter, channel 1 and velocity 100, using note events only:

- **Init**: an A minor bass line in eighths, then a held four-voice A minor chord.
- **String 1**: four slow pad chords (Am, F, C, Em7), each released just before the next.
- **Brass 1**: fanfare stabs on C and F, then G and a long C.
- **Flute**: a monophonic G major melody with small breaths between notes.
- **Electric Piano**: ii–V–I–vi comping in C, a bass note under a three-note voicing, two hits a bar.
- **Clavichord**: a staccato E minor riff in sixteenths with double stops, four times, then an Em7 chord.
- **Guitar 1**: C, Am, F and G arpeggiated, each note ringing to the next chord, then a strummed C.
- **Funky 1**: a syncopated G minor line in sixteenths, two bars twice, then a held fifth.

No phrase holds more than the CS-50's four voices at once, and each ends by 9.25 s, so the shared renderer's fixed 2.5 s tail keeps every demo within 8–12 s. That fixed tail is not a claim that every slow internal capacitor has fully settled. No controller changes, external audio, gain matching, normalization, tuning or preset-specific adjustment is included. Reproduce/check the source bytes with:

```sh
python3 Tools/make_research_demo_midi.py
# Deliberate source replacement only:
python3 Tools/make_research_demo_midi.py --write
```

Subsequent changes to a phrase should remain deliberate source changes in their own commit; do not regenerate them merely because the DSP changes.

# Circuit research preset phrases

These14 original MIDI files are the fixed demonstration inputs for the existing native `ceres::presetNames()` table. They are not recorded performances or recovered Yamaha preset MIDI, and do not imply that the separate JUCE plug-in exposes these presets.

All use the same phrase at120BPM with960ticks/quarter, channel1 and velocity100: C3 from0.25–1.25s, E3 from1.5–2.5s, G3 from2.75–3.75s, C4 from4–5.5s, then C3/G3/C4/E4 together from6–9.5s. MIDI note numbers are48/52/55/60/64; octave names here follow MIDI60=C4. Each file has16 channel events and differs only in its track-name metadata. The unchanged shared renderer adds2.5s after the last note-off, producing12s demos without triggering its20s truncation fade. That fixed tail is not a claim that every slow internal capacitor has fully settled.

No controller changes, external audio, gain matching, normalization, tuning or preset-specific adjustment is included. The single notes expose attacks and releases; the final chord exercises four voices. Reproduce/check the source bytes with:

```sh
python3 Tools/make_research_demo_midi.py
# Deliberate source replacement only:
python3 Tools/make_research_demo_midi.py --write
```

Source files are committed separately from the demo-tool implementation. Subsequent changes to the phrase should remain deliberate source changes in their own commit; do not regenerate them merely because the DSP changes.

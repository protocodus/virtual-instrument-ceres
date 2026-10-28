#!/usr/bin/env python3
"""Write the committed research-preset demo phrases, Docs/audio/midi (G3, G3a).

Eight of the fourteen research presets, each showing a different side of the
circuit, get an original phrase written for its role: the Init voice as a
bass line, String 1 as a pad, Brass 1 as stabs and a swell, Flute as a
melody, Electric Piano comping, Clavichord as a staccato riff, Guitar 1
arpeggiated and Funky 1 as a syncopated line. None is recovered hardware MIDI
or a recorded performance, and the preset names inherited from the research
table do not establish calibrated Yamaha voices.
Phrases stay within the CS-50's four voices, use note events only at
velocity 100 (the research presets ignore velocity), at 120 bpm and 960 ticks
per quarter on channel 1, and their last event falls at 5.5-9.25 s so the
shared renderer's 2.5 s tail keeps every demo within 8-12 s. That fixed tail
is not a claim that every slow internal capacitor has fully settled. No
controller changes, external audio, gain matching, normalization, tuning or
preset-specific adjustment is included, and no sound is rendered here.

    python3 Tools/write_preset_midi.py           # write Docs/audio/midi
    python3 Tools/write_preset_midi.py --check   # the committed files match

Change a phrase only on purpose and commit it on its own; never regenerate
the sources because the DSP changed.
"""
import argparse
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
TICKS = 960  # per quarter note, at the 120 bpm tempo below


def init():
    """A minor bass line in eighths, then a held four-voice chord."""
    line = [45, 45, 48, 45, 50, 45, 52, 55, 45, 45, 48, 45, 43, 45, 52, 50]
    notes = [(0.5 + 0.5 * i, 0.5 + 0.5 * i + 0.4, key) for i, key in enumerate(line)]
    return notes + [(9, 16, key) for key in (45, 52, 57, 60)]


def string1():
    """Four slow pad chords, each released just before the next."""
    chords = [(57, 60, 64, 69), (53, 57, 60, 64), (55, 60, 64, 67), (52, 59, 64, 67)]
    return [(0.5 + 4 * i, 0.5 + 4 * i + (3.9 if i < 3 else 5), key)
            for i, chord in enumerate(chords) for key in chord]


def brass1():
    """Fanfare stabs on C and F, then G and a long swelling C."""
    notes = []
    for start, chord in ((0.5, (48, 55, 60, 64)), (4.5, (53, 57, 60, 65))):
        for offset, length in ((0, 0.5), (1, 0.4), (1.5, 0.4), (2, 1.75)):
            notes += [(start + offset, start + offset + length, key) for key in chord]
    notes += [(8.5, 10.25, key) for key in (55, 59, 62, 67)]
    return notes + [(10.5, 15.5, key) for key in (48, 55, 60, 64)]


def flute():
    """A monophonic G major melody with small breaths between notes."""
    melody = [(74, 1), (79, 1), (78, 0.5), (76, 0.5), (74, 1), (71, 1), (72, 0.5), (74, 0.5),
              (76, 1), (74, 2), (71, 1), (69, 1), (67, 3)]
    notes, beat = [], 0.5
    for key, length in melody:
        notes.append((beat, beat + length - 0.08, key))
        beat += length
    return notes


def electric_piano():
    """ii-V-I-vi comping: a bass note under a three-note voicing, two hits a bar."""
    chords = [(50, 65, 69, 72), (43, 65, 69, 71), (48, 64, 67, 71), (45, 64, 67, 72)]
    notes = []
    for bar, chord in enumerate(chords):
        start = 0.5 + 4 * bar
        notes += [(start, start + 1.4, key) for key in chord]
        last = bar == len(chords) - 1
        notes += [(start + 1.5, start + (4.5 if last else 3.9), key) for key in chord]
    return notes


def clavichord():
    """A staccato E minor riff in sixteenths with double stops, twice, then a chord."""
    riff = [(0, (52,)), (2, (52,)), (3, (55,)), (4, (57,)), (6, (55,)), (7, (52,)),
            (8, (50,)), (9, (52,)), (12, (55, 59)), (14, (57, 60)), (15, (55, 59))]
    notes = []
    for repeat in range(4):
        for step, keys in riff:
            start = 0.5 + 4 * repeat + step / 4
            notes += [(start, start + 0.15, key) for key in keys]
    return notes + [(16.5, 18, key) for key in (52, 59, 64, 67)]


def guitar1():
    """C, Am, F, G arpeggiated, each note ringing to the next chord, then a strum."""
    chords = [(48, 55, 60, 64), (45, 52, 57, 60), (41, 48, 53, 57), (43, 50, 55, 59)]
    notes = [(0.5 + 4 * bar + 0.5 * i, 4.5 + 4 * bar, key)
             for bar, chord in enumerate(chords) for i, key in enumerate(chord)]
    return notes + [(16.5 + 0.05 * i, 18.5, key) for i, key in enumerate((48, 55, 60, 64))]


def funky1():
    """A syncopated G minor line in sixteenths, two bars twice, then a held fifth."""
    bars = [(0, 2, 43), (3, 1, 55), (4, 1, 53), (6, 1, 55), (8, 2, 46), (10, 1, 48),
            (12, 2, 50), (14, 1, 53), (15, 1, 55),
            (16, 2, 43), (19, 1, 55), (20, 1, 58), (22, 1, 57), (24, 1, 55), (26, 1, 53),
            (28, 2, 50), (30, 1, 48), (31, 1, 46)]
    notes = []
    for repeat in range(2):
        for step, length, key in bars:
            start = 0.5 + 8 * repeat + step / 4
            notes.append((start, start + 0.7 * length / 4, key))
    return notes + [(16.5, 18, key) for key in (43, 50, 55)]


# Program number (the file's <NN>) and research-table key of each demo.
DEMOS = [(1, 'init', init), (2, 'string1', string1), (4, 'brass1', brass1), (6, 'flute', flute),
         (7, 'electric_piano', electric_piano), (8, 'clavichord', clavichord),
         (10, 'guitar1', guitar1), (12, 'funky1', funky1)]


def vlq(number):
    result = [number & 127]
    number >>= 7
    while number:
        result.insert(0, 128 | (number & 127))
        number >>= 7
    return bytes(result)


def midi(name, notes):
    events = []
    for start, end, key in notes:
        events += [(round(start * TICKS), 1, bytes([0x90, key, 100])),
                   (round(end * TICKS), 0, bytes([0x80, key, 0]))]
    events.sort(key=lambda item: item[:2])  # a Note Off before a Note On at the same tick
    label = ('Ceres circuit research: ' + name).encode('ascii')
    track = b'\x00\xff\x03' + vlq(len(label)) + label + b'\x00\xff\x51\x03\x07\xa1\x20'
    prior = 0
    for tick, _, message in events:
        track += vlq(tick - prior) + message
        prior = tick
    track += b'\x00\xff\x2f\x00'
    return b'MThd' + struct.pack('>IHHH', 6, 0, 1, TICKS) + b'MTrk' + struct.pack('>I', len(track)) + track


def check(notes):
    """At most four keys down at once, and the last Note Off at 5.5-9.25 s."""
    edges = sorted([(start, 1) for start, _, _ in notes] + [(end, -1) for _, end, _ in notes])
    held = 0
    for _, step in edges:
        held += step
        assert held <= 4, 'more than four voices'
    assert 5.5 <= max(end for _, end, _ in notes) * 0.5 <= 9.25, 'phrase ends outside 5.5-9.25 s'


def program_name(key):
    """The plug-in's program name, as ceres_plugin::programName in Source/PluginControls.h."""
    name, word_start = '', True
    for i, c in enumerate(key):
        if c in '_-':
            name, word_start = name + ' ', True
            continue
        if c.isdigit() and i > 0 and not key[i - 1].isdigit():
            name += ' '
        name, word_start = name + (c.upper() if word_start else c), False
    return name


def slug(name):
    # The renderer's slug rule: lowercase alphanumerics, other runs as one '-'.
    return re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')


def files():
    result = {}
    for number, key, phrase in DEMOS:
        notes = phrase()
        check(notes)
        result[f'{number:02}-{slug(program_name(key))}.mid'] = midi(key, notes)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--check', action='store_true', help='fail unless the committed files match')
    args = ap.parse_args()
    folder = ROOT / 'Docs/audio/midi'
    expected = files()
    found = {path.name: path.read_bytes() for path in folder.glob('*.mid')}
    if args.check:
        stale = sorted(set(found) ^ set(expected) | {n for n in expected if found.get(n) != expected[n]})
        if stale:
            sys.exit(f'Docs/audio/midi differs from {Path(__file__).name} in: {", ".join(stale)}')
        print(f'PASS {len(expected)} authored phrases match, at most four voices, ending by 9.25 s')
        return
    folder.mkdir(parents=True, exist_ok=True)
    for name in set(found) - set(expected):
        (folder / name).unlink()
    for name, data in expected.items():
        (folder / name).write_bytes(data)
        print(name)


if __name__ == '__main__':
    main()

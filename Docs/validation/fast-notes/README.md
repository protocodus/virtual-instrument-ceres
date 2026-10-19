# Rapid notes and envelope charge

This follow-up validates the existing `f46e829` MIDI corrections against the
older `2ee6cf2` circuit engine used by the installed Rack b11. It adds focused
regression coverage and a before/after probe. It does not change the DSP,
waveform phase, envelope law, ranges, presets or default sound.

## Reproduced defects in the installed baseline

The same [probe](../../../Tools/measure_fast_notes.cpp) compiles against both
revisions using the original four-board API. Four MIDI 60 notes at velocity
0.2 sound for 20 ms at 48 kHz and 4x. A fifth note, MIDI 72 at velocity 0.9,
reuses an audible board. With filter-envelope and keyboard tracking removed
from the diagnostic, the production HP frequency reveals the consumed velocity
CV independently of the gain path.

The b11 engine jumps immediately to 0.9. The current engine reaches
0.20291059870842287 after the first host sample, versus the independent 5 ms
RC solution 0.2029105987084231. Oscillator phase and both envelope capacitor
voltages remain unchanged at assignment in both revisions. The discontinuity
was the newly assigned velocity, not an envelope reset.

The eventual note-off for the stolen first MIDI 60 press incorrectly releases
one of the three surviving MIDI 60 owners in b11. The current physical-press
FIFO consumes that old press without releasing its newer replacement: all
three surviving owners remain held. `--require-fixed` returns 2 for the
unmodified baseline and 0 for the current engine. Those are deliberately
retained before-failing and after-passing witnesses, rather than a new policy
change made just to produce a different sound.

## Envelope and burst checks

[FastNoteEnvelopeTests.cpp](../../../Tests/FastNoteEnvelopeTests.cpp) adds:

- 54 capacitor cases with 162 independent closed-form comparison points:
  8/48/96 kHz host rates, 1x/2x/4x, 0.1 ms/1 ms/1 s attacks, amplitude and
  filter envelopes. A one-sample attack, one-sample release and immediate
  re-gate retain the actual charge and selected RC time.
- 72 rapid velocity/re-gate cases: same and different pitches, elapsed-time
  RC checks across the three rates and qualities, with and without ring modulation,
  long and shortest attacks/decays. Thirty-two velocity retargets arrive every
  13 samples; consumed CV follows the continuous five-millisecond solution.
  Stale note-offs cannot release the replacement.
- 432 burst cases: 1/4/16 voices, all qualities, repeated and changing notes,
  single notes and four-note chords, equal and varying velocities, ring on/off,
  and three envelope configurations. These include the edited Bloom's long
  times, 1 ms attack/10 ms decay-release, and the exposed 0.1 ms extremes.
  Free-board assignment, audible released-board reuse and over-capacity held
  stealing preserve both EG voltages and oscillator phase at every event.
  Rendered audio and capacitor bounds remain finite; final physical-key and
  held-owner ledgers clear correctly.

The focused Release test passes. ASan/UBSan also passes all three selected
suites: this new suite, note reuse and capacitor continuity. The machine-readable
[report](validation.json) binds the source and local evidence.

## What a very fast envelope means

At 48 kHz, an exposed 0.1 ms attack reaches approximately 0.623851 of normalized
amplitude-envelope voltage in its first sample. A 1 ms attack reaches 0.092587.
Both are the specified RC solution, not a reset or unintended jump. Extremely
short attacks, decays and releases can therefore make an intentionally sharp
transient with substantial high-frequency content. This follow-up preserves
those controls rather than imposing a hidden minimum fade. Re-triggering starts
from retained capacitor charge; it does not restart from zero.

Different-pitch board reuse still changes keyboard CV and oscillator slope as
requested. Its normal pitch/filter transient is not classified as a defect
merely because it differs from a no-note control. No pitch glide, arbitrary
phase randomization, voice crossfade, output limiter or global smoothing was
added. These checks establish specific corrected defects and charge continuity;
they do not establish that every audible glitch has been reproduced, that a
particular host has no CPU underruns, or that 1x eliminates aliasing from every
extreme modulation setting. Listening and host testing must use the rebuilt
candidate rather than the older installed b11.

## Reproduce

Configure a native build with `CERES_BUILD_PLUGIN=OFF`, then run
`ctest -R Ceres.fast_note_envelope_contracts --output-on-failure`. For the
portable installed-baseline witness:

```sh
c++ -std=c++17 -O2 -I Source/DSP Tools/measure_fast_notes.cpp Source/DSP/ceres.cpp -o /tmp/ceres-fast-note-probe
/tmp/ceres-fast-note-probe --require-fixed
```

Compile the identical probe with `ceres.cpp` and `ceres.hpp` extracted from
`2ee6cf2` in a separate directory to reproduce its expected exit status 2.
The probe generates only numeric diagnostics. No recording, sample asset,
manual PDF or proprietary Rack SDK content is added to this report.

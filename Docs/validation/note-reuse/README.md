# Audible board reuse and repeated-note ownership

Ceres 0.2.1 corrects two MIDI adaptation defects without resetting the modeled
oscillator, envelope, filter or coupling-capacitor histories.

An already audible board previously consumed a newly assigned velocity in one
step. This stepped both its selected expression CV and its velocity-dependent
VCA gain. The board now retains its latched velocity target and approaches it
with the existing 5 ms controller RC time constant. An idle or zero-envelope
board precharges immediately, preserving fresh-note response. The consumed
value feeds velocity-selected volume/filter/vibrato and the independent legacy
velocity gain. Keyboard pitch is not slewed. This is a modern digital MIDI
policy, not a newly identified CS-50 component or hardware calibration.

Each physical press also receives a bounded per-pitch FIFO ticket. A note-off
consumes the oldest physical ticket even if its board was stolen. Only the
matching surviving owner is released. The former implementation instead
released the oldest *currently allocated* same-pitch board, incorrectly cutting
off a newer press. No dynamically allocated event queue is used. Counts are
bounded at `UINT32_MAX`; ticket increments wrap as unsigned values and are
matched only by equality. Reset, All Notes Off and All Sound Off clear the
physical ticket ledger while releasing their corresponding gates.

## Protocol and results

The user's screenshot contained an edited Bloom patch: notably 100% saw,
54% pulse, 84% sine and 73% ring mix with an eight-octave ring sweep. It is not
the shipped Bloom patch. The probe reconstructs the displayed rounded values;
the exact saved user patch and performance MIDI were not supplied.

[The probe](../../../Tools/measure_note_reuse.cpp) runs the canonical production
`Synth`, with seed 50, 48 kHz, four voices and 1x/4x quality. Four unison MIDI 60
notes at velocity 0.2 sound for one second. A fifth assignment either repeats
MIDI 60 or changes to MIDI 84 at velocity 1. A companion control receives no
fifth assignment. Released-board cases release the oldest note and wait 50 ms
before reuse. Each case retains 200 ms after the assignment. No-ring,
equal-velocity and zero-expression-depth ablations isolate the mechanism.

There are 40 event/control pairs per engine revision. The probe's `stock` label
means a factory-waveform/PWM ablation that still inherits the screenshot's
minus-ten-cent tuning. It must not be presented as exact factory Bloom. A
separate eight-pair baseline continuation corrects that tuning to zero; its
source/plan/report are independently bound in [validation.json](validation.json).
It is not substituted for any original witness.

For the screenshot's same-pitch held-board case, the first affected 1x output
sample's event-minus-control difference falls from 0.0116021261 to 0.0000480860
full scale, a 47.65 dB reduction. The released-board result falls from
0.0135688633 to 0.0000577793, a 47.42 dB reduction. Removing ring modulation
retains the original step, demonstrating that this mechanism does not require
the ring circuit. Equal-velocity and zero-expression cases preserve all 16
complete event outputs byte for byte; all 40 pre-assignment prefixes also
match between engine revisions.

The largest event-minus-control sample-derivative difference in the following
240 samples decreases from 0.0116021 to 0.00486753 at 1x and from 0.0118890 to
0.00563766 at 4x for the held screenshot case. These windows include intended
gain and timbre changes during the RC transition, so they are not a measurement
of residual click audibility. At 4x the causal FIR begins responding before its
31-sample group-delay center; its tiny first differing tap is not a useful step
metric. Different-pitch witnesses include normal changed waveforms and filter
transients, and are not treated as evidence that those differences are defects.

The final 0.2.1 Release suite passes 21/21 tests. Five targeted ASan/UBSan suites
also pass. Configured native/embedded rendering matches exactly for 184,320
samples across five sample rates, three voice counts and three quality factors,
including repeated-note overflow, stale note-offs and audible velocity
retargets. The embedded allocation trap observes zero post-construction
allocations through the new paths. Focused production-CV checks verify the
closed-form five-millisecond law at three sample rates and all quality factors,
with fresh/zero/idle gates, retargeted transitions, release, pedal, panic and
reset. Independent checks agree within 8.66e-15 for the consumed velocity CV.

The unmodified 82a1424 baseline fails the new first-sample velocity regression
(actual 0.9; expected 0.21728306158 at 8 kHz), and independently reproduced stale
note-offs release a newer surviving same-pitch board. The current engine passes.
These witnesses establish specific implementation defects and their correction;
they do not establish that every sound the user heard is reproduced or fixed,
nor that an arbitrary host is free of CPU underruns.

## Reproduce

From the repository root, compile the probe against each engine revision and
give it a separate existing output directory:

```sh
c++ -std=c++17 -O2 -I Source/DSP Tools/measure_note_reuse.cpp Source/DSP/ceres.cpp -o /tmp/ceres-note-reuse-probe
mkdir -p /tmp/ceres-note-reuse-current
/tmp/ceres-note-reuse-probe /tmp/ceres-note-reuse-current
```

For the baseline, compile the same probe against `ceres.cpp` and `ceres.hpp`
from commit `82a1424e53e29252d314b40d16c88292644a3003`, in a separate checkout.
The files are little-endian IEEE float32 mono sample payloads, not synthesis
assets. Subtract each `control` payload from its `event` payload; at 1x the first
affected sample follows the assignment by 31 samples. The derivative metric is
the maximum absolute difference between their adjacent-sample differences over
the 240 samples beginning at that point.

Plans, before/after payloads, binaries and all failure logs remain in the local
experiment archive. The public report includes their content hashes and logical
roles; it does not imply that those local files are shipped or available from
this checkout. No recordings, audio payloads, proprietary SDK files or manual
PDFs are added to the repository. The earlier test-expectation unsigned-negation
error is retained as a failed attempt; it was corrected in the test expression,
and the final corrected test was separately compiled against the unchanged
baseline to retain an unambiguous expected failure.

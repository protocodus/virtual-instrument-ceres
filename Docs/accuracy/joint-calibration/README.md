# Joint calibration surrogate qualification

The later fit and20 native validation renders are complete. The candidate was
rejected; see [RESULT.md](RESULT.md). No production mapping was promoted.

Status: all 32 retained DC-cutoff-change conditions pass the frozen
[plan](plan.json). [Attempt 001](attempt-001/report.json) retains both authorized
control-only traces and the complete predictions/errors. Exact deterministic
recomputation failed its final report-equality assertion. The preserved
[diagnostic](recompute-diagnostic-001/README.md) isolates 989 last-bit floating
differences in Flute analytical predictions and their derived statistics, with a
maximum difference of 1.4211e-14 dB. Native measurements, Brass predictions and
every gate decision match exactly. [The exact-check failure](attempt-001/check-failure-001.json)
remains preserved; no comparison tolerance or passing report was changed. No
recording-linked audio render or parameter fit was performed by this helper.

This experiment asks whether a native-baseline-plus-analytical-change surrogate
can predict **retained native DC-cutoff changes** closely enough to support a
later, separately declared conditional calibration. It does not identify a
physical hardware bias, chip calibration, or factory preset map. It does not
use source recordings to synthesize audio.

The helper is [joint_calibration.py](../../../Tools/accuracy/joint_calibration.py).
Its only executable commands are `oracles`, `qualify`, and `check`; it contains
no parameter optimizer. The only eligible clips are raw high Flute and Brass
history20. Qualification evaluates all 12 loaded and four stiff conditions per
clip against their retained frozen controls: 32 changed cases, two controls.
No lower-Flute or history10 measurements are accessed.

## Measurement model

[control_trace.cpp](control_trace.cpp) steps the exact archived native engine
with each frozen control patch/event schedule and seed. It discards every audio
return value and exports only control states at 16 kHz: all four voices' note,
key, gate, envelopes, oscillator phase/frequency, and HP/LP frequencies, plus
shared VCA gain, resonance and SUB state. This operation requires explicit
permission in the frozen declaration. It is not a new audio render and produces
no WAV. All compiled inputs, compiler identity, binary, logs and traces are bound
by SHA-256.

For qualification, only the two neutral cutoff intercepts change. Key CV, the
existing envelope scales, envelope states, source tuning, Q and modulation stay
frozen. This matches the retained DC experiment; it is **not** a full time-varying
network calibration.

The analytical source uses the exact finite-reset ascending saw Fourier series
and cubic BLAMP response. It applies the small-signal transfer of the production
TPT filter's physical input/feedback nodes, including the three-branch source
average. Brass's direct sine is a cosine at oscillator reset and is added as a
complex amplitude after the filtered branch, before CP4. Its interference with
the filtered saw fundamental must not be replaced by a sum of powers. The
remaining coupling capacitors, VCA schedule and decimator magnitude are included.

Harmonic powers are integrated over every sample of each full analysis window,
using Hann-squared weights. Flute RMS instead uses rectangular weights and up to
64 harmonics. The 8 kHz quadrature is compared with the 16 kHz trace grid. The
known 31-host-sample FIR delay is retained. Distinct resolved voice ladders add
in power; coincident active notes and note reassignment inside a window fail the
qualification implementation rather than being silently approximated.

The estimate for a changed case is:

`retained native baseline + analytical(changed) - analytical(baseline)`

The same frozen measurement kernels extract ratio and level changes from the
retained WAVs. No recording target enters the error. The original DC declaration
contains historical score fields, but the helper never reads those fields or its
aggregate report. The Brass source JSON supplies only frozen window centers and
eligibility masks. Candidate-dependent exclusions are forbidden.

## Frozen gate

Every one of the 32 changed cases must pass:

| Quantity | Limit |
|---|---:|
| Ratio-delta error RMS | 1 dB |
| Ratio-delta error 95th percentile absolute error | 2 dB |
| Each Brass cell's absolute **signed mean** delta error | 0.5 dB |
| Level-delta error RMS | 1 dB |
| Level-delta error 95th percentile absolute error | 2 dB |
| Coarse/fine analytical-delta maximum difference | 0.1 dB |

Observed worst values across the declared cases:

| Quantity | High Flute | Brass history20 |
|---|---:|---:|
| Ratio-delta error RMS | 0.061016 dB | 0.043100 dB |
| Ratio-delta error 95th percentile absolute error | 0.139522 dB | 0.090567 dB |
| Largest individual ratio-delta error | 0.186662 dB | 0.246366 dB |
| Level-delta error RMS | 0.009626 dB | 0.020575 dB |
| Level-delta error 95th percentile absolute error | 0.019739 dB | 0.015353 dB |

The largest absolute Brass cell signed bias is 0.100474 dB; the largest
coarse/fine quadrature difference is 0.000008313 dB. Each table entry is a
separate maximum over all 16 changes for that clip, so they need not belong to
the same condition.

The report SHA-256 is
`5ae567e7dbdbce2ac63569f367c74e2867b0f432186313391cb3e740b80ae176`;
the frozen plan SHA-256 is
`9a13c10f5c6a65d65cf20cd061e133c1d4a88a6eb5532895bf2279c5a06a54ca`.

The cell gate measures signed bias, not mean absolute error; the separate RMS and
95th percentile gates limit cancellation. Levels are total Flute RMS or each
Brass note's H1 power. All frozen eligible ratio frames remain in the test.

Independent numerical checks compare the closed-form filter with a five-node
complex solve, compare source coefficients with sampled exact oscillator/BLAMP
functions, verify the direct sine's phase, verify the broad-pass per-wave gain,
and check zero-change and scalar-gain ratio invariance. These are model identities
and implementation checks, not measurements of a CS-50.

## Limits and preservation

Changing capacitor memory, finite drive, output saturation, cross-ladder leakage
and finite FFT bands are not represented exactly by the quasistationary power
model. Native-delta qualification must expose their effect. Passing DC-only
qualification would not establish the accuracy of later dynamic network changes.
Failing any case stops the proposed fit; it does not justify relaxing tolerances
or dropping a preset after viewing results.

[preflight-001](preflight-001/review.json) preserves the first reviewed code. Its
filtered source path missed the native `/3` mixer factor; that error was found
before any native trace or qualification. It is now corrected and guarded by an
independent broad-pass oracle. The original numerical checks had passed because
they did not cover source-mixer gain. Their incompleteness is retained in the
preflight record.

Execution creates a new attempt directory and refuses to overwrite evidence.
Failures preserve traceback and available inputs. `check` repeats only analytical
and measurement calculations from retained traces/WAVs, verifies their producer
identity and all bound inputs, and compares the complete report exactly.

The frozen helper does not calculate additional source-threshold weakness flags
or a separate count of numerical floor crossings. All declared valid frames and
their errors remain in the report; no weak-native frame was excluded. The gate
therefore qualifies native ratio/level **changes**, not source-relative audibility
or weakness. Those source diagnostics must remain separate in any later native
candidate evaluation.

From the repository root:

```sh
python3 Tools/accuracy/joint_calibration.py oracles
# Only after the frozen controller declaration authorizes control stepping:
python3 Tools/accuracy/joint_calibration.py qualify --attempt attempt-001
python3 Tools/accuracy/joint_calibration.py check --attempt attempt-001
```

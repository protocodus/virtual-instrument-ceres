# CS-50 waveform mixing: public recording check

The former equal-amplitude saw/pulse mixture cancels too strongly at 50% pulse
width. A one-sign pulse correction predicts both the harmonic balance and level
of a public hardware demonstration substantially better, including a second
pitch whose gain was held out. The actual Ceres 0.2.2 renderer reproduces all six
temporary-candidate WAVs byte for byte. **This is a conditional correction of
relative audio polarity, not a complete phase or waveform calibration.**

The [source plan](plan.json) and [additional-source declaration](additional-source-plan.json)
were written before inspecting the new recording. The sign hypothesis was then
formed from that recording; these are development comparisons, not blind scores.
No reference samples are loaded by Ceres or included in the repository.

## Public evidence and qualification

The primary source is Marc Doty's [AutomaticGainsay oscillator demonstration](https://www.youtube.com/watch?v=d2u1QrIYvnw),
published on 7 January 2013. The creator identifies the isolated saw and square,
enables both around 142 seconds, and demonstrates the three combinations again
around 152–163 seconds. Inspected closeups put PW at its labeled 50% endpoint
and PWM depth at zero; the filter bank is not manipulated in this section.

The creator's retrieved description does **not** establish a dry recording,
unchanged capture gain, exact envelopes, pressure or electrically calibrated
panel values. Camera edits and separate performances remain potential confounds.
The comparison therefore assumes a common capture gain and approximately steady
VCA level across the three takes. It cannot establish absolute output gain.
A sine contribution is not calibrated independently. The near-saw harmonic
balance of the isolated-saw segment constrains, but does not prove, its absence.

Other leads do not improve that qualification. Andy Whitmore's creator-labeled
dry manual section at 327–391 seconds contains changing notes, chords and live
controls. The archived PTK page labels its separate saw/square recordings dry,
but explicitly places its "Waveforms" clip among examples using effects.
Synth4ever discloses stereo chorus. Details and media hashes are retained in
[reference.json](reference.json); copyrighted recordings stay in local diagnostics.

## Source measurements

A frozen 200 ms grid covers saw 50.3–61.7 s, square 63.3–82.7 s, and both
143.3–149.7 s. Of 189 windows, 41 pass all declared isolation checks: raw RMS at
least 0.03, first/second-half RMS movement at most 0.75 dB, and at least 98% of
energy explained by a single fundamental and its first 16 harmonics plus DC.
These conditions reject speech, changing notes and chords; they do not prove
complete settings or a dry capture.

| Source | Retained windows | H2 relative to H1 |
|---|---:|---:|
| Saw | 13 | −6.409 to −6.047 dB |
| Square | 13 | −25.747 to −23.741 dB |
| Both | 15 | −16.265 to −15.297 dB |

The 15 both-on windows cover seven notes. Their consistently reduced even/odd
harmonic ratio differs from the former Ceres mixture. Shared-pitch raw level
checks also distinguish a relative sign error from simply doubling pulse gain:

| Note | Hardware square/saw RMS | Hardware square/saw H1 | Hardware both/saw RMS |
|---|---:|---:|---:|
| A3, MIDI 57 | 1.812 | 2.049 | 2.467 |
| D4, MIDI 62 | 1.782 | 2.032 | 2.351 |

Equal-amplitude saw and square predict approximately twice the fundamental
amplitude for square alone. Doubling pulse amplitude instead would predict
approximately four times the saw fundamental, contrary to these checks under
the common-gain assumption. All 189 windows, exclusions and phase measurements
remain in [source-report.json](source-report.json).

## Production comparison

The frozen contrast contains 12 renders: two engines × A3/D4 × saw/square/both.
The baseline is canonical DSP revision `f46e829bce46bea8e8400c58ce549457cd6a611e`.
The temporary candidate changes only the sign of the returned pulse waveform.
PW is 50%, PWM/sine/noise and expression are off, HP/LP are 30/6000 Hz with zero
resonance, filter envelope motion is zero and amp sustain is one. Both engines
use four voices, 4× processing, 48 kHz and seed 50. Those engineering settings
are explicit diagnostic conditions, not recovered electrical hardware controls.

One master tuning, +8.5803 cents, comes from the isolated D4 saw frequency and
is held across every render. One gain, 2.9803704062, is selected from the D4 saw
raw RMS and is held across both engines, all wave combinations and A3. No
per-wave normalization, filter fitting, EQ, time stretch, branch phase rotation
or surrogate waveform is used. Whole-wave relative harmonic phase is measured
as `phi[h] - h * phi[1]`, preserving the phase relations within each mixture.

| Both-on comparison | Hardware | Former Ceres | Pulse-sign candidate / Ceres 0.2.2 |
|---|---:|---:|---:|
| A3 RMS / same-note saw RMS | 2.467 | 0.892 | 2.361 |
| D4 RMS / same-note saw RMS | 2.351 | 0.887 | 2.360 |
| A3 H2/H1 | −16.265 dB | −6.007 dB | −15.620 dB |
| D4 H2/H1 | −16.148 dB | −6.057 dB | −15.670 dB |
| A3 held-gain level error | — | −8.739 dB | −0.291 dB |
| D4 held-gain level error | — | −8.467 dB | +0.033 dB |

Across the two mixture notes, raw-level MAE falls **8.603 → 0.162 dB**
(RMSE 8.604 → 0.207 dB), and H2/H1 MAE falls **10.175 → 0.561 dB**.
Solo square/saw RMS is unchanged at 1.743/1.746 for A3/D4, so this improvement
was not obtained by changing relative branch levels. The six actual production
WAVs match the six candidate WAVs exactly, including attack and output tail;
[production-binding.json](production-binding.json) binds their source/binary hashes.

The source screen covers 41 windows across ten notes. The frozen model contrast
covers **all ten retained A3/D4 windows**, including six square-only windows;
31 other windows remain source-only corroboration. The two both-on notes each
have one retained window, so these figures do not supply a confidence interval
or a broad hardware-fidelity score. [comparison-report.json](comparison-report.json)
retains every model result, while [summary.json](summary.json) lists every matched
source-window error and the two-note aggregate.

## Limits that remain

- Relative H2 phase MAE is still **40.94°** (formerly 41.11°). Unknown filters,
  waveform shaping, capture response and gate history remain unresolved. The
  sign correction fixes a large level/harmonic inconsistency; it does not recover
  the complete analog waveform or absolute phase.
- Square-only hardware has even-harmonic leakage around −25 dB. The ideal 50%
  Ceres pulse is almost purely odd-harmonic and does not reproduce that leakage.
  No duty offset or hidden sine amount was fitted to disguise the difference.
- This evidence identifies relative audio sign at **50% PW**. A polarity
  inversion and a half-cycle pulse offset are equivalent there, but differ at
  other widths. It does not verify the entire PW/PWM range or continuous fader
  curve. Cancellation between other branches, particularly filtered sources
  and direct sine, can still be real.
- Yamaha's [servicing guide, printed pp. 28–31](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf)
  establishes a shared VCO/WSC and a pulse edge controlled by PWM. Its separate
  waveform sketches do not establish a sufficiently unambiguous common time
  origin here. The output-sign choice is an empirical audio convention, not
  evidence that an identified transistor or gate inverts that branch. Logical
  comparator/PWM edge geometry must be distinguished from its returned audio
  polarity.

A stronger public reference would keep one note held while switching saw,
pulse and their mixture, show sine/filters/PWM and gain unchanged, and repeat
at several pulse widths and pitches. That would distinguish the remaining
phase convention from a simple sign choice without inventing capture settings.

## Reproduction and retained attempts

The Python analysis uses NumPy/SciPy and FFmpeg. `source_measurement.py` explicitly
asserts an original 48 kHz mono/stereo source before decoding; it does not
silently assume or resample another rate. Every tool rejects an existing output
attempt. Reference audio is an offline measurement input only.

```sh
python3 Docs/accuracy/waveform-mixing-20261019/source_measurement.py \
  --audio "$ceres_reference_audio" --output "$ceres_attempt/source"
python3 Docs/accuracy/waveform-mixing-20261019/polarity_comparison.py \
  --baseline "$ceres_f46_renderer" --candidate "$ceres_sign_renderer" \
  --source-report "$ceres_attempt/source/source-report.json" \
  --output "$ceres_attempt/comparison"
python3 Docs/accuracy/waveform-mixing-20261019/bind_production.py \
  --renderer "$ceres_production_renderer" --source-root "$ceres_checkout" \
  --comparison "$ceres_attempt/comparison" --output "$ceres_attempt/binding"
```

The temporary candidate was built from the unchanged f46 C++17 renderer/core,
with exactly `return {saw, square, sine};` replaced by
`return {saw, -square, sine};`. Actual production parity establishes that the
current production source implements the same result in all six declared cases.
The checked-in reports retain the renderer's complete build identity.

The original source report is retained locally under
`mixing/source-analysis/iteration-001/`; its field called a circular "median"
actually contained a circular mean. The corrected report names that mean
properly. Adding the explicit source-rate contract in attempt 003 reproduced
all 189 individual rows exactly; summary numbers changed only in field name.
The first and final 12-render attempts also remain locally under
`mixing/polarity-comparison/` and `mixing/polarity-comparison-002/`.

The separate [baseline continuous pulse sweep](baseline-pulse-sweep.json) and
[production probe](../../../../Tools/verify_waveform_mixing.py) preserve the former
model's isolated monotonic response and mixed-source dips. They are synthetic
implementation diagnostics, not hardware-verified fader curves, and do not
represent the corrected 0.2.2 sign convention.

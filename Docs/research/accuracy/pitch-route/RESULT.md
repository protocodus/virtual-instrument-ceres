# Pitch-route diagnostic: modest Flute improvement, larger mismatch remains

The new native-only research adapter can keep reconstructed pitch motion on
the oscillator while holding filter key tracking fixed. Its other alternative
adds one common, explicitly hypothetical static master offset to both Flute
passages. The [source review](source-review.md) establishes the separate
physical paths; it does not identify the recording's exclusive modulation
route, true master setting or VIB voltage law. No production MIDI behavior,
patch default, Rack GUI or synthesis sample asset changed.

The isolated DSP is committed at **`7e82a5ba212ebe5bf9cbf41967839a4aa3fbaf7d`**
in `/tmp/ceres-pitch-route-20261019`, parent `ff2dd42`. It preserves the original
oscillator calculation and changes only the filter key exponent. The setter
and CLI options are native-only; the embedded 57-key parameter ABI is unchanged.
Static master tuning stays outside the engineering key-track amount and the
network's alpha. The fixed filter frequency floor stays additive.

## Verification and experiment

The implementation passed **11/11 Release checks** and **3/3 targeted
ASan/UBSan checks**. Exact state tests cover oscillator frequency/phase, EGs,
gates, allocation, SUB/ring/Q/common-VCA states and network nodes across routes.
Default and explicit Shared paths reproduce the parent on **96,000 synthetic
float32 samples per stream**, four rates and all three network modes. The raw
temporary parity streams were not retained; their source/driver/commands and
fingerprints are archived. Tested compiled inputs match the committed source;
the commit itself did not require a rebuild.

The [new immutable declaration](plan.json) fixes three routes × two raw Flute
passages × three cutoff contexts, plus six shared-route compatibility cases:
**24 new native WAVs**. The contexts are the frozen patch and the unfitted
four-voice network at BR=0 or -15 V. The stopped brightness fit supplies no
candidate here. The same constant `+0.367919921875` semitones, taken from the
first saved high-Flute bend, is used for both static-master cases. It is a
deterministic reference rather than a recovered DC estimate. Events, MIDI,
source oscillator trajectories, Q, envelope parameters, masks and recording
gains remain unchanged.

All 24 outputs existed before source scoring. **All 11 known shared-route
controls are byte-identical to their retained references**, including the
six compatibility cases. The lower-Flute dark-network shared control is new
and was not available after the preceding brightness study. No optimizer,
candidate selection or per-segment gain fit ran.

## Actual native results

In the BR=-15 network context, routing all reconstructed bend to the oscillator
improves both spectral measures and the level error against each passage's
matched Shared control. The common-static alternative does not pass those
same gates.

| Dark-network passage / route | Median H2..H7 MAE | Ratio-trajectory MSE | Fixed-gain RMS-level error |
|---|---:|---:|---:|
| High / Shared |6.68942 dB|47.18595 dB²|1.01617 dB|
| High / VCO only |**6.45632 dB**|**42.26946 dB²**|**1.00294 dB**|
| High / common static master |6.93683 dB|48.73953 dB²|1.01912 dB|
| Lower / Shared |4.69747 dB|22.81443 dB²|1.16733 dB|
| Lower / VCO only |**4.22236 dB**|**20.16045 dB²**|**1.15501 dB**|
| Lower / common static master |4.69682 dB|24.50355 dB²|1.17350 dB|

This is a modest control-routing effect: the median spectral improvement is
0.23310 dB high and 0.47511 dB lower, with level-error reductions of
0.01322/0.01232 dB. The much larger raw factory-calibration discrepancy remains.
The dark-network VCO-only level errors also remain worse than the frozen
production controls' 0.66755/0.38795 dB. These comparisons do not support a
production default change.

Other contexts give mixed results. With the frozen cutoff patch, VCO-only
routing slightly worsens both spectra; the common-static route helps high
spectra but worsens its level error and fails the lower comparison. At network
BR=0, VCO-only improves spectra in both passages but worsens both level errors;
common-static fails the spectral gates. The [complete report](run-001/report.json)
retains all 24 metrics, twelve altered-versus-matched contrasts, six direct
static-versus-VCO comparisons, waveform differences, commands and receipts.
No aggregate winner is selected from these disagreements.

Native RMS slopes also barely change: dark-network VCO-only is -1.55336 dB/s
high and -1.76606 dB/s lower, versus the recordings' -0.28906/-0.54280 dB/s.
That difference is a level-trajectory diagnostic, not an isolated measurement
of an amplifier capacitor: filter motion, pressure and gate history also
affect audio amplitude. Audio-estimated F0 remains descriptive; the exact
oscillator/phase invariants are proved by the bound native state tests.

## Reproduction and evidence

The [independent stored-evidence audit](review-001/report.json) verified 180
input/artifact identities, all 24 commands/receipts/logs, all 11 complete-WAV
and sample-payload references, seven state-evidence files and five compiled
inputs. Its 216 scalar metric-difference and contrast checks agree exactly.
It did not decode or remeasure audio, rerun the model, or render new outputs.

The separately declared [metric replay](run-001/check-001/report.json) decoded
the retained outputs and recomputed the unchanged measurements. It returned
**zero differing analysis values** under an advance-declared 1e-10 scalar
roundoff allowance. Its values were exactly equal in this run; that does not
change earlier studies' preserved exact-replay failures.

```sh
python3 Tools/accuracy/pitch_route.py run
python3 Tools/accuracy/pitch_route.py check
```

These are one-time retained attempts and refuse reuse. Restore exact bound
inputs to reproduce in a separately declared location; do not overwrite this
study. Public recordings are local analysis evidence, never synthesis assets.

The [42.95-second listening comparison](listening-002/comparison.wav) includes
all six passage/context groups in plan order. Each plays **recording → Shared
→ VCO only → common static master**, with 0.25-second gaps. It uses the frozen
recording gains, one common playback gain of 1, the benchmark's arithmetic
stereo-to-mono source downmix, and no normalization, fades or effects. Peak is
-9.97 dBFS. All 24 float32 segments and 23 zero gaps were verified exactly;
this derived reel adds no native DSP render. The initial listening-001 format
failure (stereo sources versus a mono-only producer) is preserved; the corrected
listening-002 declaration explicitly matches the benchmark's source downmix.
Original source files are unchanged.

## Remaining work

The source distinction is now implemented and measured, rather than only
listed as a benchmark caveat. It is too small in these fixed contexts to
explain the factory filter/envelope gap. Both alternatives remain conditional;
there is no new independent known-settings/original-MIDI holdout. Unknown
global RES/Q, pressure, gate/voice history, ring contribution and chip trim
still limit absolute calibration.

Before another fit, recheck the retained Flute note/voice history and the
existing preset-resonance/EG-loading audits. The video establishes a
Brass-II-to-Flute selection around 202.60–202.88 s, but the frozen fresh-note
histories do not establish the original capacitor/gate states. A useful next
action is to inspect the already retained key frames and source onset evidence
around each excerpt, identifying what can be constrained without fitting
another arbitrary envelope factor. Preserve this diagnostic and the stopped
80-call brightness attempt; neither authorizes a restart or wider fit bounds.

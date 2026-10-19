# Reconstructed pitch route: bounded native diagnostic

19 October 2026. Method review only: source/code/event metadata read, with no tool imports, model evaluations, source-target analysis, fitting, or rendering.

The proposed native-only experiment is justified. The source distinguishes oscillator-only VIB from the shared keyboard/master-PITCH path, while the reconstructed Flute bends currently change both oscillator frequency and filter key tracking. A fixed route comparison can isolate this assumption without adding another fitted cutoff parameter. It cannot identify the hardware route from the public recording alone.

The stopped [brightness-boundary experiment](../brightness-boundary/RESULT.md) remains stopped. Its last callback is not a candidate and supplies no parameter here. Both full-network contexts retain the unfitted calibration `(HP bias, LP bias, alpha) = (0, 0, 1)`.

## Three explicit hypotheses

Let `b(t)` be the exact original bend-event value in semitones, `n` the physical board's retained key note, `k` the engineering `keyTrack`, and `T` the existing `tuneCents`. The original shared filter key exponent is:

`Kshared(t) = k*(n - 60 + b(t))/12 + T/1200`.

Use one common constant `b* = 0.367919921875` semitones: the first chronological high-Flute bend, before any rendered sample. Apply that same value to **both** passages. The lower first bend, `0.077392578125`, is not used as a second DC estimate. Its difference of 29.052734375 cents from the high first bend is a reason to avoid treating separate first samples as a recovered common master tuning.

| Route | Filter key exponent | Change from shared |
|---|---|---|
| Shared | `k*(n-60+b(t))/12 + T/1200` | `0` |
| All bend to VCO only | `k*(n-60)/12 + T/1200` | `-k*b(t)/12` |
| Common static master, remaining bend to VCO only | `k*(n-60)/12 + T/1200 + b*/12` | `b*/12 - k*b(t)/12` |

The common static term is outside `keyTrack`, just like the already implemented physical master-PITCH term. A literal “hold the first bend in the engineering KV input” would instead contribute `k*b*/12`; that is a different convention at `k != 1` and is not this proposed route. Frozen Flute uses `k=1`, so tests at `k=0` and `k=0.5` must prevent these conventions from being accidentally conflated.

For every route, leave the oscillator calculation, original bend values, master tuning, transpose, sample times, and phase evolution unchanged. In particular, do not obtain the common-static route by numerically rewriting `bend := bend-b*` and `tune := tune+100*b*`; reassociation can change floating-point frequency/phase. Change only the filter-key exponent. This is algebraically equivalent to the stated partition while keeping the source waveform exactly invariant.

The two altered routes differ by the **constant** `b*/12` in the variable-gm exponent in every context and passage. Comparing those two directly distinguishes the effect of a constant filter offset from the removal of time-varying bend tracking. Neither route is an estimate of true mean/DC modulation: the first sample can contain SUB phase, pressure bias, voice offset and pitch-estimation error. VCO-only DC is likewise an unidentified oscillator correction, not proof that a zero-centered physical SUB waveform generates it.

## Cutoff transformation and invariants

For the analytical sign/oracle only, a key change `deltaK` must satisfy

`f_new = f_fixed + (f_shared - f_fixed) * 2**deltaK`.

Apply this equally to HP and LP variable conductance. Keep the fixed 4.7 MΩ feed contribution `f_fixed` unchanged. In full-network mode, keep key tracking outside alpha and preserve the preset/BR/bias/EG/SUB node solve. This formula is an implementation oracle, not an analytical replacement for the proposed native measurements.

The legacy and research cores both currently include bend inside the filter key expression and tuning outside `keyTrack`; transpose, oscillator vibrato and drift/spread are outside filter KV. The route adapter must preserve note allocation, key history, gates, pressure, EG trajectories, source RNG progression, all four oscillator frequencies/phases, Q, common VCA state, and network nodes. Filter audio states and output audio are expected to differ.

Required discriminating checks before public-reference renders:

- Shared mode preserves the previous arithmetic and reproduces known output bytes.
- Identical original oscillator F0 and phase across all routes, with identical EG/source/control state. Audio pitch estimators are not valid exact invariance oracles: a changed filter can slightly bias their estimated F0.
- `b(t)=0` produces zero all-bend-VCO-only correction. A constant `b(t)=b*` with `k=1` makes the static-master filter match shared mode.
- Master tuning and `b*` remain active at `keyTrack=0`; transpose remains oscillator-only. Zero variable conductance keeps the same fixed cutoff floor.
- The static-master versus VCO-only exponent difference is exactly `b*/12`, independent of time, note, `k`, and cutoff context.
- Events at a host frame are dispatched in their original file order before processing that frame. The frame-zero note-on followed by bend must not introduce a sample of an alternate source frequency. No MIDI/event rewriting, re-centering, smoothing, or re-estimation is allowed.

The full high bend sequence has 192 events, from 0.108642578125 to 0.4169921875 semitones. The lower sequence has 122 events, from 0.0712890625 to 0.369384765625. These are metadata from frozen event files, not new pitch measurements. In particular, removing bend always reduces the variable-gm key contribution for these positive sequences; the static-master correction can have either sign.

## Final declared matrix and control accounting

Use three cutoff contexts, all with original Q/EG/source/gains:

1. Frozen native factory patch, research network disabled.
2. Full network at `(0,0,1)`, BR = 0 V.
3. Full network at `(0,0,1)`, BR = −15 V.

For each context, render all three routes for both raw Flute passages: **18 WAVs**. Rerendering shared mode supplies matched controls under the new binary. Five of those six shared outputs have exact retained references: the two frozen factory outputs, the two full-network BR 0 outputs, and high-Flute full-network BR −15. Lower-Flute full-network BR −15 is the one new matched control that did not exist after the stopped brightness study.

Render the remaining six frozen battery cases in shared mode: two effective-Flute cases, two Brass histories, and two dry-source cases. Total: **24 new WAVs**, with **11 known byte-exact reference comparisons**. The six extra cases are compatibility checks; they do not authorize altered-route Brass or effective-patch tuning. This explicit matrix supersedes the earlier provisional count of twelve altered-route outputs, which omitted the missing lower BR −15 matched control and the new-binary identity checks.

Freeze the new binary and five compiled input sources, route interface, common `b*`, all 24 case descriptions, patches, event/MIDI identities, exact commands, reference identities, and unchanged measurement tools before the first render. Keep originals and earlier studies immutable. Render and retain all 24 outputs before calculating canonical source scores. A rendering or exact-control failure remains a failed attempt; do not silently retry or rebase a reference.

## Measurement and interpretation

Use unchanged `run_battery.measure` for every case. For each altered raw-Flute route, report its complete canonical metrics, the numeric differences, and unchanged `preset_dc_loading.contrast` against the **same clip/context's shared route**. Include the direct static-master versus VCO-only comparison and waveform differences, without fitting a gain or choosing a winner. Preserve median harmonic errors, full ratio trajectory errors, fixed-gain level error, RMS slopes and descriptive pitch estimates even when they disagree.

No new control traces or analytical surrogate are necessary for this native-only design. It avoids needing a new lower-Flute state trace and makes every reported sound change a direct native comparison. The archived high trace may remain background evidence, but it should not become an extrapolated promotion argument.

These comparisons isolate a reconstructed control-route assumption within each fixed cutoff context. An improvement could support changing a future benchmark's explicitly declared input hypothesis; it would not establish the original hardware's exclusive VIB route, calibrate pin15 voltage, prove the exponential cents law, or justify a production MIDI-routing/default change. The raised SUB-VCO control makes the hypothesis plausible, while unknown filter modulation, pressure and constant tuning remain material. A better aggregate cannot override an adverse level trajectory or another context's result. Both passages are reused evidence; there is no new independent holdout and no optimizer or candidate selection in this study.

## Read evidence identities

| Input | SHA256 |
|---|---|
| `brightness-boundary/RESULT.md` | `613cd5d4fc777daeffa930de680fa70b551f38d5ea2591ddddfb5b8d01d77b3c` |
| `brightness-boundary/checkpoint.json` | `ef2484f2b2111746ce6d5edcd3ef64544d75c32977e0fe04f3889eb2e243b63f` |
| `brightness-boundary/plan.json` | `9c7c3a4b6ed44375ae1fa7c0952f4df78fd5e6e0bbb99a109ad96ab449cfd0c6` |
| High physical events | `d4f8d7749e38e5cc07d49300e14c206e679523a7b1c589ccb13f248c802da55d` |
| Lower physical events | `fe9e6a9e45782cc099590ba2d0fa97a34f0a8cfb649d7ec77a25f84c894a2b2f` |
| Each original raw-Flute patch | `9135e3e3bf96f18b56ae7a90d53ddfaa65ee8d155d3771799270510a682dd47f` |
| Cached `pitch-modulation-routing.md` | `362dac857bb5227366b8869259dbc34aeb545731241b5da57361896f41b6e655` |
| Cached `keyboard-tracking-and-master-pitch.md` | `d3b865472270f5c21d27e69d2f486c18e9836941127a82de8352a17b5d5b92a5` |
| Research core before new adapter | `f31fe5359c182cda58deda9ee5ca46a3fc78df2ef0fdca7811fa24f2eff6e3f2` |
| Research renderer before new adapter | `25bee66eb014d2575cdac4f836a9fc69435b945569ca14eb4351af7c652a52eb` |

The companion [source review](source-review.md) binds the primary routing evidence and the common-offset interpretation. No previous file was edited by this review.

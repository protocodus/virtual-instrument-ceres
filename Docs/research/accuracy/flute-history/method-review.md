# Flute envelope history: prior work, memory bounds and the next discriminating test

19 October 2026. Read-only methodological audit, followed by the separately authorized constant-only calculation linked below. This work reads existing source notes, declarations, reports and native code. It does not decode or measure target audio, import the synth or an audio-analysis model, render, fit, modify production DSP, or change an earlier attempt.

The next useful question is **which physical board receives a trigger, and which other boards remain audible**. Repeating the old fresh-note age grid would add little. A retained capacitor followed by a normal attack trigger is also not equivalent to a multi-second uninterrupted hold: in the present model its post-attack amplitude effect is bounded by advancing the peak by at most one factory attack duration, about 115 ms. This is a concrete falsifiable restriction, not a claim that every unknown history is negligible.

## What has already been tested

The archived files below are under `/tmp/ceres-cs50-1x8LGd`; their identities are recorded at the end.

| Prior study | Actual intervention | Why it does not settle physical board history |
|---|---|---|
| `tools/profile_whitmore_envelope_age.py`, `calibration/whitmore-envelope-age.json` | Profiled a continuous, fresh single-voice hold over 0.5–12 s, with one constant gain eliminated on high; then rendered the selected histories. High selected 4.191667 s, lower 3.5 s with high gain, and a separate lower free-gain diagnostic 3.072917 s. | These are fitted effective ages. Their implied onsets at 201.5583, 207.3 and 207.7271 s conflict with the simple uninterrupted-note interpretation of the retained chronology. They do not identify a residual charge or a board assignment. |
| `tools/profile_whitmore_fresh_history.py`, `calibration/whitmore-fresh-history.json` | Six nonoptimized fresh histories: high 150/225/300 ms; lower requested 140/180/220 ms, quantized to 139.583/179.167/220.833 ms. Factory EG parameters and one inherited LP-effective patch were held. | All six start with one fresh board and empty state. They explicitly omit the precursor, previous voices, pedal, pressure and assignment. High slopes remained −1.3623…−1.2961 dB/s and lower −1.6050…−1.5503 dB/s, versus archived source −0.2891/−0.5428 dB/s. |
| `tools/fit_whitmore_fresh_envelope.py`, `calibration/whitmore-fresh-envelope-fit.json` | Fitted D/S on high at 225 ms with a constant high gain; transferred the selected pair over all six age cases. Selected D=22.2527435 s T60, S=0.7655158. Also rendered S=1 controls on the middle high/lower histories. | An effective envelope-law fit on the inherited LP patch, not a recovered history or actual terminal voltage. The present task should not repeat this as an initial-state fit. |
| `calibration/whitmore-fresh-envelope-ridge.json` | Retained high-only points D=factory with S=.825 and D=30 with S=.706 give similarly small conditional errors. | Demonstrates the earlier D/S/gain ambiguity. It does not justify changing a shared physical decay constant or sustain mapping. |
| Old resonance-transition study | Modeled the Brass-II→Flute Q-capacitor transient after the visible 202.60–202.88 s tablet transition. | Its declaration explicitly excluded new gate, oscillator and audio-state prehistory. It is not a full preset-and-key history reconstruction. |
| Current [pitch-route result](../pitch-route/RESULT.md) | Reassigned only reconstructed bend's filter contribution at fixed 225/179.167 ms fresh histories, in three cutoff contexts. | It preserves fresh-board EG states. The modest dark-network VCO-only benefit leaves native RMS slopes −1.55336/−1.76606 dB/s. No production change follows. |

The old fresh-history patch was an inherited **effective LP** fit (`lp_cutoff≈411.511`, near-maximum LP resonance), not the present raw factory or full-network patch. Its age sensitivity cannot be copied numerically into the current contexts. Its experimental distinction still holds: fresh-age perturbations and initial-state/assignment histories are different interventions. The newer 225/179.167 ms cases did not newly recover a gate; they retained the middle hypotheses from that grid.

## What the source currently constrains

The archived `whitmore-flute-history-boundaries.json`, `whitmore-flute-history.md` and source-only component audit report:

- High: both hands are off the keybed at 205.20 s; a quiet interval spans approximately 205.20–205.44. A new output transient appears at 205.455–205.465, with a roughly 629 Hz precursor before the roughly 668 Hz target appears around 205.60–205.64. Hand contact/repositioning occurs during that sequence. The frozen 225 ms age implies a gate at 205.525, which is not itself a uniquely identified event.
- Lower: rapid playing precedes the 395→525 Hz transition around 210.58–210.66. Right-hand position changes around 210.56–210.68; the left hand is away. There is no clean silence before the target. The frozen 179.167 ms implies 210.620833, a quantized midpoint hypothesis, not a detected gate.
- Flute selection is visually established after the Brass-II→Flute transition at 202.60–202.88. The predecessor chord/key set, all physical gates, active-board census, sustain-mode state, pedal, pressure and exact audio/video offset remain unresolved.

Hands off and one harmonic ladder are not measurements of empty capacitor states or a single voice. Conversely, the multi-second single uninterrupted target notes inferred by the old age fit are not supported by this chronology. Additional control or voice hypotheses would be required to rescue that interpretation and must be declared independently, not inferred from its good audio score.

## Retriggered amplifier memory has a tight model bound

The retained research core has `Envelope::on()` set `gate=true, stage=Attack` **without resetting voltage**. With current factory parameters and ordinary histories the normalized amplifier state satisfies `0≤u≤1`. The attack target is `1+h`, and the comparator transitions at peak 1. The factory values recorded in the old parameter probe and still defined by current factory code are:

```text
A = 0.1150800388944436 s
h = 1/(exp(5)−1)
tau_A = A/5
D_T60 = 13.815510557964274 s
tau_D = D_T60/log(1000) = 2 s
S = .524
```

For a continuous-time attack starting from retained `u0`, before the peak:

```text
u(t) = 1+h − (1+h−u0)*exp(−t/tau_A)
t_peak(u0) = tau_A * log((1+h−u0)/h)
0 ≤ t_peak(u0) ≤ A, for 0 ≤ u0 ≤ 1.
```

After the comparator, all those histories have the same peak and the same decay law:

```text
u(t) = S + (1−S)*exp(−(t−t_peak)/tau_D)
d(20 log10 u)/dt = −20/(ln(10)*tau_D) * (u−S)/u.
```

Thus larger residual charge shortens attack, makes the state **lower** at a fixed post-trigger time because it has decayed longer, and makes its negative log slope modestly flatter as it approaches S. The largest apparent decay-age advance is A, with one internal-step comparator rounding in the discrete implementation. It cannot supply the several seconds of extra age required by the earlier uninterrupted-hold fit. Both fixed ages are beyond A, and the first complete 128 ms scoring window starts at the excerpt boundary, so this post-attack reasoning applies throughout those windows under the specified amp model.

This is an amplifier-state result, not an exact bound on complete output RMS. Filter-envelope history, filter and coupling capacitors, phase, nonlinear summation and additional voices can alter full audio. A scalar multiplication of an old render by the amp ratio would be a surrogate requiring separate qualification; it must not be presented as a new native result.

The new self-contained [constant-only calculation](amp_memory_bound.py) derives the parameters from the archived factory-CV transcription, checks them against the retained native parameter-probe values, binds the current core and frozen battery, and computes the continuous endpoint bound. Its [result](amp-memory-bound-result.json) was reproduced with exact equality; the independent attack-endpoint substitution has maximum numerical error `1.69e−16`. No audio or synth was read/imported and no residual value was selected from source errors.

| Fixed passage | Largest amp-coordinate level reduction at excerpt start | Maximum instantaneous log-slope flattening anywhere in excerpt | Endpoint-secant flattening across excerpt |
|---|---:|---:|---:|
| High, age .225 s | −.227489 dB | .0619669 dB/s | .0554927 dB/s |
| Lower, age .179166667 s | −.230332 dB | .0620862 dB/s | .0589447 dB/s |

The last column is a secant of `20 log10(u)`, **not** the canonical 128 ms-window RMS regression. These are maxima across every `u0∈[0,1]` for the stated continuous amp coordinate, not a chosen initial-state grid. The native comparator's conservative one-internal-step timing allowance is 5.20833 µs at 192 kHz; this analytical calculation does not claim a fresh native trace verification. Under the separate all-released/global-release-zero condition, the normalized residual after .25 s is at most `1.61133e−8` before the implementation's Idle snap. The source does not establish those release conditions.

An unretriggered held voice could retain a much older decay phase. A released voice instead obeys `u(t)=u_off*exp(−t/tau_R)` until the model's idle threshold. A lower-than-S state does not remain in the held decay trajectory after a new ordinary trigger: it first attacks to peak. Setting a desired low state directly into `Decay` is therefore a distinct initial-condition intervention, not the result of the normal note-on code.

With the **frozen** global-release value zero, Flute's amp release is `tau_R=2*10^(−.3*7.19)` seconds, roughly 14 ms. If all gates really were off for a quarter second, this model strongly attenuates their amp residuals and may enter Idle. This conditional calculation cannot establish that the recording had zero global sustain, no pedal or an exactly aligned visual key-off. It does explain why adding a short preceding note under the frozen controls may make almost no difference by the held excerpt.

## Key ownership matters as much as capacitor charge

Current `Synth::noteOn` chooses an Idle board first; otherwise the quietest released board; otherwise the oldest held board. It assigns note/key, then calls both amp/filter `on()`. Existing filter, oscillator and coupling state is not reset by this assignment. Repeated same-note presses can occupy different boards; note-off is FIFO among held instances. This is the current engineering allocator, not a complete recovered CS-50 KAS priority law.

During this audit the independent hardware review identified a more direct discrepancy: the Servicing Guide printed 19/PDF28 gives a five-successive-key example whose fifth key is not stored in four-note mode. The present oldest-held-steal policy does the opposite once all four boards are held. Likewise, a quietest-EG choice needs an amp-state observation unavailable to the documented KAS path. The full held-key/deferred-assignment rule still needs the companion source review; do not replace it with another guessed policy. This source-backed allocator investigation takes priority over a new recording-history fit. Its deterministic five-key/release oracle can be tested without any target audio or invented predecessor chord.

Consequences for an implementable history test:

1. One preceding note followed by the target normally leaves other Idle boards. The target can receive a fresh amp state on another board. Calling this “retriggered residual” without checking the selected board is wrong.
2. Forcing reuse generally requires a sufficiently populated four-board history. Do not invent three extra held notes or directly write an amp voltage merely to obtain a desired slope. A state-bound probe can use endpoint states explicitly, but it is not reconstructed performance audio.
3. Sustain II changes free/released boards' sampled key voltage when another note is played. It does not retrigger their amp/filter envelopes. Sustain I retains each released board's key. These are separate from MIDI damper and the global release-time selector. The current frozen mode is I; changing it is an additional source hypothesis.
4. A mode-II predecessor that changes to target pitch also changes its filter KV. Multiple boards with different envelopes and phases can share one observed harmonic ladder. No scalar RMS envelope or single-note pitch tracker identifies their count or coherent sum.
5. In full-network mode all four filter EG voltages enter the shared cutoff-node solve. Even an amp-silent board's filter history can matter. A single-board residual replacement is not a full history comparison.

The [preset-envelope-loading audit](/tmp/ceres-cs50-1x8LGd/research/preset-envelope-loading.md) adds a separate physical possibility: AA/DA/SL are shared direct buses with stage-dependent selector loads and signed trim backfeed. A different number of boards in attack/decay/release could change the actual circuit timing or target. However, those chip input/selector loads are not identified or implemented by the current stiff timing controls. The known passive-ground-only loading cannot lower Flute DA below zero or raise SL. Introducing free per-history timing offsets would disguise an uncalibrated circuit model as history.

The [amp offset audit](/tmp/ceres-cs50-1x8LGd/research/amp-eg-offset-closure.md) likewise does not supply a free residual-amplitude parameter. Its related-board bias is a whole-loop affine-coordinate hypothesis with a separate VCA closure coordinate. All stages and retained state must transform together. A release-only subtraction or applying rectification to the stored capacitor state would be inconsistent.

## Smallest new discriminating computation

**First, resolve and test the source-backed KAS assignment rule.** Its five-key and subsequent-release examples provide deterministic oracles that can change which envelope history is relevant without fitting any recording. Preserve all earlier benchmark attempts and distinguish changed input history from corrected board-allocation behavior. A new history comparison should use the reviewed rule, not calibrate around the known engineering allocator mismatch.

The source-independent constant-side amp bound is now complete, as above. If a native state oracle is useful during allocator work, a separately declared diagnostic can check only the two endpoint amp states `u0=0` and `u0=1`, each at the two existing gate ages, with the unchanged factory envelope. Four endpoint trajectories suffice because the monotonic closed-form relation bounds every intermediate residual. Check the independent attack/comparator formula and post-peak time shift against actual internal updates, retaining complete traces and finite-step error. This requires no new public-reference WAV and must not be called physical reconstruction or complete-audio fidelity.

**A full native history comparison becomes informative when the source review supplies an event-order/ownership hypothesis beyond the six old ages.** The useful minimum is one source-constrained prehistory per passage plus its matched fresh-history control, at one predeclared cutoff context/route: four renders, not a cross-product of new ages, release lengths, ranks and global modes. If a source interval leaves two genuinely distinct event orderings, report both as boundary hypotheses and increase that declared count once; do not choose a boundary by target error. If no predecessor ownership constraint can be supported, retain that uncertainty and do the state-bound check first rather than inventing a chord.

A mere precursor→target two-note schedule is newly explicit, but it can still assign the target to a fresh board. Its value is testing that concrete event hypothesis and the collateral filter/phase history, not proving amp residual memory. The high precursor is visibly/audibly supported; its complete gates and board assignment are not. Lower requires more than replacing its midpoint gate by another already-tested age. A whole Brass-II→Flute/key sequence needs a dedicated event driver supporting the single preset change while retaining state; `setPatch` does not reset active amp EGs, but the ordinary static-patch CLI cannot describe that event. Do not pretend a Q-only automation is a full preset-state replay.

Before any full-history render, freeze all events, global controls, assignment convention, preset switch, original post-window bend/off samples, fixed gain, route and cutoff context. Use a common absolute simulation start and clock for matched histories. Equalize silent pre-roll rather than resetting at the scored window. Preserve generated phases and all board state; history legitimately changes them, so do not claim the waveform invariance proved for the earlier routing-only experiment. Do not select another random seed or target board after hearing the result.

Required evidence and gates:

- Before audio, archive per-board ownership, held/released state, amp/filter state and stage at the target gate and excerpt start; inspect that the hypothesized reused board actually was reused. Freeze exact gate/event ordering and scope of any physical claim.
- Verify the unchanged control reproduces its retained reference when the same clock/history is used. Any necessary longer-clock matched control must be declared as new, with its differences from the old reset-origin render reported rather than falsely called byte-identical.
- Render every declared case before canonical scoring. Use unchanged `run_battery.measure`, original masks/windows/recording gain; no per-case constant-gain fit, onset fit or response normalization.
- A favorable history should reduce the absolute source/native RMS-slope mismatch **and** not increase fixed-gain RMS-level error, while not increasing either harmonic-median MAE or ratio-trajectory MSE, in both passages under the same declared interpretation. Keep strict equality/inequality choices explicit before scoring. Report all partial failures; no weighted aggregate or best-history selection.
- Retain source-eligibility status separately: a conditional history improving every metric still does not identify KAS logic, actual pedal/global release, or internal bias. No production EG timing/default change follows from that comparison alone.

This proposal does not authorize execution. It identifies a genuinely untested state/ownership distinction and a bounded preliminary falsification, while avoiding repetition of the old age and D/S searches. The raw factory fidelity goal remains incomplete.

## Inspected identities

Paths in the first column are relative to `/tmp/ceres-cs50-1x8LGd` unless absolute.

| File | SHA256 |
|---|---|
| `research/preset-envelope-loading.md` | `8d2ee732ff35df1e651272622d55a82cc8071500c46009b36e96f8bd9565719c` |
| `research/amp-eg-offset-closure.md` | `bdbb4f27c30e023bc05869475be64f953d23b082909c2e5322a9ffe41899b079` |
| `research/whitmore-flute-history.md` | `6f2183206abe1c43ceb5931678fc4aa74ef058fa14206a5d0ab1849d359ab4af` |
| `research/whitmore-flute-preset-transition.md` | `cbba7b176edc441ccd972b150ded3887ffc23e4d9255f10d1aadfe7fbcb730aa` |
| `research/master-pitch-v09-review.md` | `820a132be8529eacf00dee892b64c7d7e0e1e440ea1f16917ea379d08b37289b` |
| `research/sustain-release-routing.md` | `4df91ecef7eb484805bd84bef20f0b01460d8cb34e8d7702bf82ed1c4a3ed87d` |
| `calibration/whitmore-flute-history-boundaries.json` | `7218c77553f290e823e0f41bfb67b235273aa9928d5a3b80c4a952a02e20c1d1` |
| `calibration/whitmore-envelope-age-plan.json` | `4554180e64efb72e082069008edd33ebc1ec0a3ad275c05c72cb436ceffc639a` |
| `calibration/whitmore-envelope-age.json` | `36b35a1f8f92119dcb818b35e930c0c56739a2425d0acea05ce774fb40e87fb4` |
| `calibration/whitmore-fresh-history-plan.json` | `216ae0186ec7042f4b41039ed01e53a07094bbb90ce6418582dea0cc2eeceff2` |
| `calibration/whitmore-fresh-history.json` | `15b16ab2f9af775fc440120e5a8df324a1e668c203c3839ab405101821a3e4b0` |
| `calibration/whitmore-fresh-envelope-plan.json` | `8525c59ffd49bae64f0281bddd021d5eb1ba9a5d867544e2ec3f2f60198119fd` |
| `calibration/whitmore-fresh-envelope-fit.json` | `cea45346a281f78930b58cc1304898039eb75fc60381f4038012c42c96f73ffa` |
| `calibration/whitmore-fresh-envelope-ridge.json` | `69527068941b6dae16b8801b3008118b01b6bc838a79083bdfcb70df24e79604` |
| `tools/profile_whitmore_envelope_age.py` | `165829c93d77c677e34e3cf21b0ce3d110ab95d047d36276f6eee25b9f98db52` |
| `tools/profile_whitmore_fresh_history.py` | `5d4a085603fb67af7a7c5be685a9a25419c878a5c7d1f3b0025d7e4cee661019` |
| `tools/fit_whitmore_fresh_envelope.py` | `3eb875b8544ef73dda032fc62a205099751e87c9443f997a1f1fe3841a9e460a` |
| `/tmp/ceres-pitch-route-20261019/Source/DSP/ceres.cpp` | `7b0bfc9b9ff2377390ca43ebc4491bf804d53e83ebcff273dfa20cbcfd780306` |

The source-only companion review is owned separately. This note neither rewrites its visual findings nor converts their uncertainty into an estimated physical voltage or gate.

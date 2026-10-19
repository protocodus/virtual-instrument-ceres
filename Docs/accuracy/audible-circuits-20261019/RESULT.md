# Ceres audible circuit experiment

Implemented in the standalone temporary source at `/tmp/ceres-audible-integrated-20261019`, implementation commit `28ddadca6b546f2f81b18adc2d060449214c4da3`. The final renderer SHA256 is `59964a09832bb14772976e5dd53e2b61f5f65de6278d46767948800669dc3a3a`. This is a circuit-model experiment with no sample playback, recording fit, GUI or Rack-package changes.

| Block | Implemented audible gap | Evidence and limit |
|---|---|---|
|Oscillator/WSC|Source-timed nominal59.7286us reset; conditioned saw instead of exposed raw recharge; the same conditioned reset now enters the ideal sine fold. Analytic edge corrections cover saw and sine.|Yamaha current balance, Normal calibration and separate waveshaper block. Flat clamp, exact slice shape and fast-FM behavior remain approximations; no transistor-exact waveform claim.|
|Filter|HP and LP resonance edits now preserve the10uF Q-control capacitor histories, using the existing1.1s stiff-source limit.|Independent closed-form RC/KCL tests, step/reversal/lifecycle checks, static parent parity. Actual source impedances and absolute Q law remain uncalibrated.|
|Envelope|Live initial-level edits after Idle retain capacitor charge and return through the existing release law; the next attack starts from retained charge.|Capacitor/selector topology supports continuity. Guide idle-gm wording is ambiguous, so the chosen post-idle rate remains a minimal continuation, not measured timing.|
|Ring|Four100uF carrier-network capacitors and differential pin7/8 drive, then original Fairchild matched-BJT switching using nominal3Vpp source,25°C thermal voltage and derived DC bias.|Independent nodal/transient/DC/exponential-current/Fourier tests. Bias omits all IC loads; source impedance, signal-port nonlinearity and absolute wet gain remain unresolved. No mean or peak normalization.|

The source and independent reviews are retained in [filter/envelope research](../filter-eg-capacitor-continuity/source-audit.md), [oscillator/passive-ring research](../audible-osc-ring/source-review.md) and the [original Fairchild carrier-law audit](../ring-carrier-nonlinearity-20261019/source-audit.md). The latter acquired and visually verified an actual1970 Fairchild µA796 equivalent circuit; no MC1496 parameter substitution was used. Already-modeled filter/audio coupling parts were not duplicated. The nominal cutoff-CV pole is too fast to explain the audible envelope mismatch and was left out rather than added as a cosmetic fidelity claim.

## Listening and audible consequences

[18.75-second component comparison](comparison-circuits.wav): each pair plays **old then new**, in order: saw, sine, LP resonance edit, HP resonance edit, note after idle IL edit, physical ring at30Hz. These are component-isolated comparisons; exact source paths, hashes, windows and boundaries are in [the montage plan](montage-plan.json) and [verified assembly report](montage.json). Gain is1, no fades or normalization.

All25 component native WAVs were declared before rendering and used the same driver for each compared model. Static Q/IL controls and pulse-only audio are byte-identical where declared. The first20-120ms after the Q edit changes overall RMS by−2.084dB for LP and−6.267dB for HP. The first50ms after a note following an idle IL edit differs by0.819dB RMS, with a substantial waveform/spectral transient. Those changes decay toward the same settled settings.

The finite-reset sine fold increasingly affects upper notes: the chosen model's native sine THD below20kHz is−58.63/−43.81/−29.50/−16.92dB at440/880/1760/3520Hz, compared with approximately−73.63dB for the old ideal sine. This quantifies the new approximation; it is not measured original sine distortion. The earlier conditioned59us diagnostic had already been run and did not beat its fitted6us baseline; that adverse result is preserved, not relabeled as new or discarded.

The ring experiment separates original ideal-sine multiplication, passive-only coupling and physical switching. The passive network alone gives1/30/220Hz RMS ratios .04768/.67083/.98862 versus the original product. Once source-scaled switching is included, the corresponding ratios become .98256/1.39076/1.38314, with extra sidebands and biased signal feedthrough. This demonstrates why passive attenuation alone is not a calibrated wet-output model. The full [three-stage20-second ring comparison](ring-switch-run-001/comparison.wav) retains every rate and all three variants. The derived unloaded DC bias predicts−.427738 steering at zero AC; it is preserved, not subtracted to force a balanced result.

## Accuracy against public recordings

[Full eight-case results](public-frozen/README.md) and [55.516-second hardware → parent → candidate comparison](public-frozen/listening-hardware-parent-candidate.wav) use the unchanged frozen settings, gains, MIDI/gate schedules, masks and analysis windows. All45 frozen inputs, five final compiled-source identities,16 parent/candidate metric trees and24 listening segments were verified. No gain, gate, waveform parameter or eligibility fit was performed. No candidate output clipped or became nonfinite.

| Case | Parent → candidate spectral error |
|---|---:|
|Raw factory Flute high, trajectory MAE|21.772 →21.904dB|
|Raw factory Flute lower, trajectory MAE|19.018 →19.087dB|
|Prior effective Flute high, trajectory MAE|.934 →1.054dB|
|Prior effective Flute lower, trajectory MAE|1.084 →1.012dB|
|Conditional Brass history20, six-cell MAE|3.626 →3.624dB|
|Conditional Brass history10, six-cell MAE|3.635 →3.634dB|

Dry saw changes are mixed across the three fixed windows; dry square remains byte-identical. There is **no overall recording-accuracy breakthrough**. Large raw factory/filter-envelope mapping errors remain. The effective cases were previously fitted/reused development evidence; none is an independent known-settings holdout. Original hardware MIDI is unavailable and the retained schedules are reconstructions. Every public case bypasses ring modulation, so these eight recordings cannot validate its new switching model. These limitations remain part of the result, not a reason to fit hidden terms until the metric improves.

## Verification and delivery

The combined build passes **12/12 Release** and **6/6 targeted ASan/UBSan** contracts, including all changed circuit groups, zero post-construction allocations and active native/embedded parity. The actual physical ring kernel's stationary220Hz sampling comparison against independent3.072MHz exponential-current evaluation has a worst0–24kHz error of4.752e−14 RMS for the declared carrier/440Hz/15kHz products. This is a bounded stage test, not a universal dynamic alias-free claim.

The local native four-voice48kHz/4x CPU probe shows median64-frame CPU time0.2175→0.2226ms for Init and0.2203→0.2293ms for active paths, with zero measured CPU deadline misses. It does not establish Rack-host or target-chip performance.

The temporary source's [reproduction guide](/tmp/ceres-audible-integrated-20261019/Docs/audible-circuits/README.md) and `validation.json` bind the final source and checks. Historical component reports intentionally bind isolated parent snapshots. This study adds25 component native renders and8 final recording renders; all existing frozen plans, previous adverse results and old source branches remain intact. Unidentified internal transfer/loading quantities are explicitly deferred rather than replaced by audio-fitted constants.

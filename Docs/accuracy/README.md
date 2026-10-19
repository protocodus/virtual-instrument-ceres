# Public-recording accuracy battery

Newest research: the [native pitch-route diagnostic](pitch-route/RESULT.md)
implements oscillator-only versus shared filter tracking in an isolated core.
All 24 renders completed and all 11 known shared controls remain byte-identical.
VCO-only routing in the dark-network context modestly improves both Flute
passages, while other contexts are mixed and the larger factory/envelope gap
remains. No production MIDI behavior or default changed; a verified listening
comparison accompanies the full retained results.

Preceding research: the [source-conditioned dark-endpoint experiment](brightness-boundary/RESULT.md)
corrects the unsupported Flute-neutral/Brass-dark control contrast as an explicit
new hypothesis. One native high-Flute BR=-15 control reduces spectral MAE to
6.689 dB but worsens level error to 1.016 dB. Its subsequent constrained fit
exhausts the frozen 80-call budget without a feasible point, so no candidate or
transfer audio follows and no production default changes. The original battery
and all previous rejected studies remain unchanged.

The final sound-path build preserves all eight frozen recording comparisons **byte for byte**. No recording loss improved or worsened. This establishes regression compatibility for these patches; it does not validate the newly active BR/RES/SUB routes. Their controls are neutral in this battery, and the selected named presets use full VCA level. The new output trim remains 0 dB.

The current velocity-capable DSP at `55869df8` was also rerun through the unchanged eight-case plan. [Its retained-identity report](runs/expression-55869df8/report.json) and [comparison](comparison-expression-55869df8.json) reproduce all eight prior WAVs byte for byte. No hardware-fit improvement is claimed from the velocity implementation; all original mapping limitations and losses below remain.


Baseline: git `10e17fd9ecdc0a5a0354bad4c5a53780885a5503`, separately rebuilt from five hash-checked git blobs. Candidate: git `f99a47c967ec478da89c394506575d263bee0c8a`, renderer SHA256 `f114c02a42c6e40178205b8cc973bc552ab32f810c002b713ccbdc19bb638e2b`. All eight baseline WAVs also reproduce the historical Ceres experiment WAVs exactly. No optimizer, gain fit, gate adjustment, mask change, resynthesis from recordings, or default modification is performed here.

**Provenance correction:** the frozen plan accidentally calls the two PTK sources Polynominal. That attribution is wrong. The immutable [metadata erratum](metadata-errata.json) supersedes those two descriptive fields using the original hash-bound PTK manifests: the creator is **ptk / matador**, real-name attribution unestablished, from the [archived primary page](https://web.archive.org/web/20070208025146/http://personal.inet.fi/private/matador/cs-50_index.html). The source WAV hashes, MIDI, controls and metrics were always correct; none changed. The frozen original declaration is preserved for audit.

| Recording/controls | Spectral result, baseline = candidate | Separate level/tail result |
|---|---:|---:|
| Whitmore Flute, raw factory, high | H2–7 median ratio MAE 21.841 dB | Held-gain RMS error .668 dB |
| Whitmore Flute, raw factory, lower |19.165dB |.388dB |
| Whitmore Flute, prior effective patch, high |.731dB; full-trajectory MSE 1.094 dB² |.0297dB |
| Whitmore Flute, prior effective patch, lower |.932dB; full-trajectory MSE 1.521 dB² |.224dB |
| Whitmore Brass I, fixed history20 | Six-cell ratio MAE 3.510 dB | A3/C4 tail 4.349 dB; E4 tail 5.948 dB |
| Whitmore Brass I, fixed history10 |3.514dB | A3/C4 tail 5.435 dB; E4 tail 3.135 dB |
| PTK saw, effective patch | H2–32 MAE .162/.324/.315dB, early/train/late | Same-window RMS error.196/.257/.290dB |
| PTK square, effective patch |.796/1.077/2.298dB |.392/.271/.065dB |

These metrics are not one interchangeable overall accuracy score. Flute measures 45/27 frames, 128 ms windows and 40 ms hops, following the fundamental independently in source and render. Brass retains 243 source-valid partial records in 160 ms windows, equally weights three notes × early/release, keeps weak native partials in the score and reports weakness separately. Its tail metric floors both signals 40 dB below each source note’s early median; it is descriptive and does not identify hardware gates or an RC law. PTK windows are fixed at .4–.6, .8–1.0 and 1.2–1.4 s. Square odd-only results are retained separately in JSON; neither weak even harmonics nor the odd subset is silently removed from the all-harmonic result.

The [comparison JSON](comparison-soundpath-final.json), [candidate report](runs/soundpath-final/report.json), [baseline report](runs/baseline/report.json), and [predeclared plan](plan.json) contain full metrics, ordered native commands, file identities and signal statistics. The [55.516s listening comparison](listening-soundpath-final.wav) plays **hardware → baseline → candidate** for each case, in plan order, with .25 s gaps. Previously frozen recording gains are applied; one common playback gain is 1.0, with no individual normalization. Before/after synth tracks are deliberately identical. The JSON supplies exact segment frames. Audio is local, git-ignored evidence, not a distributable sample asset.

Every MIDI file is a **synthetic or inferred reconstruction**, not the original hardware MIDI. Whitmore’s [public dry demo](https://www.youtube.com/watch?v=Fj21t8MIS_o) supports the named preset, and video supports Flute’s physical note/transpose correction; it does not establish exact global voltages, pressure, all active voices or gates. High/lower Flute histories are the frozen 225 ms/179.167 ms fresh-onset scenarios. The effective patch was previously selected using the high excerpt, with lower reused as development transfer. Source-following pitch bend also moves Ceres filter key tracking; the physical modulation route is not identified. Brass uses two existing hypothetical chord-release histories, conditional BR-boundary-equivalent filter controls and common AM. PTK’s named waveform and no-external-effects statement are useful but do not identify complete panel settings.

The [cutoff-control capacitor audit](cutoff-control-audit/README.md) bounds the omitted nominal passive pin4 dynamics to at most30µs, including finite shared source loading. That source-backed bound rules out these particular capacitors as an explanation for the DC cutoff mismatch or long envelope timing error; it is not an audio-fidelity improvement.

There is **no independent known-settings/original-MIDI holdout** in this battery. All windows have been exposed during development. Low losses for prior effective patches are not evidence that the raw factory mapping is correct. The [bounded active-source screen](ACTIVE-SOURCES.md) recovered manual VCF/resonance sweeps and VCO modulation, but no sufficiently controlled public BR/RES or SUB→VCF comparison. New-route circuit tests and synthetic reachability demonstrations must therefore be reported separately from recording fidelity.

Run from this repository with Python3, NumPy, SciPy and CMake available:

```sh
python3 Tools/accuracy/verify.py
python3 Tools/accuracy/run_battery.py build-baseline
python3 Tools/accuracy/run_frozen.py new-candidate --binary build/ceres_render --source-root .
python3 Tools/accuracy/run_battery.py compare new-candidate
```

A run label is never reused, including an unfinished run. The new `run_frozen.py` entry point retains a byte-exact renderer and all five compiled source inputs under `run-identities/<label>/` before invoking the unchanged, hash-bound battery, so later DSP builds cannot invalidate an earlier comparison. Those local executable/source snapshots are git-ignored analysis evidence. Verification fails if the frozen source excerpts, settings, measurement helper, MIDI schedule, compiled source or saved audio changed. `prepare` was the one-time import/declaration from `/tmp/ceres-cs50-1x8LGd`; it deliberately refuses an existing plan. `plan.json` records exact original paths/hashes to restore omitted local media. The saved native commands can replay any one case to a new output filename. Original recordings, build products and copied baseline C++ are excluded by the scoped `.gitignore`; keep those local artifacts for reproduction. A clean clone without them cannot verify or run the battery until the exact hash-matching inputs are restored. It must not silently substitute different recordings.

The brief pre-output-trim candidate is retained as `runs/soundpath`; its former mutable source/binary identity is explicitly resolved to a hash-matching local archive by [identity-relocations.json](identity-relocations.json). The final comparison uses `runs/soundpath-final`. Source media remains copyrighted by its creators; public availability supplies no inferred redistribution license.

The later velocity build superseded the mutable renderer path used by the historical `soundpath-final` report. Its original executable has been rebuilt from the exact hash-verified `b22dda69` source blobs and recovered **byte for byte** (`f114c02a…`). The explicit `identity-relocations.json` entry resolves that unchanged report to `soundpath-final-source/`; its recorded audio and measurements are untouched.

The [eight-case effective-release isolation](release-isolation/README.md) varies only effective filter release across two fixed Brass histories while preserving amplifier timing, MIDI, masks and gain. Both unchanged controls are byte-identical; no faster factor passes the predeclared cross-history criteria. Small gains in one history are offset by worse note/tail errors in the other, so no DSP/default correction is adopted.

The subsequent [68-render preset DC-loading experiment](preset-dc-loading/README.md) tests the full nominal-divider-to-loaded-preset-bus intercept under explicitly declared voltage anchors, zero chip-current/centered-bias assumptions and verified diode states. All four unchanged controls are byte-identical. BR −15 V loading improves Flute spectra but worsens its amplitude trajectory and both Brass aggregates; none of twelve common settings transfers across all four clips, including against matched stiff-source controls. No production mapping change is adopted. This is distinct from the earlier incremental BR/LP-bus experiments.

The [short-window Brass harmonic-ratio diagnostic](brass-ratio-modulation/README.md) measures the fixed 6.669 Hz component with 40/60/80 ms joint partial estimates and sixty declared synthetic controls. H4/H2 shows repeatable motion, but neither target clears the declared threshold against the tested stationary-filter/moving-pitch family. It supplies no basis for fitting a new filter-LFO depth or identifying a hardware modulation route. All source rows, carrier sensitivities, failed gates and recovery controls are retained; deterministic recomputation matches exactly.

A subsequent [shared-SUB polarity correction](sub-polarity/README.md) is retained in a separate standalone DSP branch. Direct circuitry requires positive SUB to raise cutoff and common gain together; the former implementation used opposing signs. Circuit-state tests cover all five waveforms, and zero-tremolo audio remains exact. Both raw-phase and algebraic half-cycle recording comparisons are retained. This is a source-backed routing correction, with no claim that the unresolved factory filter/envelope mapping now matches the recordings. The Rack package remains on its separately validated prior core revision.

The [shared-calibration identifiability audit](calibration-identifiability/README.md) now makes the source-network ambiguity reproducible. Its declared12×8 local control Jacobian has rank5, with three independently verified null directions. It reads no audio and does not establish hardware calibration; fixing the nuisance controls can support a conditional fit but cannot recover their actual values. A separate [sine-input admittance note](sine-input-admittance.md) bounds the omitted passive pole family without choosing an unsupported input impedance or production correction.

The [primary video inspection](preset-video-visual-audit/README.md) retains82 paused on-service screenshots across the AutomaticGainsay preset/amplifier and Doctor Mix preset videos. It establishes useful camera/control evidence and specific remaining component-video leads, but no controlled voltage/gate benchmark or new numerical fidelity score. No DSP, GUI, sample asset or original battery input changed during these audits.

The subsequent [filter-video inspection](filter-video-inspection/README.md) adds34 source frames and confirms the buffered manual-cutoff source path. The proposed two-position test remains unqualified because the cap is obscured during the relevant sweep and Q changes. Separately, the new [joint full-network calibration experiment](joint-calibration/RESULT.md) implements all four instantaneous EG contributions in an isolated native mode. Its surrogate and circuit checks pass;20 native renders preserve all8 disabled baseline outputs but reject the one fitted candidate. Flute spectra improve while its level curve and both Brass histories worsen. The production mapping and retained SUB-polarity correction remain unchanged.

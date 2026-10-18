# Public-recording accuracy battery

The final sound-path build preserves all eight frozen recording comparisons **byte for byte**. No recording loss improved or worsened. This establishes regression compatibility for these patches; it does not validate the newly active BR/RES/SUB routes. Their controls are neutral in this battery, and the selected named presets use full VCA level. The new output trim remains 0 dB.

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

There is **no independent known-settings/original-MIDI holdout** in this battery. All windows have been exposed during development. Low losses for prior effective patches are not evidence that the raw factory mapping is correct. The [bounded active-source screen](ACTIVE-SOURCES.md) recovered manual VCF/resonance sweeps and VCO modulation, but no sufficiently controlled public BR/RES or SUB→VCF comparison. New-route circuit tests and synthetic reachability demonstrations must therefore be reported separately from recording fidelity.

Run from this repository with Python3, NumPy, SciPy and CMake available:

```sh
python3 Tools/accuracy/verify.py
python3 Tools/accuracy/run_battery.py build-baseline
python3 Tools/accuracy/run_battery.py run new-candidate --binary build/ceres_render --source-root .
python3 Tools/accuracy/run_battery.py compare new-candidate
```

A completed run label is never overwritten. Verification fails if the frozen source excerpts, settings, measurement helper, MIDI schedule, compiled source or saved audio changed. `prepare` was the one-time import/declaration from `/tmp/ceres-cs50-1x8LGd`; it deliberately refuses an existing plan. `plan.json` records exact original paths/hashes to restore omitted local media. The saved native commands can replay any one case to a new output filename. Original recordings, build products and copied baseline C++ are excluded by the scoped `.gitignore`; keep those local artifacts for reproduction. A clean clone without them cannot verify or run the battery until the exact hash-matching inputs are restored. It must not silently substitute different recordings.

The brief pre-output-trim candidate is retained as `runs/soundpath`; its former mutable source/binary identity is explicitly resolved to a hash-matching local archive by [identity-relocations.json](identity-relocations.json). The final comparison uses `runs/soundpath-final`. Source media remains copyrighted by its creators; public availability supplies no inferred redistribution license.

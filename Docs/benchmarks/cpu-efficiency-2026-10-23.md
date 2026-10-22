# Ceres CPU efficiency — 2026-10-23

Ceres 0.6.6 reduces repeated DSP work while retaining the 0.6.5 circuit equations, nonlinear solver, sample clocks, quality settings, latency and stored state. The baseline is `1cad081` (the media-only child of `8f543ce`, the engine used by the current Rack adapter). This is an unreleased source change; no installed plug-in or Rack Extension was replaced.

## Changes

- Share the filter networks' rate/mixer coefficients across voices, and compute each smoothed Q bus's damping coefficients once. Exact input equality permits reusing settled coefficients; automation still updates every internal sample.
- Cache each filter's tangent prewarp only while its clamped cutoff and sample rate are exactly equal. Moving cutoff controls use the original `tan` without interpolation or approximations.
- Cache the board pitch residual while its exact key/bend/PITCH expression is unchanged. Patch changes, board redraws and reset invalidate it; gliding keys continue to recompute it.
- Skip muted sine shaping while advancing oscillator phase and the TO–STI coupling capacitor unconditionally. Enabling sine later hears the same continuing state.

Idle voices, noise generation, ring circuitry and passive capacitor histories continue running. No oversampling, solver iterations, noise model, gain, polyphony or latency was reduced.

## Audio and safety

The same `Tests/CpuParityDriver.cpp` was compiled against each core with identical C++17/`-O3` flags. **All 439,296 float32 samples match byte for byte**, including signed zero, with SHA-256 `6039d981aaf29c4846cc7400e51091c1ac44db8ce25b5d4c1791884c70a24012`. Repeating the comparison with `-fno-builtin` also matches exactly.

The protocol covers all 14 factory presets across 1/4/16 voices and 1x/2x/4x, representative controls at 44.1/48/96/192 kHz, live waveform gate changes, reversed resonance ramps, glide/bend/temperature/tolerances, sustain/retrigger/release, EXT IN, ring modulation, reset, panic, reconfiguration and stationary/idle paths. This is deterministic regression evidence on this compiler/platform, not a listening-test verdict or a claim about every possible session.

- Release DSP CTest: **32/32 passed**, including all unit contracts, seeded fuzzing and repeated full preset demos.
- ASan/UBSan/float-cast-overflow CTest: **31/31 passed** (30 unit contracts plus fuzzing), built with `-O1 -g`.
- Unchanged Rack adapter against the optimized core: normal and ASan/UBSan integration suites passed, including all 48 voice/quality combinations, five rates, live controls, exact canonical audio and zero realtime C++ allocations.
- Verified `JukeboxSDK_500_028`, `TargetVersion=5.0`: actual SDK 5 C++17 bitcode compile/link and immutable-global checks passed using the optimized canonical source and unchanged Rack wrapper. Compile-only; no host, installation or package was exercised.

[Audio comparison reports and validation logs](../validation/cpu-efficiency-2026-10-23/).

## Native DSP timing

[Complete measurements](cpu-efficiency-2026-10-23.json) retain compiler/command/source/binary hashes, raw results and every deadline-miss count. Apple arm64, Clang, `-O3`, embedded ABI, 48 kHz and 64-frame blocks. One synth; Init and active scenes use four held voices/4x with 500 warmup and 10,000 measured blocks. The configuration matrix uses 128 warmup and 1,000 measured blocks per row. Runs use a fixed before/after order with no retries or selection.

| Workload | Baseline median thread CPU, ms | Candidate median thread CPU, ms | Reduction |
| --- | ---: | ---: | ---: |
| init | 0.287000 | 0.236333 | 17.65% |
| active | 0.296333 | 0.258417 | 12.80% |
| configuration/1v/1x | 0.036916 | 0.033834 | 8.35% |
| configuration/1v/2x | 0.068833 | 0.062917 | 8.59% |
| configuration/1v/4x | 0.123000 | 0.112333 | 8.67% |
| configuration/4v/1x | 0.086667 | 0.076666 | 11.54% |
| configuration/4v/2x | 0.162833 | 0.142125 | 12.72% |
| configuration/4v/4x | 0.307125 | 0.266958 | 13.08% |
| configuration/16v/1x | 0.306834 | 0.259708 | 15.36% |
| configuration/16v/2x | 0.578292 | 0.490709 | 15.15% |
| configuration/16v/4x | 1.058542 | 0.900125 | 14.97% |

This is a measured reduction in thread CPU on this machine. Other work was active; wall deadline misses are preserved in the report and these results do not establish real-time scheduling guarantees. The maximum 16-voice/4x candidate still recorded 15 thread-CPU and 21 wall deadline misses in 1,000 blocks; the four-voice rows had no thread-CPU misses. The native probe excludes the Rack wrapper, Reason scheduling and SDK target translation. Earlier measurements of the first optimization iteration remain under ignored `build-cpu-evidence/before/` and `build-cpu-evidence/after/`; the final implementation was then measured once with the reproducible runner.

## Native Rack callback timing

The unchanged current `CCeres::RenderBatch` was compiled against both cores at `-O3`, using a test MOM/buffer shim. Both engines were asserted to use the shipped four voices/1x; Mono keeps the second engine warm. 48 kHz, 64-frame blocks, four velocity-127 notes, 1,000 warmup plus 10,000 measured blocks per mode. The fixed order was baseline Mono, candidate Mono, baseline Double, candidate Double, with no retries.

| Mode | Baseline mean thread CPU, ms | Candidate mean thread CPU, ms | Reduction | Median reduction |
| --- | ---: | ---: | ---: | ---: |
| Mono | 0.176484 | 0.155319 | 11.99% | 9.60% |
| Double | 0.171535 | 0.154145 | 10.14% | 9.08% |

Before/after measured stereo audio fingerprints, checksums, peaks and RMS match in both modes. Every run observed zero post-construction C++ allocations and zero thread-CPU deadline misses. Wall misses were 425/231 for baseline/candidate Mono and 274/262 for Double; no zero-wall-miss gate is claimed. The timing helper's shim was adapted to the current wrapper's 66 properties, expression pedal and disconnected EXT IN; production adapter files were unchanged.

[Summary and retained deadline counts](cpu-efficiency-rack-native-2026-10-23.json). The full local report, all per-block overruns, frozen helper changes, sources/hashes and reproduction script are retained under ignored `build-cpu-evidence/wrapper/`. This is native callback evidence, not Reason host or translated target performance.

## Reproduce

```sh
python3 Tools/benchmark_cpu.py --baseline-ref 1cad081 \
  --output /tmp/ceres-cpu-new-report.json --build-dir /tmp/ceres-cpu-new-build
```

The runner refuses an existing report and a populated build directory. It freezes both cores and the same existing benchmark drivers, runs each workload once, requires exact checksums, and retains failures. Use an idle machine for new measurements; retain every run.

Build `ceres_cpu_parity` normally with CMake. To repeat the strict audio comparison, compile that same driver against the baseline core with the same compiler/flags, then run:

```sh
python3 Tests/check_cpu_parity.py --baseline /path/to/baseline-driver \
  --candidate build-cpu-after/ceres_cpu_parity --report /tmp/ceres-cpu-audio.json
```

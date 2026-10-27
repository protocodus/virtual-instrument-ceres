# Exact filter CPU optimization validation

Baseline: `9c292b76c892483991092c278be499a1910f0fec`. The candidate caches each
OTA filter's final `tanh(b)` for the next solve, which starts at the identical
`previousB`. It keeps the original nonlinear law, Newton updates, seven-iteration
ceiling, convergence threshold, capacitor states, rates, gains and presets.
The additional cache costs one double per filter, or 256 bytes at 16 voices.

The same drivers and compiler flags were used against frozen baseline/candidate
sources. Both `-O3` and `-O3 -fno-builtin` comparisons pass byte for byte:

- Existing CPU audio protocol: 439,296 finite float32 samples.
- Direct filter protocol: 208,896 frames containing output and five electrical
  states, totaling 1,253,376 finite float64 values. This covers six rates from
  8 kHz to 1.536 MHz, four source conductances from 1e-12 to 0.1 S, both output
  selections, cutoff floor/nextafter-floor/clamping, resonance endpoints and
  moving resonance, signed high-level input, live rate/network changes with
  retained charge, zero initialization and reset.

See `exact-parity.json`, `exact-parity-compile-commands.json`, and
`Tests/FilterCpuParityDriver.cpp`. Frozen cores, binaries and raw streams remain
in the ignored `build-cpu-filter-audit/` directory.

Fresh CMake builds use plugin/CLAP OFF, tools ON, universal OFF:

- Release: **33/33 CTest checks passed**, including the complete research
  preset-demo finite/audible/ceiling/determinism check and seeded fuzzing.
- ASan/UBSan/float-cast-overflow, `-O1 -g`: **32/32 passed**, including all
  unit/native-embedded parity contracts and seeded fuzzing. Only the duplicate
  long preset-demo render is omitted from this sanitizer execution.

Both builds completed without compiler warnings. Configuration/build/test logs,
JUnit results and full CTest details are retained here. `source-state.json`
records exact source hashes and commands, and confirms the source bytes stayed
unchanged through validation. The active SDK version was read as
`JukeboxSDK_500_028`, `TargetVersion=5.0` before building.

These are native development-machine correctness checks. No CPU timings,
translated target builds, host playback, installation or packaging are claimed
by this validation record. Rack-bank parity and performance evidence are
recorded separately by their respective probes.

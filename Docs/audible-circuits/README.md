# Ceres audible circuit experiment

This standalone temporary worktree combines oscillator conditioning, filter-Q capacitor continuity, idle filter-EG continuity and passive differential ring-carrier coupling followed by source-scaled matched-BJT switching. The current source is a native and embedded C++17 DSP library with no sample playback or GUI work.

Run the combined contracts from this worktree:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j6
ctest --test-dir build --output-on-failure
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON
cmake --build build-sanitize -j6
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize -R 'oscillator_circuit_contracts|capacitor_continuity_contracts|ring_carrier_circuit_contracts|ring_switching_circuit_contracts|embedded_allocation_contract|capacitor_native_embedded_parity' --output-on-failure
```

The final combined snapshot passes12 Release contracts and6 targeted ASan/UBSan contracts. It includes all changed circuit groups, current native/embedded parity, zero post-construction allocations and live capacitor edits. `release-final.log` and `sanitize-final.log` record the final integrated checks; the earlier logs are preserved. The final physical-ring alias check is `ring-alias-final.json`.

`Docs/oscillator-conditioning/`, `Docs/capacitor-continuity/` and `Docs/ring-carrier/` preserve historical component-isolated evidence. Their `validation.json` files bind different source snapshots, not this combined binary. The component parent-comparison recorder commands belong in their respective isolated worktrees; they are not old-versus-new parity assertions for the intentionally changed combined oscillator model. The current combined test commands are above.

Declared native A/B scenes, source reviews and recordings remain at `/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-circuits-20261019/`. Twenty-five source-bound native WAVs isolate Q/EG, oscillator, passive ring and physical ring-switching effects. Eight further native WAVs evaluate the unchanged public-recording battery. Static Q/IL controls, the pulse-only comparison and ring bypass are exact where specified. No normalized or fitted comparison audio is used.

The audible circuit experiment is implemented and tested. The [study report](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-circuits-20261019/RESULT.md) includes an18.75s old/new component montage, a55.516s hardware/parent/candidate comparison, all declarations and explicit unresolved model terms. The final public-recording result is small and mixed: effective Flute improves at the lower note and worsens at the higher; Brass aggregate changes are tiny; raw factory Flute still has roughly19-22dB harmonic-ratio error. No broad hardware-match improvement is claimed. All public cases bypass ring modulation, so that stage is validated against circuit/thermal equations and controlled native demos, not a measured hardware ring reference. Original hardware MIDI and an independent known-settings holdout remain unavailable.

The final renderer is `build/ceres_render`, SHA256 `59964a09832bb14772976e5dd53e2b61f5f65de6278d46767948800669dc3a3a`. Source implementation commit is `28ddadca6b546f2f81b18adc2d060449214c4da3`; subsequent changes document and retain evidence without changing compiled DSP inputs. Runtime sources are `Source/DSP/ceres.cpp` and `Source/DSP/ceres.hpp`. Source and report bindings are in `validation.json`.

The native four-voice48kHz/4x performance probe reports median CPU per64-frame block of0.2175→0.2226ms for Init and0.2203→0.2293ms for active paths, with zero measured CPU deadline misses. This is a bounded local native comparison, not a Rack/target-host guarantee. Full statistics and source/binary bindings are in `performance/comparison.json`.

Exact VCO clamp/slice behavior, fast-FM monostable dynamics, finite source/chip loading, the filter Q/voltage scale, idle EG return-rate ambiguity, audio-port ring transfer, populated-chip DC bias and wet gain remain model limitations. Unidentified quantities were not fitted to the recordings. The small cutoff-CV pole and duplicate already-modeled audio/output capacitors were not added as spurious audible fixes.

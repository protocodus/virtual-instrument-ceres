# Native-only pitch-routing intervention

19 October 2026. Isolated worktree `/tmp/ceres-pitch-route-20261019`, parent `ff2dd42e71f545bd52e621bac896d1e46c132d53`. This is a research intervention on the filter contribution of **the complete incoming MIDI pitch-bend value**. It does not identify a Yamaha VIB transfer, recover physical pitch controls from audio, or alter the oscillator. The separately retained SUB-polarity correction is outside this parent/experiment and is not reverted by it.

## API and formulas

The native header exposes:

```cpp
enum class ResearchPitchRoute { Shared, OscillatorOnly, StaticMaster };
void Synth::setResearchPitchRoute(ResearchPitchRoute,
                                 double staticMasterSemitones = 0.0);
```

Let `n` be each physical board's current `keyNote`, `b` the existing MIDI bend in semitones, `k` the engineering key-tracking slope, and `t` the existing `tuneCents`.

| Route | Filter key exponent in octaves |
|---|---|
| Shared (default) | `(n-60+b)*k/12 + t/1200` |
| OscillatorOnly | `(n-60)*k/12 + t/1200` |
| StaticMaster | `(n-60)*k/12 + t/1200 + staticMasterSemitones/12` |

Shared retains the exact old expression and arithmetic order. StaticMaster **replaces** the bend's filter contribution with one caller-supplied constant outside `keyTrack`; it does not add the constant to the original bend contribution. It does not multiply that constant by the full-network alpha. The passive 4.7M integrator feed floor remains fixed. Both the existing cutoff controls and the optional coupled calibration network consume the same resulting filter key exponent.

The oscillator line remains byte-for-byte unchanged:

```cpp
440 * std::exp2((v.keyNote-69 + bend + 12*patch.octave + cents*.01)/12)
```

No note, velocity, event schedule, oscillator phase, random draw, SUB/PWM/ring clock, envelope update, gain control, filter source mixing, Q control, output coupling or FIR operation is added or rerouted. Filter audio states naturally evolve differently when their cutoff changes; this intervention does not claim identical filter capacitor histories or audio across the non-default routes.

The setter validates before mutation. Unknown enum values, nonfinite offsets and offsets outside −48..48 semitones throw `std::invalid_argument`. Shared and OscillatorOnly reject a nonzero offset rather than silently ignoring it. StaticMaster accepts zero. Route configuration survives `setPatch`, `allSoundOff` and `reset`; reset still clears the ordinary bend/controller and analog state as before. Live route edits preserve state and take effect on the next internal sample, with no added ramp or phase reset. This is an offline control, not a new host automation feature.

The enum, setter and fields are absent under `CERES_EMBEDDED_TARGET`. Patch layout/serialization and its existing 57-key embedded control ABI are unchanged. The four previous native calibration keys are also unchanged. No Rack files or other worktree were edited.

## CLI

```text
ceres_render --events events.tsv --output output.wav --frames N \
  --preset NAME --patch file.patch --rate 48000 --seed 50 \
  --research-pitch-route shared

ceres_render [same inputs] --research-pitch-route vco-only

ceres_render [same inputs] --research-pitch-route static-master \
  --research-master-semitones 0.367919921875
```

Omitting both flags gives Shared. The renderer does not inspect the first bend or estimate a static offset; a study must derive and freeze any offset independently and pass it explicitly. Supplying `--research-master-semitones` outside StaticMaster is rejected even when the supplied value is zero. StaticMaster without the master flag uses zero. The summary JSON records `research_pitch_route` and `research_master_semitones`. Event parsing and dispatch are unchanged.

## Verification

Release CTest: **11/11 passed**. Targeted ASan/UBSan: **3/3 passed**, with leak detection disabled and address/undefined failures configured to halt. Logs and exact compiled identities are bound by `pitch-route-validation/validation.json`.

The new state tests cover calibration mode0 plus both native full-network modes, keyTrack0/.5/1, negative/positive tune, dynamic positive/negative bend, SustainII, repeated notes/stealing, pressure, pedal release, note-off, panic and reset. They compare oscillator frequency and phase, both EGs, gate/ownership state, SUB/ring state, Q-control state, common-VCA gain and calibration node voltages exactly across routes. Filter-frequency checks retain the fixed floor and independently verify the bend removal/static factor. Additional tests cover exact no-bend/zero-offset audio nulls, a setter that does not advance/reset state, invalid-input nonmutation and configuration persistence. CLI tests cover explicit/default values, endpoint validation and unchanged synthetic input events. A compile-time embedded test checks that the native setter is absent; the existing embedded suite retains zero post-construction allocations.

`Tools/check_pitch_route_shared_parity.py` freshly compiles the frozen parent source against the same synthetic driver, then compares parent-default, candidate-default and candidate-explicit-Shared. Each stream contains **96,000 float32 samples** across 44.1/48/96/192kHz and calibration modes0/1/2, including moving bend/pressure, mixed velocities, release, pedal, stealing, panic and reset. All three streams are byte-identical:

```text
SHA256 a87e9e0df1a6d6224797927463c8fa4f2d7bdeb7ef771fe33debc152eb613be4
```

The broader existing native/embedded and active-expression parity tests also pass. Exact equality is evidence for this compiler/math-library build and these test streams, not a proof for every possible floating-point environment. No hardware-recording-linked render, fit or score was produced during implementation. CLI tests use disposable synthetic WAVs; the parent comparison uses disposable raw-float streams.

## Reproduce

From the isolated worktree:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=/Library/Frameworks/Python.framework/Versions/3.11/bin/python3
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 Tools/check_pitch_route_shared_parity.py \
  --report build/shared-parent-parity.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON \
  -DPython3_EXECUTABLE=/Library/Frameworks/Python.framework/Versions/3.11/bin/python3
cmake --build build-sanitize --target ceres_pitch_route_tests ceres_render ceres_embedded_contract -j4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure \
  -R 'research_pitch_route_contracts|research_pitch_route_cli|embedded_allocation_contract'
```

The Python interpreter path above records the tested environment, not a runtime DSP dependency. NumPy is used only by the pre-existing offline calibration oracle. Research code and documentation remain uncommitted pending parent review.

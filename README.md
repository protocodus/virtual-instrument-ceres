# Ceres

Ceres is a four-voice, sample-free CS-50 research synthesizer core. This repository owns the reusable C++17 DSP. The Reason Rack Extension consumes it through a pinned `Instrument` Git submodule; host code, panel assets and Rack patches belong to that separate project.

Product version **0.1.0** imports the canonical **v0.10.0 research model**. It preserves that model's audio calculations and adds a small embedded build interface. This is a reduced circuit model with provisional component/control calibration, not a verified transistor-level recreation or a completed hardware match. No hardware recordings, manuals or sampled instrument data are required at runtime.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

The build creates `ceres` and `ceres_embedded` static libraries, the standalone `ceres_render`, and native tests. CTest includes the original 21 DSP groups, a 64-frame embedded allocation contract, and exact native/embedded audio comparison. Python 3 is required for the parity check.

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCERES_SANITIZE=ON
cmake --build build-asan -j 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

## Use the core

Compile `Source/DSP/ceres.cpp` and include `Source/DSP/ceres.hpp`. For a Rack target define **`CERES_EMBEDDED_TARGET=1` consistently for the core and all consumers** and use C++17. The macro changes `Patch`'s layout, so native and embedded objects must never be mixed across an ABI boundary.

```cpp
#include "ceres.hpp"

// Construct outside the audio callback, once per device/sample-rate lifetime.
ceres::Synth synth(48000.0, 50);
ceres::Patch patch;
ceres::setParameter(patch, "lp_cutoff", 6000.0);
synth.setPatch(patch);
synth.noteOn(60, 1.0);
// Dispatch events at their actual frame before calling process().
for (unsigned frame = 0; frame < 64; ++frame)
    output[frame] = synth.process();
```

The embedded interface has a numeric `Patch`, a C-string parameter setter, no exception path, and no allocating offline probes/preset APIs. Construction allocates the synth state. Rendering, controller changes, validated patch updates, panic and reset use existing state. Do not construct or replace a synth inside `RenderBatch`.

The output is mono. All four physical voices and their capacitor states continue running when no note is held. `activeVoices()==0` does **not** imply silence: the common output bus has a roughly 0.888-second time constant. `allNotesOff()` honors the MIDI pedal; `allSoundOff()` clears audio states while preserving controllers; `reset()` also restores seeded oscillator/controller state. The FIR has nominal group delay 31 host samples, with the decimator's sampling phase affecting event alignment.

## Standalone render

```sh
./build/ceres_render --events Examples/init.events.tsv --preset init \
  --rate 48000 --frames 288000 --seed 50 --output /tmp/ceres-init.wav
```

Events are sorted `frame type note value` rows; supported types are `on`, `off`, `bend` (semitones), `pressure`, `sustain`, `alloff`, `panic` and `reset`. Values for note-on velocity/pressure/pedal are normalized to 0..1. The output is mono float32 WAV with no normalization. `--patch path` applies `key=value` overrides to the selected native preset. `--build-info` prints the compiled source identity.

The native research preset table is retained for regression and is excluded from the embedded target. Its presence does not establish a calibrated hardware preset bank or require that the Rack product expose it.

See [DSP source and model limits](Docs/DSP_SOURCE.md), [import provenance](Docs/EXPERIMENT_IMPORT.json), and [initial validation](Docs/validation/initial-core.json).

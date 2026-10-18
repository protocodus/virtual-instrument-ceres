# Ceres

Ceres is a four-voice, sample-free CS-50 research synthesizer core. This repository owns the reusable C++17 DSP. The Reason Rack Extension consumes it through a pinned `Instrument` Git submodule; host code, panel assets and Rack patches belong to that separate project.

Product version **0.1.0** imports the canonical **v0.10.0 research model**. The core also implements the global ring modulator, five internal SUB waveforms, SUSTAIN I/II key-voltage behavior, shared brilliance/resonance offsets and a separate common VCA level, with an embedded build interface. This is a reduced circuit model with provisional component/control calibration, not a verified transistor-level recreation or a completed hardware match. No hardware recordings, manuals or sampled instrument data are required at runtime.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

The build creates `ceres` and `ceres_embedded` static libraries, the standalone `ceres_render`, and native tests. CTest includes 26 existing DSP groups plus five sound-path/utility-gain groups, a 64-frame embedded allocation contract for all 56 keys, and two exact native/embedded audio comparisons. Python 3 is required for the parity checks.

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

## Ring, SUB and SUSTAIN controls

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `ring_mix` | 0..1 | 0 | Global dry/wet ring blend |
| `ring_rate` | 0.2..220 Hz | 30 | Base free-running carrier frequency |
| `ring_depth` | 0..8 octaves | 0 | Positive carrier-frequency envelope excursion |
| `ring_attack` | 0.001..10 s | 0.01 | Frequency-envelope time to peak |
| `ring_decay` | 0.01..10 s T60 | 0.35 | Frequency-envelope decay toward the base rate |
| `lfo_waveform` | 0..4 | 0 | Sine, Down, Up, Square, Noise |
| `sustain_mode` | 1..2 | 1 | I holds released key voltage; II follows the last assigned key |

The ring envelope triggers when the first physical key is pressed after all keys have been released. Additional held keys do not retrigger it, including when four-voice stealing occurs. Final key-up returns its frequency envelope toward zero through a fixed nominal release; the carrier phase keeps running. The total carrier is bounded at 220 Hz, so SPEED and DEPTH interact near that limit. This is a circuit-informed balanced-multiplier model with provisional envelope and wet-gain calibration.

The selected SUB waveform drives VCO, VCF, VCA and pressure vibrato. Noise uses the documented 22k/0.1µF low-pass and a fixed engineering level; it is independent of SUB rate and audio Noise mix. PWM retains its separate sine oscillator. No external modulation input is implemented.

In Sustain II, released/free boards follow the last assigned key in both oscillator pitch and filter tracking. Their envelope charge, gates, oscillator phase and filter state remain continuous. Entering II applies this immediately; returning to I captures the current key voltage. MIDI-damper-held boards remain protected until pedal-up, an explicit software adaptation rather than recovered Yamaha allocation behavior. This selector is distinct from `global_release` and MIDI damper gating.

The ring/SUB/Sustain additions preserved the earlier audio path in their recorded same-build regression. The subsequent unequal SUB filter route intentionally changes nonzero `filter_lfo` patches. Controls and circuit uncertainties are detailed in [DSP_SOURCE.md](Docs/DSP_SOURCE.md).

## Global sound-path controls

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `brilliance` | −15..+15 V | 0 | Loaded shared BR-node offset; different HP/LP cutoff gains |
| `resonance_offset` | −10..+10 V | 0 | Shared RES-node voltage change; positive lowers Q, with sourced 1.1-second RC response |
| `amp_level` | 0..1 | 1 | Common filtered+sine VCA level before amp EG, bus and ring |
| `output_trim_db` | −24..+24 dB | 0 | Utility gain after all circuit processing and FIR, for saved preset calibration |

The first two controls use an explicitly anchored stiff-source model, not recovered passive-pot positions. The signed resonance offset is an engineering delta around existing effective Q settings; the actual hardware wiper is 0..10 V. Its capacitor state survives note/preset changes and audio panic. SUB filter depth remains the LP depth; HP now gets approximately 0.3374 times as much modulation. Common VCA level adds to the existing SUB-amplitude control and closes at zero, rather than multiplying two gain controls. Factory LEVEL now drives that common VCA instead of final master volume.

Neutral offsets, common level 1 and output trim 0 preserve the prior paths when the intentionally corrected SUB filter route is inactive. The output trim is a modern linear utility, not part of the analog model; it adds no limiter and cannot promise a fixed peak under arbitrary MIDI. The [source audit](Docs/sound-path-gap-audit.md) distinguishes known resistor/capacitor relationships from provisional control calibration.

## Standalone render

```sh
./build/ceres_render --events Examples/init.events.tsv --preset init \
  --rate 48000 --frames 288000 --seed 50 --output /tmp/ceres-init.wav
```

Events are sorted `frame type note value` rows; supported types are `on`, `off`, `bend` (semitones), `pressure`, `sustain`, `alloff`, `panic` and `reset`. Values for note-on velocity/pressure/pedal are normalized to 0..1. The output is mono float32 WAV with no normalization. `--patch path` applies `key=value` overrides to the selected native preset. `--build-info` prints the compiled source identity.

The native research preset table is retained for regression and is excluded from the embedded target. Its presence does not establish a calibrated hardware preset bank or require that the Rack product expose it.

See [DSP source and model limits](Docs/DSP_SOURCE.md), [import provenance](Docs/EXPERIMENT_IMPORT.json), [initial validation](Docs/validation/initial-core.json), historical [panel-control validation](Docs/validation/panel-controls.json), and [sound-path implementation validation](Tests/Reports/soundpath/validation.json). The latter binds successful release and ASAN/UBSAN CTest logs to the compiled source; after both builds/tests, regenerate it with `python3 Tests/record_sound_path_validation.py` (or pass the build-directory options if using different paths).

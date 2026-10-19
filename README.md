# Ceres

Ceres is a four-voice, sample-free Yamaha CS-50 research synthesizer with a reusable C++17 circuit engine. This repository also contains the existing JUCE VST3/AU/CLAP/Standalone scaffold: that plug-in still uses its separate silent placeholder engine and is **not yet connected** to the circuit synthesizer. The Reason Rack Extension consumes the shared DSP through a pinned submodule in its separate adapter repository.

![Ceres editor scaffold](Docs/screenshots/ceres.png)

## Audio demos

The CI-generated section below belongs to the JUCE scaffold. Its factory-preset list is empty, so that demo renderer and `Ceres.AudioDemos` currently fail by design. The standalone circuit engine can render procedural audio with `ceres_render`; its research preset table is not a calibrated hardware bank or a JUCE plug-in preset bank.

<!-- audio-demos-begin -->
<!-- audio-demos-end -->

## Version history

### Unreleased — 0.1.0 — circuit engine and existing plug-in scaffold

- Adds the tested four-voice oscillator/filter/envelope/ring model and embedded interface alongside the existing plug-in scaffold.
- Retains the source-timed VCO conditioning, Q/idle-envelope capacitor continuity and physically scaled ring switching described in the [audible-circuit review](Docs/audible-circuits/README.md). The hardware-recording comparison remains mixed; this is not a verified full hardware match.

### Unreleased — 0.0.1 — scaffold

- Repository, silent plug-in front end, test tiers and CI set up to the portfolio standard.

## Downloads

<!-- build-download-begin -->
<!-- build-download-end -->

---

## How it works

`Source/DSP/ceres.cpp` and `ceres.hpp` implement the standalone circuit engine and its sample-by-sample API. `Source/DSP/CeresEngine.*` remains the distinct silent placeholder used by `Source/Plugin*`; this merge does not silently change the plug-in's sound, controls or saved-state contract. The native and embedded circuit libraries do not depend on JUCE, samples, hardware recordings or manuals at runtime.

Product version 0.1.0 imports the canonical v0.10.0 research model and subsequent documented circuit changes. It is a reduced circuit model with provisional control calibration, not a transistor-exact recreation. No existing historical validation report is relabelled as a validation of this merged build.

### Use the standalone core


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

### Ring, SUB and SUSTAIN controls

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

### Global sound-path controls

| Key | Range | Default | Meaning |
| --- | --- | --- | --- |
| `brilliance` | −15..+15 V | 0 | Loaded shared BR-node offset; different HP/LP cutoff gains |
| `resonance_offset` | −10..+10 V | 0 | Shared RES-node voltage change; positive lowers Q, with sourced 1.1-second RC response |
| `amp_level` | 0..1 | 1 | Common filtered+sine VCA level before amp EG, bus and ring |
| `output_trim_db` | −24..+24 dB | 0 | Utility gain after all circuit processing and FIR, for saved preset calibration |

The first two controls use an explicitly anchored stiff-source model, not recovered passive-pot positions. The signed resonance offset is an engineering delta around existing effective Q settings; the actual hardware wiper is 0..10 V. Its capacitor state survives note/preset changes and audio panic. SUB filter depth remains the LP depth; HP now gets approximately 0.3374 times as much modulation. Common VCA level adds to the existing SUB-amplitude control and closes at zero, rather than multiplying two gain controls. Factory LEVEL now drives that common VCA instead of final master volume.

Neutral offsets, common level 1 and output trim 0 preserve the prior paths when the intentionally corrected SUB filter route is inactive. The output trim is a modern linear utility, not part of the analog model; it adds no limiter and cannot promise a fixed peak under arbitrary MIDI. The [source audit](Docs/sound-path-gap-audit.md) distinguishes known resistor/capacitor relationships from provisional control calibration.

### Velocity expression

`expression_source` selects **0 = channel aftertouch** (the legacy default) or **1 = note-on velocity**. Values clamp to 0..1 and round to the nearest integer. The existing `pressure_volume`, `pressure_filter` and `pressure_vibrato` depths then use the selected source. This optional velocity mode is a modern DAW mapping, not a claim about the original hardware.

Velocity is latched independently for each allocated voice and stays with its release tail, including during Sustain II pitch following. A new note assignment, repeated-note retrigger or steal stores the incoming velocity only on that board. Channel pressure has no audible effect in velocity mode; switching back uses its continuously smoothed history. Live source changes preserve oscillator/envelope/filter state and may change the expression gain immediately.

Level gain is `1 − pressure_volume*(1 − expression)`; filter expression adds `expression*pressure_filter` octaves to each filter's variable conductance above its fixed resistor floor; vibrato adds `expression*pressure_vibrato` cents of the selected SUB waveform. The separate native `velocity_sensitivity` gain remains multiplicative and backward-compatible: leave it at zero when using only the selected expression level depth. All native research presets retain source 0. Hosts should default missing saved properties to 0 to preserve old patches, and explicitly save 1 for velocity-oriented patches.


## Known gaps

- The JUCE plug-in remains a silent placeholder with a plain editor; connecting the actual instrument and controls is separate work. Its source, identity codes, release helpers and shared portfolio tools are preserved.
- The plug-in's empty preset list makes its existing audio/demo tier fail. The full suite and CI must not be described as green while this remains unresolved.
- Factory control-voltage calibration, source impedances, some chip loading/nonlinearities and absolute ring wet gain remain uncertain. See [DSP source and model limits](Docs/DSP_SOURCE.md) and the [audible circuit evidence](Docs/audible-circuits/README.md).
- Historical research reports bind their original sources and may reference external working directories or source recordings. They are retained provenance, not portable current-CI results.
- No external modulation input is implemented. Optional velocity and MIDI-pedal behavior are explicitly modern adaptations.

## Build

CMake 3.22+, a C++17/20 compiler and Python3 are required for the full unit suite. The circuit targets explicitly compile as C++17; the existing JUCE scaffold uses C++20. Build the standalone DSP without JUCE:

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp -L '^unit$' --output-on-failure
./build-dsp/ceres_render --events Examples/init.events.tsv --preset init \
  --rate 48000 --frames 288000 --seed 50 --output /tmp/ceres-init.wav
```

`ceres` and `ceres_embedded` are the circuit libraries. `ceres_render` writes mono float32 WAV without normalization; sorted event rows are `frame type note value`. It supports note-on/off, bend, pressure, pedal, all-notes-off, panic and reset. `--patch path` reads key=value overrides and `--build-info` reports the exact compiled sources including the merged CMake file. The native research preset API is excluded from the embedded ABI.

The 12 circuit checks cover oscillator/filter/envelope/ring contracts, expression, all 57 keys with zero post-construction allocations, and native/embedded parity. They are named `Ceres.<Suite>` and labelled `unit` alongside the existing scaffold unit tests. Test executables are resolved by CMake target paths for single- and multi-configuration generators. Existing `audio`, `render` and `fuzz` tiers remain separate and unmodified.

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_BUILD_PLUGIN=OFF -DCERES_SANITIZE=ON
cmake --build build-sanitize --parallel
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize -L '^unit$' --output-on-failure
```

`CERES_SANITIZE` is a compatibility alias for the existing `CERES_ENABLE_SANITIZERS`; either instruments development targets with ASan/UBSan. `CERES_BUILD_TOOLS=OFF` excludes both renderer tools without dropping core tests. `CERES_BUILD_LIBFUZZER` retains the existing scaffold fuzz target.

The separate plug-in build fetches pinned JUCE 8.0.14 and clap-juce-extensions unless `CERES_JUCE_PATH` and `CERES_CLAP_JUCE_EXTENSIONS_PATH` point to local checkouts. `CERES_BUILD_CLAP` controls the CLAP wrapper; on macOS `CERES_BUILD_UNIVERSAL` defaults to ON. Linux dependencies are listed in `.github/workflows/ci.yml`.

```sh
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=ON -DBUILD_TESTING=ON
cmake --build build-plugin --parallel
ctest --test-dir build-plugin -L '^unit$' --output-on-failure
# Full suite additionally includes the known pre-existing empty-bank failure:
ctest --test-dir build-plugin --output-on-failure
```

On multi-configuration generators add `--config Release` to the build and `-C Release` to CTest; command-line executables are then in the corresponding configuration directory. `scripts/build-macos.sh` retains the existing universal build and full-suite behavior.

## Licensing

Ceres's source code is MIT-licensed; see [LICENSE](LICENSE). The plug-in builds with JUCE (AGPLv3 or a commercial JUCE licence) and clap-juce-extensions; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No Reason/Jukebox SDK, third-party hardware capture, firmware or ROM data is part of the standalone runtime.

# Ceres

Ceres is a sample-free Yamaha CS-50 research synthesizer with a reusable C++17 circuit engine and configurable 1–16 voice polyphony. This repository also contains the existing JUCE VST3/AU/CLAP/Standalone scaffold: that plug-in still uses its separate silent placeholder engine and is **not yet connected** to the circuit synthesizer. The Reason Rack Extension consumes the shared DSP through a pinned submodule in its separate adapter repository.

![Ceres editor scaffold](Docs/screenshots/ceres.png)

## Audio demos

The CI-generated demos below use the **standalone circuit engine's existing 14 research presets**, including Init, without preset edits, gain normalization or external audio. They do not demonstrate the still-silent JUCE plug-in, which has no instrument preset bank yet. The source phrases are original committed MIDI, not recovered hardware performances; names inherited from the research table do not establish calibrated Yamaha preset matches.

<!-- audio-demos-begin -->
<!-- audio-demos-end -->

## Version history

### Unreleased — 0.2.0 — configurable polyphony and processing quality

- Adds 1–16 preallocated voices and actual 1×, 2× or 4× internal processing. The standalone default remains four voices at 4×.
- Smooths live Ring Depth edits over 20 ms without restarting the carrier or envelope; saved/static depths start immediately at their configured value.
- Keeps audio-noise density consistent across 1×/2×/4× at the same host sample rate, preserving the original 4× level and the separate SUB-noise path.
- Keeps the existing four-voice, 4× audio byte-identical in the recorded parent comparison. Configuration changes require an unclocked/reset engine; adapters can defer them until idle and fade residual tails.
- Retains nominal 31-host-sample output latency at all qualities. Lower quality trades oversampling headroom and alias rejection for CPU cost; it is not the same sound under a new label.

### Unreleased — 0.1.0 — circuit engine and existing plug-in scaffold

- Adds the tested four-voice oscillator/filter/envelope/ring model and embedded interface alongside the existing plug-in scaffold.
- The README demo renderer now exercises the existing circuit research presets with fixed original MIDI; the separate JUCE plug-in remains unchanged.
- Retains the source-timed VCO conditioning, Q/idle-envelope capacitor continuity and physically scaled ring switching described in the [audible-circuit review](Docs/audible-circuits/README.md). The hardware-recording comparison remains mixed; this is not a verified full hardware match.

### Unreleased — 0.0.1 — scaffold

- Repository, silent plug-in front end, test tiers and CI set up to the portfolio standard.

## Downloads

<!-- build-download-begin -->
<!-- build-download-end -->

---

## How it works

`Source/DSP/ceres.cpp` and `ceres.hpp` implement the standalone circuit engine and its sample-by-sample API. `Source/DSP/CeresEngine.*` remains the distinct silent placeholder used by `Source/Plugin*`; this merge does not silently change the plug-in's sound, controls or saved-state contract. The native and embedded circuit libraries do not depend on JUCE, samples, hardware recordings or manuals at runtime.

The original product version 0.1.0 imported the canonical v0.10.0 research model and subsequent documented circuit changes. It is a reduced circuit model with provisional control calibration, not a transistor-exact recreation. No existing historical validation report is relabelled as a validation of this merged build.

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

The embedded interface has a numeric `Patch`, a C-string parameter setter, no exception path, and no allocating offline probes/preset APIs. Construction allocates the synth state. Rendering, controller changes, validated patch updates, panic, reset and configuration reuse existing state. Do not construct or replace a synth inside `RenderBatch`.

The output is mono. All configured voices and their capacitor states continue running when no note is held. `activeVoices()==0` does **not** imply silence: the common output bus has a roughly 0.888-second time constant. `allNotesOff()` honors the MIDI pedal; `allSoundOff()` clears audio states while preserving controllers; `reset()` also restores seeded oscillator/controller state. The FIR has nominal group delay 31 host samples, with the decimator's sampling phase affecting event alignment.

### Polyphony and processing quality

`configure(voices, factor)` accepts 1–16 voices and factors 1, 2 or 4. It returns `false` without mutation for invalid values, after processing has started, or when notes are already assigned before the first sample. A request equal to the current configuration is a no-op. `polyphony()` and `oversamplingFactor()` report the applied configuration; the old static constants describe the four-voice, 4× defaults.

Configure before sending notes, or explicitly reset first, then reapply controllers. A changed configuration resets capacitor, oscillator and controller state while retaining the patch. A live adapter should wait for physical keys, pedal and active envelopes to be released, fade the residual tail, reset/configure, restore patch/controllers and fade back in. Panic alone does not reopen configuration. This API does not silently cut held notes or allocate new voices on the audio thread.

The 4× path retains its original 249-tap FIR. The 2× path has a 125-tap FIR with the same nominal group delay; 1× uses an exact 31-sample delay and no redundant low-pass. Final-of-factor sampling retains the legacy fractional decimation phase, so the modes do not promise identical transients. All oscillators, filters, envelopes and circuit states actually run at the selected internal rate. The existing frequency guards operate at that rate, so lower qualities can also limit extreme settings earlier.

Additional voices are a virtual extension of the four-board model. The calibrated common bus and per-voice gain remain unchanged; no voice-count normalization or limiter is added. Larger chords can increase rail saturation and output level. Extra voices use a separate initialization stream; at the default four-voice setting the original shared noise/drift stream remains exact. See [configuration validation](Docs/validation/poly-quality/README.md).

Audio noise scales by `sqrt(factor/4)` before the waveform mixer. This corrects the increased in-band noise density that unscaled random samples would produce at lower internal rates. The 4× source is unchanged, and SUB noise retains its own RC normalization. Different quality settings can still differ through filter discretization, nonlinear processing and alias rejection; this is not hardware gain calibration. See [noise-quality validation](Docs/validation/noise-quality/README.md).

The native renderer accepts `--voices 1..16 --oversampling 1|2|4`, with defaults `4` and `4`. Host adapters may choose and save different defaults independently of the 57 hardware/engineering patch keys.

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

Live `ring_depth` changes interpolate over 20 ms in the digital control domain. This dezipper preserves the existing exponential frequency law and 220 Hz cap; it is not a newly inferred analog time constant. Startup/reset use the saved depth immediately. Repeated notifications at the same target do not prolong the transition, and a reversal starts from the current interpolated value. The change removes the immediate frequency-control step in a synthetic test; it does not prove the exact cause of a reported host crackle.

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
- The plug-in still has no preset bank. The audio tier checks the separately identified circuit research presets; passing it does not establish audible JUCE plug-in functionality.
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

The 12 circuit checks cover oscillator/filter/envelope/ring contracts, expression, all 57 keys with zero post-construction allocations, and native/embedded parity. They are named `Ceres.<Suite>` and labelled `unit` alongside the existing scaffold unit tests. Test executables are resolved by CMake target paths for single- and multi-configuration generators. The `audio`, `render` and `fuzz` tiers remain separate; the unchanged audio contract now renders the actual circuit preset list twice.

```sh
ctest --test-dir build-dsp -L '^audio$' --output-on-failure
./build-dsp/RenderCeresPresetDemos --jobs 4 Docs/audio/midi build-dsp/demo-audio
```

The demo tool uses the shared fixed 48 kHz/256-frame/seed protocol and the original [MIDI phrase definitions](Docs/audio/midi/README.md). It duplicates the mono engine into two identical channels without gain changes. Every 12 s raw render must be finite, audible, below 0 dBFS and identical on repetition. MP3 export uses ffmpeg; no captured sound or preset-specific level adjustment is involved. Historical empty-bank failure evidence remains frozen in the [initial integration report](Docs/validation/main-integration-2026-10-19/report.json); the subsequent [demo integration validation](Docs/validation/research-demos-2026-10-19/report.json) records the resolved audio path and its new build identity.

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
# Full suite also includes circuit audio, editor rendering and fuzz tiers:
ctest --test-dir build-plugin --output-on-failure
```

On multi-configuration generators add `--config Release` to the build and `-C Release` to CTest; command-line executables are then in the corresponding configuration directory. `scripts/build-macos.sh` retains the existing universal build and full-suite behavior.

## Licensing

Ceres's source code is MIT-licensed; see [LICENSE](LICENSE). The plug-in builds with JUCE (AGPLv3 or a commercial JUCE licence) and clap-juce-extensions; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No Reason/Jukebox SDK, third-party hardware capture, firmware or ROM data is part of the standalone runtime.

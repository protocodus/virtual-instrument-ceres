# Canonical DSP source

`Source/DSP/ceres.cpp` and `Source/DSP/ceres.hpp` are the only production DSP implementation. A host should pin this repository as a Git submodule and compile these files directly. Do not maintain a copied Rack-specific oscillator, filter or envelope implementation. The Rack project records the gitlink revision, embedded compile definition and compiled source inventory in its own release manifest.

The initial import is the tested canonical v0.10.0 model from `/tmp/ceres-cs50-1x8LGd`, recorded by original-file SHA256 in `EXPERIMENT_IMPORT.json`. The new product's version starts at 0.1.0. The experiment's isolated amplitude-bias, shared LP-bus and alternate decay candidates are deliberately absent. The temporary source is a provenance location, not a build dependency. Hardware recordings, downloaded schematics, fitting scripts and the large experiment archive were not imported.

## Embedded adaptation

`CERES_EMBEDDED_TARGET=1` removes `Patch::name`, native preset lookup and allocating measurement APIs. It adds no alternate sound model. The shared C-string and native string parameter setters accept the same numeric keys/ranges. The seven added panel controls extend the imported API; enum-like waveform/mode values clamp then round to the nearest declared integer. Invalid embedded construction rates fall back to 48 kHz; native construction retains its exception. Supported construction rates are finite 8000..384000 Hz. A host should validate its sample-rate property before construction.

The embedded library is compiled with `-fno-exceptions` in the native validation build. Its only owned heap allocation occurs during `Synth` construction; reset, panic, patch changes and processing reuse it. `Patch` is trivially copyable in this configuration. Native and embedded `Patch` layouts differ and must not be linked interchangeably. No filesystem, threading, SDK, plugin GUI, sample playback or host framework is used by the embedded translation unit.

The allocation witness interposes `new`/`new[]` around 512 blocks of 64 samples with parameter/controller changes and lifecycle events. It is a bounded witness, not exhaustive host testing or a blanket interception of every platform allocator. Source inspection finds no direct realtime `malloc`/allocation calls. The separate Rack wrapper must establish its own property/event and buffer contracts.

## Model and parameter boundaries

The model has four free-running oscillator boards, finite oscillator reset, WSC waveform approximations, cascaded nonlinear two-integrator HP/LP filters, capacitor envelopes, per-voice/common output coupling, four-times oversampling and a 249-tap FIR decimator. Its normalized circuit voltages, nonlinear scales and several raw panel/preset-CV conversions remain provisional. The full research archive contains public-recording diagnostics, but those did not establish exact hardware calibration, complete original MIDI/gates or a universal fit.

Filter frequency parameters specify the total small-signal integrator frequency at MIDI 60; a fixed 4.7-Mohm feed contribution remains while keyboard/envelope controls scale only variable conductance. Master `tune_cents` affects the oscillator and both filter keyboard paths; `octave` affects oscillator footage only. `key_track` is an engineering control, not proof of an alternative hardware law. Filter initial/peak levels and HP/LP envelope scales retain their explicit engineering units.

Engineering attack times mean initial-to-peak duration; decay/release mean T60 relative to the target. The native hardware table uses a separately documented, related-model five-RC conversion. `global_release` is an ideal shared minimum release-time override, distinct from MIDI damper gating and from the CS-50 SUSTAIN I/II keyboard-CV modes. Optional attack headroom/current-limit controls are research parameters, not identified chip controls.

Output coupling uses the documented topology with a low-input-impedance VCA limit, fully conducting expression element and unloaded HIGH output assumptions. Downstream branch calibration and recording/interface loading remain uncertain. The direct sine bypass, absolute drive, Q/control calibration and oscillator reset duration must not be advertised as measured final values.

The model is deterministic within a given floating-point/compiler/math-library build. Cross-platform bit identity is not promised. No fast-math flag or silent oversampling reduction was introduced. Constructor-created FIR coefficients are built outside realtime processing. The bounded native tests do not establish a Reason target deadline: the four-voice nonlinear solver and decimator need separate 64-frame wrapper/SDK/host performance measurement.

## Added panel-control model

The default settings preserve the imported audio path: ring mix 0, SUB sine, Sustain I. The ring bypass returns the previous bus value directly. New noise state consumes the existing shared white sample without another random draw, so oscillator initialization, noise and drift sequences remain unchanged. Native-only `voiceState()` and `modulationState()` return read-only diagnostics; they are absent from the embedded ABI.

**Ring.** The Yamaha servicing guide and direct CS-50 PRA/Overall sheets put one ring processor after the voice sum, before expression/output. Its independent sine carrier multiplies the bus through an ideal balanced-product approximation to the µA796 topology. The wet branch includes the direct-sheet 10µF/10k output coupling (tau 0.1 s); the guide shows a conflicting capacitor value. Unknown input/carrier coupling loads, transistor distortion, carrier leakage and the wet gain trim are not identified. Unity normalized wet gain and linear dry/wet mixing remain engineering choices.

The carrier is free-running. An aggregate physical-key ledger, independent of voice stealing and MIDI damper, triggers the frequency envelope only at all-keys-up → any-key-down, consistent with the owner's manual printed page 15. Additional key presses are legato. Final physical key-up interrupts the attack/decay and selects a fixed release. Direct CS-50 PRA (service2 PDF16/printed22) feeds RT through 6.8k from +15V and 3.3k to ground: the unloaded 4.9009901 V, interpreted through the existing provisional five-RC law, gives **0.46781044 s T60**. The ring's different external timing path prevents treating that inference as calibration. The envelope uses a 1% attack-target headroom, retains charge on retrigger/live edits and is independent of `global_release`. Attack is time to peak; decay is T60. Depth is a positive octave excursion, not recovered slider volts. The guide's approximate 0.2–220 Hz carrier range bounds the total frequency including the envelope; this is a bounded interpretation, not a measured analog clipping law.

**SUB.** Sine, down saw, up saw and square share the free-running SUB clock. Analytic BLEP corrections soften discontinuities; waveform phase/polarity alignment is an engineering convention. All existing SUB destinations, including pressure vibrato, use this selected source. The independent PWM sine clock is unchanged. Direct PN3 selects SUB-N; the guide printed page 32 shows a **22k/0.1µF low-pass** (unloaded corner 72.343 Hz), distinct from audio NO level. Although the guide calls this source pink noise and the owner's text calls it white noise, the implementation follows that one-pole circuit rather than an ideal 1/f generator. The existing procedural white sample feeds the trapezoidal RC. A fixed sample-rate-derived normalization targets 0.25 RMS before a static tanh bound; the limiter's actual noise statistics/bandwidth and modulation level remain unknown. There is no block normalization, gain follower, key sync or rate-dependent sample-and-hold.

**Sustain I/II.** The guide printed pages 25–26 describes held key voltages in I and open sample/hold gates in II. The model stores note ownership separately from sounding key voltage. In II, nonheld/nonpedal boards mirror `lastAssignedKey` on assignments, key release, pedal-up, all-notes-off and a live I→II switch. II→I captures the current key voltage. Both VCO and HP/LP keyboard paths use it, while oscillator phase, filter capacitors and amp/filter envelope state and gates remain unchanged. Four-voice allocation still uses the existing idle/quietest-release/oldest-held software policy; exact Yamaha KAS scan timing is not emulated. Protecting MIDI-damper-held boards is an explicit modern adaptation because the original instrument's pedal path was expression. Ring's physical-key ledger ignores that damper protection. Panic clears voices and ring envelope/coupling while free oscillator phases continue; reset restores all seeded phases/controllers/states.

Validation covers independent RC attack/release equations, ring sidebands and dry/partial/wet signal connection, free-running carrier, more than four physically held keys, repeated presses, pedal rearm, sourced SUB-noise smoothing and route independence, and Sustain II's shared oscillator/filter pitch routing with continuous state. The existing allocation witness now exercises 52 numeric keys. Separate 384,000-sample native/embedded streams exercise the new controls as well as the unchanged-default import comparison. These are implementation checks, not hardware sound-match measurements.

## Reproduce the import comparison

```sh
python3 Tools/check_embedded.py --build-dir build \
  --baseline-source /tmp/ceres-cs50-1x8LGd/src \
  --report Docs/validation/panel-default-parity.json
```

The optional original-source comparison first verifies the two original hashes, then compiles the same driver against the imported experiment, new native core and new embedded core. Each renders 384,000 samples across four rates and eight scenes. It includes repeated notes, stealing, live controls, sustain, panic and reset. Without `--baseline-source`, the tool compares the two local variants and requires no temporary experiment files. The exact original/native/embedded streams matched in the initial import validation.

Run the same native/embedded driver with the new controls enabled:

```sh
./build/ceres_native_parity /tmp/ceres-panel-native.f32 controls
./build/ceres_embedded_parity /tmp/ceres-panel-embedded.f32 controls
cmp /tmp/ceres-panel-native.f32 /tmp/ceres-panel-embedded.f32
```

Each file contains 384,000 float32 samples. The `controls` argument enables ring blend/rate/depth/A/D, all five SUB shapes and both Sustain modes across the same four rates and allocation/pedal/reset sequences. The recorded enabled-control comparison is [panel-enabled-parity.json](validation/panel-enabled-parity.json); the current unchanged-default comparison is [panel-default-parity.json](validation/panel-default-parity.json). Initial-import reports remain historical evidence and are not overwritten.

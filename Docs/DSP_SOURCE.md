# Canonical DSP source

`Source/DSP/ceres.cpp` and `Source/DSP/ceres.hpp` are the only production DSP implementation. A host should pin this repository as a Git submodule and compile these files directly. Do not maintain a copied Rack-specific oscillator, filter or envelope implementation. The Rack project records the gitlink revision, embedded compile definition and compiled source inventory in its own release manifest.

The initial import is the tested canonical v0.10.0 model from `/tmp/ceres-cs50-1x8LGd`, recorded by original-file SHA256 in `EXPERIMENT_IMPORT.json`. The new product's version starts at 0.1.0. The experiment's isolated amplitude-bias, shared LP-bus and alternate decay candidates are deliberately absent. The temporary source is a provenance location, not a build dependency. Hardware recordings, downloaded schematics, fitting scripts and the large experiment archive were not imported.

## Embedded adaptation

`CERES_EMBEDDED_TARGET=1` removes `Patch::name`, native preset lookup and allocating measurement APIs. It adds no alternate sound model. The shared C-string parameter setter accepts the same numeric keys/ranges as the original native string API. Invalid embedded construction rates fall back to 48 kHz; native construction retains its exception. Supported construction rates are finite 8000..384000 Hz. A host should validate its sample-rate property before construction.

The embedded library is compiled with `-fno-exceptions` in the native validation build. Its only owned heap allocation occurs during `Synth` construction; reset, panic, patch changes and processing reuse it. `Patch` is trivially copyable in this configuration. Native and embedded `Patch` layouts differ and must not be linked interchangeably. No filesystem, threading, SDK, plugin GUI, sample playback or host framework is used by the embedded translation unit.

The allocation witness interposes `new`/`new[]` around 512 blocks of 64 samples with parameter/controller changes and lifecycle events. It is a bounded witness, not exhaustive host testing or a blanket interception of every platform allocator. Source inspection finds no direct realtime `malloc`/allocation calls. The separate Rack wrapper must establish its own property/event and buffer contracts.

## Model and parameter boundaries

The model has four free-running oscillator boards, finite oscillator reset, WSC waveform approximations, cascaded nonlinear two-integrator HP/LP filters, capacitor envelopes, per-voice/common output coupling, four-times oversampling and a 249-tap FIR decimator. Its normalized circuit voltages, nonlinear scales and several raw panel/preset-CV conversions remain provisional. The full research archive contains public-recording diagnostics, but those did not establish exact hardware calibration, complete original MIDI/gates or a universal fit.

Filter frequency parameters specify the total small-signal integrator frequency at MIDI 60; a fixed 4.7-Mohm feed contribution remains while keyboard/envelope controls scale only variable conductance. Master `tune_cents` affects the oscillator and both filter keyboard paths; `octave` affects oscillator footage only. `key_track` is an engineering control, not proof of an alternative hardware law. Filter initial/peak levels and HP/LP envelope scales retain their explicit engineering units.

Engineering attack times mean initial-to-peak duration; decay/release mean T60 relative to the target. The native hardware table uses a separately documented, related-model five-RC conversion. `global_release` is an ideal shared minimum release-time override, distinct from MIDI damper gating and from the unimplemented CS-50 SUSTAIN I/II keyboard-CV modes. Optional attack headroom/current-limit controls are research parameters, not identified chip controls.

Output coupling uses the documented topology with a low-input-impedance VCA limit, fully conducting expression element and unloaded HIGH output assumptions. Downstream branch calibration and recording/interface loading remain uncertain. The direct sine bypass, absolute drive, Q/control calibration and oscillator reset duration must not be advertised as measured final values.

The model is deterministic within a given floating-point/compiler/math-library build. Cross-platform bit identity is not promised. No fast-math flag or silent oversampling reduction was introduced. Constructor-created FIR coefficients are built outside realtime processing. The bounded native tests do not establish a Reason target deadline: the four-voice nonlinear solver and decimator need separate 64-frame wrapper/SDK/host performance measurement.

## Reproduce the import comparison

```sh
python3 Tools/check_embedded.py --build-dir build \
  --baseline-source /tmp/ceres-cs50-1x8LGd/src \
  --report Docs/validation/initial-parity.json
```

The optional original-source comparison first verifies the two original hashes, then compiles the same driver against the imported experiment, new native core and new embedded core. Each renders 384,000 samples across four rates and eight scenes. It includes repeated notes, stealing, live controls, sustain, panic and reset. Without `--baseline-source`, the tool compares the two local variants and requires no temporary experiment files. The exact original/native/embedded streams matched in the initial import validation.

# Configurable polyphony and processing quality

The standalone 0.2.0 core adds 1–16 preallocated voices and actual 1×/2×/4× internal processing. This is a software resource/quality extension, not a newly inferred hardware circuit. No research preset, oscillator, filter topology, envelope mapping or ring frequency law changed. The final integration additionally smooths live Ring Depth targets over 20 ms, as a digital control interpolation rather than a hardware time constant. The embedded `Patch` still has 57 numeric keys; configuration belongs to the host adapter separately.

`configure(voices, factor)` accepts valid changed settings only before processing and before note assignment. It resets state/controllers and retains the patch. Invalid/live calls are atomic rejections; an identical request is a no-op. `reset()` retains configuration; `allSoundOff()` preserves clock history and does not authorize a quality change. Hosts can defer until keys/pedal/envelopes are released, fade residual tails, reset/configure/restore controllers, then fade in. No such fade or idle policy is silently embedded in the circuit model.

The first four voices keep the exact parent initialization/noise/drift sequence. Extra slots use a separate initial seed stream and remain unclocked outside the configured voice limit. Once enabled, their drift draws join the shared stream; different polyphony settings are not promised identical audio. Per-voice gain and the four-board common-bus calibration remain unchanged; larger chords can drive more saturation. The legacy per-index voice-spread rule also remains unchanged.

The output filter uses 249 taps at 4×,125 taps at 2× and an exact 31-sample delay at 1×. All have nominal 31-host-sample group delay. The original final-of-factor decimation phase remains: for an impulse on the first internal subframe, fractional host offsets are 30.25 at 4× and 30.5 at 2×. Reported nominal latency stays31, but these modes do not promise identical transient timing. The sample-domain latency test reconstructs every polyphase branch and checks total DC gain, centroid, symmetry, finite support and stopband/passband response. No post-hoc delay adjustment changes the old4× sound.

## Ring Depth review

The finite 20 ms interpolator acts only on live depth-target changes. Initial configuration and reset precharge the target exactly; equal-target updates do not restart interpolation. Reversal starts at the current value, and panic preserves the control ramp. The existing frequency cap, exponential law, ring envelope and free-running carrier remain unchanged. Independent elapsed-time/reversal tests retain exact endpoint requirements; intermediate tolerance is bounded by internal-step count × 8 × double epsilon for accumulated additions. Native/embedded and allocation scenes include live changes, repeated targets and reversals.

This removes a source-demonstrated abrupt frequency-control change. No recording of the user's exact crackle was available, so the tests do not establish that this was its only cause. The fixed 20 ms span is explicitly modern control dezippering, not a recovered analog component value.

## Verification

- `ConfigurationTests.cpp`: all 1–16 accepted counts, invalid/live/held-note rejection without audio mutation;1/4/16 allocation, FIFO, stealing, pedal, panic and inactive-slot clocks; rate-dependent oscillator and independent Q-RC equations at8–384kHz; output-rate filter response.
- `EmbeddedContract.cpp`: all 57 keys plus 9 voice/rate configurations under the existing post-construction allocation trap, including resets, controllers and active modulation and live Ring Depth interpolation. Zero `new`/`new[]` calls; C++17 and no-exceptions embedded target.
- `ConfigurationParityDriver.cpp`: 184,320 exact native/embedded samples at five host rates, three voice counts and three factors, with ring, live/reversed Depth targets, noise, pressure/velocity, PWM, filter motion, pedal, steals and resets.
- `check_legacy_configuration.py`: 768,000 samples byte-identical to immutable parent `2ee6cf22deead7358d6b4ee7c294da955225af56`, default 4/4, across two old shared-source/control scene sets. Parent sources/binary/audio are rebuilt in temporary storage by the script; hashes are retained, not the temporary bytes.
- Native renderer CLI: nine valid configurations, explicit 4/4 WAV equal to default, seven invalid requests rejected before file creation. Configuration is reported in JSON; no normalization or gain change.
- Full Release CTest includes the existing 14-preset deterministic audio tier and scaffold fuzz test. Targeted ASan/UBSan covers configuration, native DSP, embedded allocation and configured native/embedded parity with leak detection disabled.

Commands (repository root):

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=OFF -DCERES_BUILD_UNIVERSAL=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp --output-on-failure
python3 Tools/check_legacy_configuration.py --candidate build-dsp/ceres_native_parity --report Docs/validation/poly-quality/legacy-parity.json
python3 Tests/check_configuration_parity.py --native build-dsp/ceres_native_configuration_parity --embedded build-dsp/ceres_embedded_configuration_parity --report Docs/validation/poly-quality/configured-parity.json
python3 Tests/check_configuration_cli.py --renderer build-dsp/ceres_render --report Docs/validation/poly-quality/cli.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCERES_BUILD_PLUGIN=OFF -DCERES_BUILD_UNIVERSAL=OFF -DCERES_SANITIZE=ON -DBUILD_TESTING=ON
cmake --build build-sanitize --parallel 2 --target ceres_configuration_tests ceres_ring_depth_tests ceres_embedded_contract ceres_tests ceres_native_configuration_parity ceres_embedded_configuration_parity
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize -R 'configuration_contracts|configuration_native_embedded|ring_depth_continuity|embedded_allocation|native_dsp' --output-on-failure
```

## Bounded CPU observation

`performance.json` is the preserved **pre-Ring-Depth-change** measurement, tied to the exact source in [pre-ring/manifest.json](pre-ring/manifest.json). The new control interpolator was not timed again; this report is not silently rebound to the final source. It records one native embedded-ABI synth at 48 kHz with 64-sample blocks, 128 warmup and 1,000 measured blocks per configuration. Active pulse/PWM/ring/SUB/filter modulation and every configured voice are enabled. There was one run, with no discarded warm repeats or selection. It is not a Rack target/Double/host guarantee.

| Voices | 1× median thread CPU | 2× | 4× |
| --- | ---: | ---: | ---: |
| 1 | .0166ms | .0437ms | .0867ms |
| 4 | .0549ms | .1188ms | .2352ms |
| 16 | .2226ms | .4291ms | .8446ms |

At 16 voices/4×, 64 of 1,000 thread-CPU blocks exceeded the 1.333ms budget, with a 2.131ms maximum. All other configurations had zero thread-CPU overruns in this run. Wall-clock overruns occurred in several cases and remain in the report (152 at 16/4×). Two warm engines in a Double adapter cost additional work, so high-polyphony/high-quality settings can exceed available realtime capacity. Lower quality also changes high-frequency guards and alias behavior; this study makes no listening-preference or hardware-match claim.

```sh
# Run in an idle measurement window, once; retain overruns.
python3 Tools/benchmark_configuration.py --report Docs/validation/poly-quality/performance.json
```

# Configurable polyphony and processing quality

The standalone0.2.0 core adds1–16 preallocated voices and actual1×/2×/4× internal processing. This is a software resource/quality extension, not a newly inferred hardware circuit. No research preset, oscillator, filter topology, envelope mapping or ring law changed. The embedded `Patch` still has57 numeric keys; configuration belongs to the host adapter separately.

`configure(voices, factor)` accepts valid changed settings only before processing and before note assignment. It resets state/controllers and retains the patch. Invalid/live calls are atomic rejections; an identical request is a no-op. `reset()` retains configuration; `allSoundOff()` preserves clock history and does not authorize a quality change. Hosts can defer until keys/pedal/envelopes are released, fade residual tails, reset/configure/restore controllers, then fade in. No such fade or idle policy is silently embedded in the circuit model.

The first four voices keep the exact parent initialization/noise/drift sequence. Extra slots use a separate initial seed stream and remain unclocked outside the configured voice limit. Once enabled, their drift draws join the shared stream; different polyphony settings are not promised identical audio. Per-voice gain and the four-board common-bus calibration remain unchanged; larger chords can drive more saturation. The legacy per-index voice-spread rule also remains unchanged.

The output filter uses249 taps at4×,125 taps at2× and an exact31-sample delay at1×. All have nominal31-host-sample group delay. The original final-of-factor decimation phase remains: for an impulse on the first internal subframe, fractional host offsets are30.25 at4× and30.5 at2×. Reported nominal latency stays31, but these modes do not promise identical transient timing. The sample-domain latency test reconstructs every polyphase branch and checks total DC gain, centroid, symmetry, finite support and stopband/passband response. No post-hoc delay adjustment changes the old4× sound.

## Verification

- `ConfigurationTests.cpp`: all1–16 accepted counts, invalid/live/held-note rejection without audio mutation;1/4/16 allocation, FIFO, stealing, pedal, panic and inactive-slot clocks; rate-dependent oscillator and independent Q-RC equations at8–384kHz; output-rate filter response.
- `EmbeddedContract.cpp`: all57 keys plus9 voice/rate configurations under the existing post-construction allocation trap, including resets, controllers and active modulation. Zero `new`/`new[]` calls; C++17 and no-exceptions embedded target.
- `ConfigurationParityDriver.cpp`:184,320 exact native/embedded samples at five host rates, three voice counts and three factors, with ring, noise, pressure/velocity, PWM, filter motion, pedal, steals and resets.
- `check_legacy_configuration.py`:768,000 samples byte-identical to immutable parent `2ee6cf22deead7358d6b4ee7c294da955225af56`, default4/4, across two old shared-source/control scenes sets. Parent sources/binary/audio are rebuilt in temporary storage by the script; hashes are retained, not the temporary bytes.
- Native renderer CLI: nine valid configurations, explicit4/4 WAV equal to default, seven invalid requests rejected before file creation. Configuration is reported in JSON; no normalization or gain change.
- Full Release CTest includes the existing14-preset deterministic audio tier and scaffold fuzz test. Targeted ASan/UBSan covers configuration, native DSP, embedded allocation and configured native/embedded parity with leak detection disabled.

Commands (repository root):

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=OFF -DCERES_BUILD_UNIVERSAL=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp --output-on-failure
python3 Tools/check_legacy_configuration.py --candidate build-dsp/ceres_native_parity --report Docs/validation/poly-quality/legacy-parity.json
python3 Tests/check_configuration_parity.py --native build-dsp/ceres_native_configuration_parity --embedded build-dsp/ceres_embedded_configuration_parity --report Docs/validation/poly-quality/configured-parity.json
python3 Tests/check_configuration_cli.py --renderer build-dsp/ceres_render --report Docs/validation/poly-quality/cli.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCERES_BUILD_PLUGIN=OFF -DCERES_BUILD_UNIVERSAL=OFF -DCERES_SANITIZE=ON -DBUILD_TESTING=ON
cmake --build build-sanitize --parallel 2 --target ceres_configuration_tests ceres_embedded_contract ceres_tests ceres_native_configuration_parity ceres_embedded_configuration_parity
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize -R 'configuration_contracts|configuration_native_embedded|embedded_allocation|native_dsp' --output-on-failure
```

## Bounded CPU observation

`performance.json` records one native embedded-ABI synth at48kHz with64-sample blocks,128 warmup and1,000 measured blocks per configuration. Active pulse/PWM/ring/SUB/filter modulation and every configured voice are enabled. There was one run, with no discarded warm repeats or selection. It is not a Rack target/Double/host guarantee.

| Voices | 1× median thread CPU | 2× | 4× |
| --- | ---: | ---: | ---: |
| 1 | .0166ms | .0437ms | .0867ms |
| 4 | .0549ms | .1188ms | .2352ms |
| 16 | .2226ms | .4291ms | .8446ms |

At16 voices/4×,64 of1,000 thread-CPU blocks exceeded the1.333ms budget, with a2.131ms maximum. All other configurations had zero thread-CPU overruns in this run. Wall-clock overruns occurred in several cases and remain in the report (152 at16/4×). Two warm engines in a Double adapter cost additional work, so high-polyphony/high-quality settings can exceed available realtime capacity. Lower quality also changes high-frequency guards and alias behavior; this study makes no listening-preference or hardware-match claim.

```sh
# Run in an idle measurement window, once; retain overruns.
python3 Tools/benchmark_configuration.py --report Docs/validation/poly-quality/performance.json
```

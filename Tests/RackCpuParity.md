# Rack CPU optimization audio parity

`check_rack_cpu_parity.py` compares the actual `CCeres::RenderBatch` output
before and after an engine or adapter efficiency change. It requires the
approved 23-preset Rack bank and all 66 controls. The same driver and the
baseline adapter's existing MOM test shim compile against each variant's
actual wrapper, headers and core with identical compiler flags. Only the
driver's rendering entry point runs; the included wrapper unit-test main is
renamed and remains a separate validation gate.

```sh
python3 Tests/check_rack_cpu_parity.py \
  --baseline-rack /absolute/path/to/baseline/Examples/Ceres \
  --candidate-rack /absolute/path/to/candidate/Examples/Ceres \
  --candidate-instrument /absolute/path/to/candidate/instrument \
  --output /absolute/path/to/diagnostics/rack-parity
```

The optional `--baseline-instrument` and `--candidate-instrument` override the
respective adapter's `Instrument` directory. Both Rack paths may name the same
approved adapter when only its engine pin is changing.

The runner rejects changed serialized patch values, changed parameter metadata
or changed generated defaults before compiling. It reads every saved value
directly from the public `.repatch` XML, including Init. It does not reauthor
the bank from native research presets.

The 165 scenes render 4,239,360 finite, interleaved float32 samples:

- 69 bank scenes: all 23 saved patches at 1x, 2x and 4x. The E2–G2–B2 chord,
  repeated notes, mixed velocities, pitch bend, pressure, sustain and release
  occur at exact host frames. Each scene includes live Mono/Double changes,
  20 ms transition reversal, detune and stereo pan changes, and release tails.
- 66 control scenes: one control at a time, continuous positions 0, 0.5 and 1,
  or every legal selector state. The starting patch activates oscillator,
  noise, PWM, ring, SUB, pressure, portamento, Double and EXT signal paths.
  Changes occur while sounding, followed by release, audio reset and new
  notes. Voice/quality requests intentionally remain deferred while keys are
  held; reset applies their final requested state. The existing wrapper suite
  separately verifies application of every legal voice/quality configuration.
- 30 reset scenes: both the actual motherboard defaults and serialized Init
  at 44.1, 48, 88.2, 96 and 192 kHz and every quality. Each scene changes
  voice/quality settings, performs a host audio reset, verifies the requested
  configuration applied and plays new notes.

Both channels are compared byte for byte, including the sign of zero. The
report identifies the first mismatching scene/frame/channel, the maximum
absolute difference, the scene offsets and activation peaks, source hashes,
compiler command lines, executable hashes and complete audio hashes. Raw
audio and logs remain in the output directory. A finite, audible scene
witness prevents a silent renderer from passing by producing equal zeros.

The check establishes native callback equivalence for this matrix. It is not
a CPU benchmark, a host playback test, an exhaustive control-combination
proof, or an SDK-target translation test. Retain the independent engine
`CpuParityDriver`, filter-state parity, native wrapper contracts, sanitizer
suite and SDK checks appropriate to the change.

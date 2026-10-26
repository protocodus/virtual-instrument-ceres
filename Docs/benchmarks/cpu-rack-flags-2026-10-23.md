# Ceres built the way the Rack builds it — 2026-10-23

Ceres 1.0.5 carries the Rack toolchain's lessons from Acustra and YouKnow into the engine: four changes, three exact and one at the last bit, graded under flags that resemble the Rack's build rather than the plug-in's. Nothing changes about the sound. Under clang `-O3 -fno-builtin` the engine does 1.6% fewer instructions at the Rack's four voices/1x and 4.4% fewer at 4x on this container; the first change's weight lies in the SDK's own object code, where the library calls it removes are real. The baseline is `492d93b` (1.0.4 as merged). This is an unreleased source change; no installed plug-in or Rack Extension was replaced.

## What the Rack compiles, and how it was imitated here

The Rack Extension is built by the Jukebox SDK's clang 15 with `-ccdsp -fno-builtin -ffreestanding -fcxx-exceptions -target-cpu phdsp64 -O2` (then the project's `-O3`), linked against the SDK's own libc/libc++ bitcode and finished by `llc -O2` for the host CPU. Acustra measured on that object code that under `-fno-builtin` the SDK libc++ turns `std::abs`, `std::isfinite`, `std::floor`, `std::sqrt` (a bit-serial software routine there) and `std::copysign` into out-of-line calls, and that the SDK IR has no auto-vectorisation and no FP contraction. The SDK is not in this container, so the engine was built as nearly as the host allows: GCC 13.3 and clang 18.1 at `-std=c++17 -O3 -fno-builtin -DNDEBUG` (and GCC `-O3` without it, the 1.0.4 round's flags, for comparison), instruction counts by callgrind on `Tools/callgrind_presets.cpp` (every factory preset, a chord of the configured voices released after three quarters of the blocks, 48 kHz, 64-frame blocks). clang could only be linked against libstdc++ and libc++ 18 here, both of which spell `std::abs` and `std::isfinite` as builtins themselves, so the one SDK consequence this container reproduces is `copysign` becoming a call; the others are taken from Acustra's measurement. [Which calls each build makes](../validation/cpu-rack-flags-2026-10-23/library-calls-by-flag-set.md).

## Ranking of 1.0.4 under clang `-O3 -fno-builtin`, four voices/1x (the Rack's configuration)

Exclusive instructions; 1,301,062,582 in all over 240 blocks per preset.

| Rank | Where | Share | What it is |
| ---: | --- | ---: | --- |
| 1 | `OtaSvf::process` (not inlined by clang) | 42.0% | two Newton solves per voice-sample: the `fastTanh` rationals (about 11%), the residual/derivative arithmetic, the prewarp |
| 2 | `Synth::Impl::sample` | 26.0% | per-voice control math (pitch, cutoff factors, envelopes' arguments), couplings, the ring section |
| 3 | `Oscillator::process` | 10.1% | clamps, wraps, BLEP/BLAMP corrections, the sine path |
| 4 | glibc `exp2` | 8.5% | 13 calls per host sample: pitch, HP and LP cutoff factor per voice, ring carrier frequency |
| 5 | `Envelope::advance` (not inlined; nine arguments) | 2.9% | two calls per voice-sample, most of it call overhead for a held or released board |
| 6 | `advanceGlide` (not inlined) | 1.9% | a call per voice-sample that, without PORTAMENTO, only parks the counter |
| 7 | inline `fabs` in the solver | 1.7% | the step tests |
| 8 | glibc `sin` | 1.3% | the ring carrier (kept `std::sin`, see 1.0.4) |
| 9 | `Synth::process` (decimator, EXT interpolation) | 1.0% | at 1x the decimator is a delay line |
| 10 | `expm1`, `sinTurn`, `exp` | 0.6–0.7% each | ring signal pair; PWM and SUB sines; `fastTanh` beyond .625 |

GCC inlines everything into `sample()` (76.8%) with the same libm shares. The remaining library calls that are calls under any flags are `exp2`, `exp`, `expm1` and `sin`; a cheaper `exp2` for pitch stays rejected (phase error accumulates over a long note), and the cutoff factors' `exp2` is listed below as the largest candidate not taken.

## Is a last-bit change null-testable in Ceres?

YouKnow's probe: multiply one control by one float32 ulp (1 + 1.2e-7) on every factory preset and watch the residual. `Tests/check_perturbation_probe.py` with `Tests/PerturbationProbeDriver.cpp` does this for `hp_cutoff`, `lp_cutoff`, `hp_resonance` and `lp_resonance`, a chord of four voices at 1x and at 4x, 4 s with the release at 3 s, in 250 ms windows. **No preset diverges.** While the chord is held the residual stays between −127 and −157 dB relative to the signal (−∞ where the control has no effect: Init's HP cutoff sits below the 4.7 MΩ floor), and the second half of the held chord rises at most 1.2 dB above the first; the release tail, where the signal decays under a slower-decaying residual, reaches −110.6 dB (Guitar 1, HP cutoff, 4x). Ceres's filters have no limit-cycle regime at the factory settings: the damping is at least .2 (Q at most 5), so a perturbation of the state relaxes instead of running away. That is why 1.0.4's last-bit changes could be, and 1.0.5's are, graded by the null test; in an instrument with self-oscillating presets they would be listening decisions. [Report](../validation/cpu-rack-flags-2026-10-23/float-ulp-perturbation-probe.json).

## Changes

One mechanism per commit; after each, `Tests/check_cpu_null.py --baseline-ref 492d93b` (byte-identical float32 stream every time) and the unit tier.

1. **Builtin `abs`, `copysign`, `sqrt` and `isfinite` on the audio path** (exact). `inlineAbs`, `inlineCopysign`, `inlineSqrt` and `inlineFinite` wrap the `__builtin_` forms (the `std::` forms under MSVC, which inlines them itself) and replace the calls in `fastTanh`, the ring signal pair, the filter solver's step tests, the envelopes' settle tests, the inverse normal's tails and the EXT IN guard. Here only `copysign` had been a call: −0.3% of instructions at four voices/1x under GCC `-fno-builtin`. On the SDK's libc++ the `abs` and `isfinite` calls go too, about 15 of them per voice-sample; Acustra's figure for the same move was a fifth of its batch.
2. **Decimator history as a mirrored ring** (last bit). Every sample is written at `[cursor]` and `[cursor+taps]`, so the newest `taps` samples are one contiguous window read against the coefficients stored oldest first (h is symmetric; the mirror is stored explicitly). The two straight runs around the wrap are gone, and with them the lanes' restart at the wrap: the four partial sums are now fixed to the tap index, so the grouping of the additions differs from 1.0.4 at the last bit, as the float32 stream does not. No test compares the storage. −0.8% (GCC) and −0.9% (clang) at four voices/4x; at 1x the decimator is a delay line.
3. **Vector lanes** (exact against the scalar lanes). The four partial sums are two `vector_size(16)` pairs of doubles under GCC/clang, which the SDK's clang keeps where it would not auto-vectorise, with a scalar fallback elsewhere. The same IEEE multiply and add per lane and each lane's additions in the same order: 0 mismatches over 200,000 random samples at 2x and 4x against the scalar lanes. A four-wide `vector_size(32)` type was measured first and rejected: on SSE2 GCC spent more instructions on it than on the scalar lanes (+1.1%), while the pairs give −1.6% (GCC) and −1.9% (clang) at four voices/4x.
4. **Hoisting** (exact). `Envelope::advance` keeps its attack, with the optional saturation law, out of line, so the one multiply-add and settle test every held or released board takes each sample inlines into the voice loop (the nine-argument function did not); the no-glide path parks the counter at the call site instead of calling `advanceGlide`; the oscillator computes the reset edge's BLEP once for the saw and the pulse; the ring carrier network's nodes start from their first product instead of a zero they are added to (0 + x is x); the pulse width, reset time, waveform gates and spread centre are read once per sample instead of once per voice. memcheck reports no uninitialised read. −1.6% (clang) at four voices/1x and at 4x; GCC, which had inlined most of it already, is unchanged within 0.1%.

**clang -O3 -fno-builtin**, cumulative instructions (change against 1.0.4):

| Commit | Change | 4 voices / 1x | 4 voices / 4x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `492d93b` | 1.0.4 baseline | 1,301,061,661 | 1,340,606,531 | 449,453,709 |
| `e03ca51` | 1. builtins | 1,300,143,472 (-0.07%) | 1,339,638,404 (-0.07%) | 448,750,700 (-0.16%) |
| `aebae6d` | 2. mirrored ring | 1,300,928,844 (-0.01%) | 1,327,357,352 (-0.99%) | 449,536,072 (+0.02%) |
| `664f510` | 3. vector lanes | 1,300,713,804 (-0.03%) | 1,302,681,512 (-2.83%) | 449,321,032 (-0.03%) |
| `c685de6` | 4. hoisting | 1,279,898,568 (-1.63%) | 1,281,827,324 (-4.38%) | 448,139,378 (-0.29%) |

**GCC -O3 -fno-builtin**, cumulative instructions (change against 1.0.4):

| Commit | Change | 4 voices / 1x | 4 voices / 4x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `492d93b` | 1.0.4 baseline | 1,171,235,678 | 1,217,801,782 | 428,242,650 |
| `e03ca51` | 1. builtins | 1,167,948,284 (-0.28%) | 1,214,466,430 (-0.27%) | 425,172,584 (-0.72%) |
| `aebae6d` | 2. mirrored ring | 1,168,009,618 (-0.28%) | 1,205,256,922 (-1.03%) | 425,233,918 (-0.70%) |
| `664f510` | 3. vector lanes | 1,168,009,618 (-0.28%) | 1,185,419,482 (-2.66%) | 425,233,918 (-0.70%) |
| `c685de6` | 4. hoisting | 1,168,422,041 (-0.24%) | 1,186,424,319 (-2.58%) | 430,193,419 (+0.46%) |

**GCC -O3**, cumulative instructions (change against 1.0.4):

| Commit | Change | 4 voices / 1x | 4 voices / 4x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `492d93b` | 1.0.4 baseline | 1,195,110,430 | 1,242,372,520 | 432,924,406 |
| `e03ca51` | 1. builtins | 1,195,110,430 | 1,242,372,520 | 432,924,406 |
| `aebae6d` | 2. mirrored ring | 1,195,161,040 (+0.00%) | 1,233,150,622 (-0.74%) | 432,975,016 (+0.01%) |
| `664f510` | 3. vector lanes | 1,195,161,040 (+0.00%) | 1,213,259,422 (-2.34%) | 432,975,016 (+0.01%) |
| `c685de6` | 4. hoisting | 1,189,236,830 (-0.49%) | 1,207,378,912 (-2.82%) | 435,688,562 (+0.64%) |

Every row's float checksum (`-9.3070369489454645` at four voices/4x) is the same. The hoisting step's +0.5% at one voice/1x under GCC is code placement: GCC had inlined the envelope and glide calls already, and the moved attack changes its layout; clang, the Rack's compiler family, gains on every configuration.

What remains, and was not done: the two cutoff-factor `exp2` per voice-sample (8 of the 13 `exp2` calls per host sample) could be an inline table-and-polynomial `exp2` accurate to an ulp, a last-bit change the probe says the null test could grade; on glibc it would not be cheaper than the 37-instruction library call, so its value is in the SDK's software libm, which this container cannot measure. The ring signal pair's `expm1` could be the `fastTanh` identity (its arguments at playing levels are small), about 0.5%. Two divisions per voice-sample in `FilterChain::process` are per-patch constants (exact only where the branch count is a power of two). Idle voices, control-rate updates and the pitch `exp2` stay rejected as before.

## Tests

Final tree, release DSP CTest: unit **31/31**, fuzz **1/1** (`Ceres.DspFuzz`), audio **1/1** (`Ceres.AudioDemos`, every preset demo rendered twice). Plug-in tree (JUCE 8.0.14 at its pinned commit, VST3, CLAP and Standalone on Linux): **36/36**, including `Ceres.PluginProcessor`, `Ceres.ParameterMatrix` and `Ceres.EditorRender`. Byte-exact protocol: `Tests/check_cpu_parity.py` with the driver at `-O2` against `492d93b` **passes**, 439,296 samples, SHA-256 `07120d8f1a018a32…` on both sides, the 1.0.4 stream's own hash, so the float32 output is unchanged since 1.0.3. `Tests/check_cpu_null.py` after every commit: byte-identical. memcheck on the hoisted tree: no uninitialised read. No test, tolerance or timeout changed; no test compares the decimator's storage, so the mirrored ring needed no fixture. All binaries behind these claims were compiled fresh from the sources under test (the checkers build into a new temporary directory; the CMake trees were rebuilt after touching the engine source). [Logs and reports](../validation/cpu-rack-flags-2026-10-23/).

## Reproduce

```sh
python3 Tests/check_perturbation_probe.py --report /tmp/ceres-probe.json
python3 Tests/check_cpu_null.py --baseline-ref 492d93b --report /tmp/ceres-null.json
c++ -std=c++17 -O3 -fno-builtin -DNDEBUG -I Source/DSP Tools/callgrind_presets.cpp Source/DSP/ceres.cpp -o /tmp/callgrind_presets
valgrind --tool=callgrind /tmp/callgrind_presets 4 1 240
```

The byte-exact protocol remains `Tests/check_cpu_parity.py` with `Tests/CpuParityDriver.cpp` compiled against each core at `-O2`. Native instruction counts are not Reason CPU; the Rack adapter's own parity and timing drivers measure the wrapper once its pin advances, and Acustra's SDK-native method measures what Reason runs.

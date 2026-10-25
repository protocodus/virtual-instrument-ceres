# Ceres inlined for the Rack's compiler — 2026-10-25

Ceres 1.0.6 is a third CPU round on top of 1.0.5 (`901e9a6`, the baseline of every figure here): four changes, all exact on the measured builds, nothing changed about the sound. Under clang `-O3 -fno-builtin`, the nearest this container comes to the Rack's build, the engine does **10.9% fewer instructions at the Rack's four voices/1x**, 11.1% fewer at four voices/4x and 7.4% fewer at one voice/1x; under the host's GCC `-O3`, 3.0%, 3.0% and 0.9%. The float32 CPU regression stream is byte-identical to 1.0.5 after every commit under both compilers, and its SHA-256 is still the 1.0.3 stream's. This is an unreleased source change; no installed plug-in or Rack Extension was replaced, and native instruction counts are not Reason CPU.

## Protocol

The protocol is written at the head of `Tools/callgrind_presets.cpp`. In short:

- **Workload**: every factory preset in order (14), one `Synth` each at 48 kHz, seed 7, a chord of the configured voices from the start (notes 48 + 3i, velocities .2 + .7 i/voices), released after three quarters of 240 blocks of 64 frames; four voices at 1x (the Rack's configuration), four voices at 4x (the heaviest quality) and one voice at 1x.
- **Builds**: clang 18.1 `-std=c++17 -O3 -fno-builtin -DNDEBUG` (the Rack's compiler family and its `-fno-builtin`; the SDK itself is not in this container) and GCC 13.3 `-std=c++17 -O3 -DNDEBUG` (the host build). `CERES_EMBEDDED_TARGET` is off because it removes `preset()`; it touches no audio-path code, and a clang `-fno-builtin -fno-exceptions -DCERES_EMBEDDED_TARGET=1` object of the final tree imports exactly the same library symbols as the baseline's.
- **Metric**: `valgrind --tool=callgrind` instruction counts (Ir), reproducible on this shared 4-core machine where timings are not. No timings were taken in this round.
- **Parity after every commit**: `Tests/check_cpu_null.py --baseline-ref 901e9a6` (the 439,296-sample `CpuParityDriver` stream over every factory preset × {1,4,16} voices × {1,2,4} quality, four host rates and three long scenes), once with GCC `-O3` and once with `CXX="clang++ -fno-builtin"`; then the unit tier. The workload's double checksum (`-0.96266008099316025` at four voices/1x, `-0.93764494033168333` at 4x, `-0.79647378410472447` at one voice) is the same in every row below.

## Where 1.0.5 spent its instructions (clang -O3 -fno-builtin, four voices/1x)

1,279,891,411 instructions in all.

| Rank | Where | Share | What it is |
| ---: | --- | ---: | --- |
| 1 | `OtaSvf::process` (out of line) | 44.4% | two Newton solves per voice-sample: `fastTanh`, the residual and derivative |
| 2 | `Synth::Impl::sample` | 26.8% | the voice loop's control arithmetic, couplings, ring section |
| 3 | `Oscillator::process` (out of line) | 10.5% | BLEP/BLAMP-corrected saw, pulse and sine |
| 4 | glibc `exp2` | 8.7% | three calls per voice-sample (pitch, HP and LP cutoff factor) and the ring carrier |
| 5 | `Envelope::advance` (out of line) | 3.1% | two calls per voice-sample |
| 6 | glibc `sin`, `expm1`, `exp` | 1.4%, 0.8%, 1.0% | ring carrier; ring signal pair; `fastTanh` beyond .625 |

1.0.5 had moved the envelope attack out of line so that `advance` would inline; under clang 18 with `-fno-builtin` it still did not, and neither did the oscillator or the filter solve: 3.4 million calls per second of audio at four voices.

## Changes

One mechanism per commit.

1. **Force-inline the per-voice-sample calls** (`5871808`, exact). `CERES_ALWAYS_INLINE` on `Oscillator::process`, the network form of `OtaSvf::process` and `Envelope::advance`, with `Envelope::attack` kept out of line by `CERES_NOINLINE`. Inlining changes no operation or its order, and neither build contracts floating-point operations (no FMA in the target). The hint is clang's alone: GCC already inlines all three, and forcing it there moved its register allocation (+1.0% at four voices/1x when tried).
2. **The filter solve's network-only factors once per configuration** (`e86c1c5`, exact). `2*ratio`, `1-qBlend`, `qBlend*(1-sourceBlend)` and `1-sourceBlend` were formed in every voice's every sample; `OtaNetwork::configure` now stores them. Each is the leading product or difference of its expression, which C++ evaluates left to right, so every value is the same double.
3. **Pitch and cutoff `exp2` inline, with glibc's own algorithm** (`e329ba9`, exact on glibc; a last-bit change of the double against other libms). `tableExp2` is Arm optimized-routines' `exp2` (MIT; glibc 2.28+ and musl ship it as theirs): x = k/128 + r, 2^(k/128) from a 128-entry table as scale·(1+tail), 2^r − 1 by a fifth-order polynomial, the same operations in the same order, inline for 2^-54 ≤ |x| < 512 and `std::exp2` for zero, tiny, huge, infinite and NaN arguments. The table was recomputed with 80-digit decimal arithmetic and is byte for byte glibc 2.39's `__exp_data` table (found at its offset in `libm.so.6`), and the coefficients are the five doubles that precede it there. glibc's x86-64 `exp2` has no FMA variant (callgrind shows `e_exp2.c:exp2`, where `exp` and `sin` resolve to their `_fma` forms), so the non-FMA build reproduces it: **0 mismatches over 451,696,163 arguments** (300 million uniform in ±40, 50 million in ±1100, 100 million random bit patterns, every k/128 and (k+½)/128 for |k| ≤ 140,800 with both neighbours, every power of two with its neighbours, ±0, ±∞, NaN) under GCC `-O2` and clang `-O3 -fno-builtin`. `Ceres.native_dsp_contracts` now carries that as a contract over 2.05 million arguments through `engineExp2` in the native-only API: bit for bit `std::exp2` on glibc, within one ulp elsewhere. On macOS, Windows and the Rack SDK the engine's `exp2` was the platform's and is now this .52-ulp routine, a last-bit change of the double that this container cannot run; the 1.0.5 one-ulp perturbation probe (residual at least 127 dB below the signal on every factory preset, no growth) is the evidence that such a change stays far under audibility there. Its weight in the Rack is larger than here: the SDK's libm `exp2` is a software routine. The ring carrier's `exp2` stays `std::exp2`, because `Ceres.ring_depth_continuity_contracts` recomputes it with zero tolerance. The MIT text is `ThirdParty/ARM-OPTIMIZED-ROUTINES-LICENSE.md`, listed in `THIRD_PARTY_NOTICES.md`.
4. **The voice loop's patch-only terms once per sample** (`cc87ab1`, exact). `12*octave`, `tuneCents/1200`, the touch cutoff bus `pressureFilter + lfo*pressureWah` and `filterLfo*hpSubRelativeScale` are locals formed before the voice loop: the compiler could not hoist the patch loads past the stores to the voices. The same subexpressions, so the same values.

**clang -O3 -fno-builtin**, cumulative instructions (change against 1.0.5; the step's own change in brackets):

| Commit | Change | 4 voices / 1x | 4 voices / 4x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `901e9a6` | 1.0.5 baseline | 1,279,891,411 | 5,092,965,832 | 448,132,221 |
| `5871808` | 1. inlining | 1,193,844,342 (−6.72%) | 4,747,115,539 (−6.79%) | 426,236,664 (−4.89%) |
| `e86c1c5` | 2. network factors | 1,180,082,720 (−7.80%) [−1.15%] | 4,692,065,943 (−7.87%) [−1.16%] | 422,796,962 (−5.65%) [−0.81%] |
| `e329ba9` | 3. inline `exp2` | 1,148,535,169 (−10.26%) [−2.67%] | 4,565,153,048 (−10.36%) [−2.70%] | 415,060,695 (−7.38%) [−1.83%] |
| `cc87ab1` | 4. patch-only terms | 1,139,730,047 (−10.95%) [−0.77%] | 4,529,916,935 (−11.06%) [−0.77%] | 414,956,165 (−7.40%) [−0.03%] |

**GCC -O3**, cumulative instructions (change against 1.0.5):

| Commit | Change | 4 voices / 1x | 4 voices / 4x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `901e9a6` | 1.0.5 baseline | 1,189,228,544 | 4,802,731,630 | 435,680,276 |
| `5871808` | 1. inlining | 1,189,228,544 (0) | 4,802,731,630 (0) | 435,680,276 (0) |
| `e86c1c5` | 2. network factors | 1,173,916,014 (−1.29%) | 4,741,454,569 (−1.28%) | 433,304,040 (−0.55%) |
| `e329ba9` | 3. inline `exp2` | 1,158,433,134 (−2.59%) | 4,679,523,049 (−2.57%) | 429,433,320 (−1.43%) |
| `cc87ab1` | 4. patch-only terms | 1,153,766,117 (−2.98%) | 4,660,878,674 (−2.95%) | 431,814,617 (−0.89%) |

The fourth step's +0.55% at one voice under GCC is code placement in a loop GCC had already arranged; clang, the Rack's compiler family, is unchanged or lower on every configuration at every step.

## Tried and not shipped

- **Reusing the last cutoff-factor and ring `exp2` while the argument holds** (exact): the argument repeats in 14% of the workload's cutoff solves (37% over a 3 s hold), but the compare and the extra state cost more than the skipped calls: +0.6% at four voices/1x under clang, +0.3% under GCC. Rejected on this container; in the SDK, where `exp2` is a software routine, it might pay, which only the SDK-native method can show.
- **The ring section while the mix is 0**: the carrier network, signal pair and wet coupling run at every mix to keep their capacitors connected (about 3–4% of a 1x sample). Holding them while unused changes what is heard when the mix opens, a different transient, so it is a listening decision (A–Z), not a null test. Not done in this round.
- **The ring signal pair's `expm1` as `fastTanh`** (0.8%): a last-bit change that the null test could grade; left for a later round.
- **`fastTanh`'s `exp` beyond .625 inline** (1.0%): glibc's `exp` on this machine is its FMA variant, so a non-FMA inline copy could not be bit-exact; it would be graded by the null test like `fastTanh` itself.
- Fewer Newton iterations, cheaper pitch `exp2`, control-rate cutoffs and compiler-flag changes stay rejected as in 1.0.4 and 1.0.5.

## Tests

Final tree, `build-dsp` (GCC 13.3, Release): unit tier **31/31**, fuzz tier **1/1** (`Ceres.DspFuzz`), and the C++17 Rack-flag target `CeresDSPCxx17` builds. The audio tier (`Ceres.AudioDemos`, every demo rendered twice) was not run in this round. The plug-in tree was not built: nothing in the plug-in changed. Byte-exact protocol: `Tests/check_cpu_parity.py` with `Tests/CpuParityDriver.cpp` at GCC `-O2` against `901e9a6` passes, 439,296 samples, audio SHA-256 `07120d8f1a018a32…` on both sides, the same stream as 1.0.3–1.0.5. `Tests/check_cpu_null.py --baseline-ref 901e9a6`: byte-identical under GCC `-O3` and clang `-O3 -fno-builtin` after every commit. No test, tolerance or fixture changed; one contract was added (engine `exp2`). Every binary behind these claims was compiled fresh from the source under test. [Reports](../validation/cpu-inlining-2026-10-25/).

## Reproduce

```sh
python3 Tests/check_cpu_null.py --baseline-ref 901e9a6 --report /tmp/ceres-null-gcc.json
CXX="clang++ -fno-builtin" python3 Tests/check_cpu_null.py --baseline-ref 901e9a6 --report /tmp/ceres-null-clang.json
clang++ -std=c++17 -O3 -fno-builtin -DNDEBUG -I Source/DSP Tools/callgrind_presets.cpp Source/DSP/ceres.cpp -o /tmp/callgrind_presets
valgrind --tool=callgrind /tmp/callgrind_presets 4 1 240
```

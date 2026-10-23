# Ceres CPU approximations — 2026-10-23

Ceres 1.0.4 does 38–56% fewer instructions than 1.0.3 for the same sound. The earlier rounds (0.6.6, 1.0.1–1.0.3) kept every double bit; this one allows the engine's internal arithmetic to differ at its last bit where that buys CPU, and holds the line at the output instead: the 439,296-sample CPU regression stream is **byte-identical at the float32 output** on every factory preset, voice count, quality and host rate. The baseline is `8655ab1` (the media-only child of the 1.0.3 validation commit, the engine the current Rack adapter compiles). This is an unreleased source change; no installed plug-in or Rack Extension was replaced.

## Changes

One mechanism per commit, in this order. "Exact" means the same double bits as 1.0.3; "last bit" means the double arithmetic differs by at most a few ulps and the float32 output does not.

1. **tanh and the prewarp tan as full-precision rationals** (last bit). glibc's `tanh` reaches `expm1` and was 30% of the engine's instructions: the OTA filters evaluate it twice per Newton iteration per stage, the WSC shaper once per sample, the ring carrier once per sample. `fastTanh` is the Cephes rational for |x| < .625 and the identity 1 − 2/(exp(2|x|)+1) beyond, within 2 ulp (4.2e-16 relative) of glibc over 1e-9..30 at 14 million points. `prewarpTan` is the Cephes tan rational, valid to π/4; the cutoff clamp keeps its argument under .22π; within 1 ulp over the prewarp range. Instructions at four voices/4x: −28%.
2. **Newton stops at a 1e-9 step** (last bit). Quadratic convergence leaves an error of order the step squared, 1e-18, under b's ulp; the earlier 1e-12 bought a further pair of tanh evaluations about a third of the time. Measured on the factory presets at four voices/4x: 2.34 → 2.02 iterations per solve, and the final tanh(b), recomputed in 61% of solves before, is the last evaluation carried along its tangent over that sub-ulp step. An unconverged loop (none observed) still recomputes it.
3. **Muted saw and pulse skipped, phases wrapped without floor, EXT IN reciprocal** (exact). A muted waveform is a pure function of the phase, so its gate costs nothing when it opens later, as the sine's already did. `wrapUnit` is `x − floor(x)` with a sign test for arguments within one turn of [0,1), which every phase step (clamped under .18) and subtracted fraction (under 1) guarantees. The EXT IN interpolation multiplies by 1/factor, exact for 1, 2 and 4.
4. **Coupling coefficients fixed per configuration** (last bit). Each RC coupling divided twice per sample for g = 1/(2 fs τ) and 1/(1+g), constants of the rate and the part; they are computed in `configureRate` for CP3, CP4, bus, line, the limiter's pick-up at its substep rate, both EXT IN states and the ring signal and wet couplings. The reciprocal differs from the division by at most an ulp.
5. **Decimator in four interleaved partial sums** (last bit). One running sum serialised the 249 (4x) or 125 (2x) tap additions behind each other's latency; four overlap. The additions reassociate: 1e-16 relative.
6. **PWM clock and SUB sine in the phase domain** (last bit). `sinTurn` reduces the phase to its nearest quarter turn exactly and applies the Cephes sin and cos kernels on |x| ≤ π/4: within 7.3e-16 of glibc's sin(2πp) and within 1.7e-16 of the long double value (glibc 6.9e-16, from rounding 2πp). The ring carrier keeps `std::sin`: `Ceres.ring_carrier_circuit_contracts` recomputes it bit for bit, and that contract was not loosened.
7. **Noise source idle while unused** (exact while unused or in use; see below). With NOISE at 0 and FUNCTION off N nothing audible depends on the limiter, which ran its 768 kHz substeps on every sample regardless and cost a quarter of a voice at 1x. The limiter, its pick-up coupling and the SUB-N filter now hold their state while unused; both random streams skip exactly the draws they would have taken (`Random::skip` advances splitmix's additive state), so the shared stream and the free-running drift it feeds keep their chronology bit for bit.

Nothing user-facing changed: voice counts, 1x/2x/4x, the 31-sample latency, the noise model's density, slew and substep rate, gains and every parameter range and default are as in 1.0.3. No compiler flag changed; every gain is in `Source/DSP/ceres.cpp`, which the Rack adapter compiles with its own toolchain.

## Audio

- **Byte-exact protocol.** `Tests/CpuParityDriver.cpp` compiled against both cores at `-O2` (the protocol's flags): `Tests/check_cpu_parity.py` **passes**, all 439,296 float32 samples equal including signed zero, SHA-256 `07120d8f1a018a32bdb41a553dd911e9b9ee29a7c780ccfdd403a54a53408b1d` for both (GCC 13.3, x86-64; the 1.0.3 document's hash is Apple Clang's). [Report](../validation/cpu-approximations-2026-10-23/cpu-parity-o2.json).
- **Scene-by-scene null test.** The new `Tests/check_cpu_null.py` compiles the same driver at `-O3` and measures the residual candidate − baseline per scene, for changes that are not bit-exact in double precision: every one of the 159 scenes (14 presets × {1,4,16} voices × {1,2,4} quality; {44.1,48,96,192} kHz × voices × quality under live controls; three long stationary scenes) is byte-identical, residual −∞ dB. [Report](../validation/cpu-approximations-2026-10-23/cpu-null-parity-stream.json). The double-precision differences of changes 1, 2, 4, 5 and 6 are 2²⁹ times smaller than a float32 ulp and never crossed a rounding boundary in this stream; they are differences nevertheless, so this is parity at the output, not the bit-for-bit engine parity of the earlier rounds.
- **Noise in use.** The callgrind workload with NOISE at .3 on every preset gives the same float checksum from both cores at four voices/1x and 4x.
- **Noise resumed after an idle stretch** (change 7), the one measurable consequence. `Tests/check_noise_resume.py` plays Brass 1 (four voices, 48 kHz) with NOISE at 0, then NOISE 1 with FUNCTION N at 1.5 s. Up to 1.5 s both engines produce identical samples. From 1.5 s the held limiter state resumes where an always-running limiter had moved on, so the first ~10 ms are a different realisation of the same noise, and the difference then decays through the output couplings:

  | After the change | 1x residual, dB re signal | 4x residual, dB re signal |
  | --- | ---: | ---: |
  | 0–5 ms | −30.0 | −24.7 |
  | 5–10 ms | −29.2 | −43.1 |
  | 10–15 ms | −44.7 | −36.6 |
  | 20–25 ms | −68.6 | −65.3 |
  | 50–100 ms | −89.6 | −71.9 |
  | 100–150 ms | −102.5 | −85.0 |
  | 200–250 ms | −100.2 | −86.6 |
  | 500–550 ms | −113.2 | −110.8 |

  RMS over 50–300 ms after the change: −28.918 dBFS from both engines at 1x, −27.676 at 4x. The noise's level, density and spectrum are those of the same model; what differs is which random realisation plays during the first milliseconds after NOISE leaves zero. A null test cannot grade that, so a two-letter listening set (A the 1.0.3 engine, B this one, the material above at 1x, identical level to 1e-4 dB, key written separately) was rendered to a working directory and handed over, not committed; this change is the last commit of the series and can be reverted alone. [Report](../validation/cpu-approximations-2026-10-23/noise-resume.json).
- **Tests.** Release DSP CTest on the final tree: unit **31/31**, fuzz **1/1** (`Ceres.DspFuzz`), audio **1/1** (`Ceres.AudioDemos`, every preset demo rendered twice). The plug-in tree (JUCE 8.0.14 fetched at its pinned commit, VST3, CLAP and Standalone built on Linux): **36/36**, including `Ceres.PluginProcessor`, `Ceres.ParameterMatrix` (every parameter at its extremes through the processor) and `Ceres.EditorRender`, besides the DSP tiers again. No test, tolerance or timeout changed. [Logs](../validation/cpu-approximations-2026-10-23/).

## Instructions

`Tools/callgrind_presets.cpp` plays every factory preset in order with a chord of the configured voices (released after three quarters of the blocks) at 48 kHz in 64-frame blocks; callgrind counts instructions, which are deterministic where timings on a shared container are not. GCC 13.3 `-O3`, x86-64 baseline ISA (SSE2, no FMA), native ABI. Blocks per preset: 60 at 4x with four voices, 15 with sixteen, 240 at 1x.

| Configuration | 1.0.3 instructions | 1.0.4 instructions | Reduction |
| --- | ---: | ---: | ---: |
| 4 voices / 4x | 2,118,942,151 | 1,242,372,422 | 41.4% |
| 4 voices / 1x (the Rack's default) | 2,339,352,754 | 1,195,110,420 | 48.9% |
| 1 voice / 1x | 982,452,816 | 432,924,396 | 55.9% |
| 16 voices / 4x | 1,897,109,831 | 1,080,979,985 | 43.0% |
| 4 voices / 4x, NOISE .3 | 2,213,501,370 | 1,323,075,137 | 40.2% |
| 4 voices / 1x, NOISE .3 | 2,432,856,119 | 1,503,483,474 | 38.2% |

Every row's float checksum is the same from both cores, the NOISE rows included. Per commit, cumulative, the same workloads:

| Commit | Change | 4 voices / 4x | 4 voices / 1x | 1 voice / 1x |
| --- | --- | ---: | ---: | ---: |
| `8655ab1` | 1.0.3 baseline | 2,118,942,151 | 2,339,352,754 | 982,452,816 |
| `b3c92b7` | 1. tanh and prewarp tan rationals | 1,520,704,997 (−28.2%) | 1,701,340,815 (−27.3%) | 815,056,327 (−17.0%) |
| `6328de5` | 2. Newton stops at a 1e-9 step | 1,452,482,975 (−31.5%) | 1,625,439,417 (−30.5%) | 795,465,904 (−19.0%) |
| `1a23747` | 3. muted saw/pulse skipped, wrapUnit, EXT reciprocal | 1,364,127,020 (−35.6%) | 1,542,736,085 (−34.1%) | 771,737,496 (−21.4%) |
| `b78b229` | 4. coupling coefficients | 1,339,591,732 (−36.8%) | 1,492,393,771 (−36.2%) | 727,224,878 (−26.0%) |
| `b4318c4` | 5. decimator partial sums | 1,325,841,744 (−37.4%) | 1,492,393,771 (−36.2%) | 727,224,878 (−26.0%) |
| `fa66f6d` | 6. PWM and SUB sines in the phase domain | 1,314,415,876 (−38.0%) | 1,475,091,330 (−36.9%) | 711,638,543 (−27.6%) |
| `520c39d` | 7. noise source idle while unused | 1,242,372,422 (−41.4%) | 1,195,110,420 (−48.9%) | 432,924,396 (−55.9%) |

What remains at four voices/4x: the OTA solver's arithmetic and its two rationals per iteration, three `exp2` per voice-sample (pitch, HP and LP cutoff factors), the decimator, and the ring modulator's `expm1` iteration and `std::sin`. The pitch `exp2` stays glibc's: a cheaper one accumulates phase error over a long note. The ring circuitry and idle voices keep running, as before.

## Thread CPU

`Tools/benchmark_cpu.py --baseline-ref 8655ab1` (one pass per workload in a fixed order, embedded ABI, `-O3`, 48 kHz, 64-frame blocks; Init and active with four held voices at 4x over 10,000 measured blocks; the configuration matrix over 1,000). The container has four logical CPUs and was running another project's builds during both passes (load average about 5), so these medians carry that noise; the instruction counts above do not. [Complete report](cpu-approximations-2026-10-23.json), with sources, commands, binary hashes and every deadline-miss count.

| Workload | 1.0.3 median thread CPU, ms | 1.0.4 median thread CPU, ms | Reduction |
| --- | ---: | ---: | ---: |
| init | 0.475317 | 0.299968 | 36.9% |
| active | 0.494202 | 0.330012 | 33.2% |
| configuration/1v/1x | 0.057511 | 0.026557 | 53.8% |
| configuration/1v/2x | 0.103759 | 0.060707 | 41.5% |
| configuration/1v/4x | 0.188444 | 0.113562 | 39.7% |
| configuration/4v/1x | 0.141977 | 0.082075 | 42.2% |
| configuration/4v/2x | 0.302205 | 0.169878 | 43.8% |
| configuration/4v/4x | 0.503101 | 0.341067 | 32.2% |
| configuration/16v/1x | 0.510225 | 0.330636 | 35.2% |
| configuration/16v/2x | 0.963424 | 0.607397 | 37.0% |
| configuration/16v/4x | 1.792373 | 1.215994 | 32.2% |

Every row's checksum matched exactly. At sixteen voices/4x the baseline missed the 1.333 ms deadline in thread CPU on all 1,000 blocks on this loaded container and the candidate on 234; the four-voice rows had no thread-CPU misses. A second pass after the plug-in build had finished (load average about 3) gave the same picture: init 0.470506 to 0.293014 ms (37.7%), active 0.491704 to 0.332555 ms (32.4%), the configuration matrix 30.3–53.9% across its nine rows, every checksum exact ([second report](cpu-approximations-second-pass-2026-10-23.json)). These are development-container measurements of the native engine. They say nothing about the Rack wrapper, Reason's scheduling or the SDK target's translation; the Rack adapter's own parity and timing drivers (`Tests/RackCpuParity.md`) are the place to measure those once its pin advances.

## Reproduce

```sh
python3 Tests/check_cpu_null.py --baseline-ref 8655ab1 --report /tmp/ceres-null.json
python3 Tests/check_noise_resume.py --baseline-ref 8655ab1 --report /tmp/ceres-noise-resume.json
python3 Tools/benchmark_cpu.py --baseline-ref 8655ab1 --output /tmp/ceres-cpu.json --build-dir /tmp/ceres-cpu-build
c++ -std=c++17 -O3 -g -DNDEBUG -I Source/DSP Tools/callgrind_presets.cpp Source/DSP/ceres.cpp -o /tmp/callgrind_presets
valgrind --tool=callgrind /tmp/callgrind_presets 4 4 60
```

The byte-exact protocol is `Tests/check_cpu_parity.py` with `Tests/CpuParityDriver.cpp` compiled against each core at `-O2`, as its header says.

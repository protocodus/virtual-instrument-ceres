# Ceres boards' filter solves in lockstep — 2026-10-24

Ceres 1.0.6 takes 38–52% less CPU than 1.0.5 wherever more than one voice is configured (the Rack's default four voices among them) and the same CPU with one voice, for byte-identical output in double precision. It is one exact change to the voice loop's order of work. The baseline is `94a4112` (1.0.5 with its generated media). This is an unreleased source change; no installed plug-in or Rack Extension was replaced.

## Why the voice loop was waiting

After 1.0.4 and 1.0.5 the engine executed few instructions for its model, but it executed them slowly. Each board ran its HP and then its LP OTA solve to completion before the next board started. A Newton step is a dependency chain: `tanh(b)`, then the stage equation, then `tanh(h)`, then the residual divided by the derivative, then the next `b`. That is two rational evaluations, each with a division, and a third division, all in series, so most of the time went to waiting on latency. The other boards' solves are independent of this one, but in program order they came hundreds of instructions later, beyond what the processor could look ahead to. A diagnostic build without the filter stage ran in about half the time (0.377 s to 0.181 s at four voices/1x), so the solves were about half the engine's time.

## The change

`b0c2b73`. The voice loop now makes three passes over the boards:

1. each board's key, drift, pitch, oscillator, EGs and HP/LP cutoffs, in board order, as before (the shared random stream is drawn in the same order);
2. both filter stages for all boards: every board's HP solve is set up, then the Newton steps go iteration by iteration across the boards, each board stopping at its own 1e-9 step, then each board's HP is finished, and the same again for LP;
3. each board's CP3/CP4 couplings and VCAs, summed into the bus in board order.

`OtaSvf::process` is split into `begin`, `step` and `end`, with the same expressions in the same order and `process` calling them in sequence. Each board therefore performs exactly the operations of its own sequential solve, with the same iteration count and the same values, so the result is bit for bit the same. Only the order in which the processor meets independent work changes. With one voice there is nothing to interleave, and that path calls `process` directly. The pieces carry `always_inline`, because clang otherwise kept `step` out of line and passed every working value through memory, which cost 6% at one voice.

Executed instructions **rise** about 8% at four voices (callgrind: GCC −O3 1,189,228,544 → 1,281,967,773, clang −O3 −fno-builtin 1,279,891,360 → 1,351,250,891 at 4 voices/1x), while time falls by 40% or more. The gain comes from overlapped latency, not from removed work. After the change the engine runs at a high instruction rate, so later rounds can use instruction counts as a guide to time again.

## Audio

- **Double precision, byte-identical.** `Tests/CpuParityDriver.cpp` with its output widened to float64 (an off-tree copy, `const double value = synth.process()`), compiled against both cores at −O3 with GCC 13.3 and with clang 18.1 `-fno-builtin`: 3,514,368 bytes, identical. Both builds give SHA-256 prefix `29dd2c824e38e6e4`. The stream covers every factory preset at 1, 4 and 16 voices and 1x, 2x and 4x, four host rates under live controls, EXT IN with NaN and infinite samples, and three long scenes.
- **Float32 protocols.** `Tests/check_cpu_null.py --baseline-ref 94a4112`: 439,296/439,296 samples identical, byte-identical stream, under `CXX=c++` and under `CXX="clang++ -fno-builtin"`. `Tests/check_cpu_parity.py` with the driver at −O2: passes, maximum difference 0, SHA-256 `07120d8f1a018a32…`, the same stream as 1.0.3, 1.0.4 and 1.0.5.
- **Other voice counts and NOISE.** `Tools/callgrind_presets.cpp`, which plays a chord of every configured voice on every preset and releases it, gives the same %.17g double checksum from both cores, under both compilers, at 1, 2, 3, 4, 5, 7 and 16 voices (1x and 2x for the odd counts). Runs at 16 voices/4x with NOISE .3 and at 2 voices/1x pass memcheck with no uninitialised read. The per-iteration solve records deliberately have no initialisers, and `begin` sets every field.

## CPU

**Paired timing.** Release builds of `Tools/callgrind_presets.cpp` (every factory preset in order, one `Synth` each, 48 kHz, seed 7, 64-frame blocks, a chord of the configured voices released after three quarters of the blocks) were compiled against both cores with the same flags. After a warm-up run of each, 15 pairs were timed as child-process CPU time, alternating which binary ran first, and the median of the per-pair candidate/baseline ratios is reported. The container has four logical CPUs (Intel Xeon @ 2.10 GHz) and ran other projects' builds throughout, at a load average of about 18. Rows whose interquartile range straddles zero are noise. Blocks per preset: 720 at 1x, 360 at 2x, 180 at 4x, 180 at sixteen voices.

| Configuration | GCC −O3 | clang −O3 −fno-builtin |
| --- | ---: | ---: |
| 1 voice / 1x | +1.2% (noise; IQR −2.3..+2.8) | +0.6% (noise; IQR −1.8..+1.6) |
| 1 voice / 2x | +0.3% (noise) | −1.8% (noise) |
| 1 voice / 4x | −0.2% (noise) | +1.1% (noise; IQR +0.2..+2.8) |
| 2 voices / 1x | −25.5% | −24.9% |
| 4 voices / 1x (the Rack's default) | −43.7% | −42.3% |
| 4 voices / 4x | −43.3% | −42.3% |
| 16 voices / 1x | −50.8% | −48.8% |

Every pair's checksum was identical.

**`Tools/benchmark_cpu.py --baseline-ref 94a4112`**, the repository's fixed single pass (embedded ABI, `-O3 -fno-exceptions`, 48 kHz, 64-frame blocks, median thread CPU per block). It ran on the same loaded container, so each row is one sample of that noise. [Complete report](cpu-voice-lockstep-2026-10-24.json), with sources, commands, binary hashes and deadline-miss counts. Every checksum is exact.

| Workload | 1.0.5 median thread CPU, ms | 1.0.6 median thread CPU, ms | Reduction |
| --- | ---: | ---: | ---: |
| init | 0.237206 | 0.125446 | 47.1% |
| active | 0.258377 | 0.143834 | 44.3% |
| configuration/1v/1x | 0.022185 | 0.022278 | −0.4% |
| configuration/1v/2x | 0.044346 | 0.046135 | −4.0% |
| configuration/1v/4x | 0.091457 | 0.093389 | −2.1% |
| configuration/4v/1x | 0.063049 | 0.039070 | 38.0% |
| configuration/4v/2x | 0.132787 | 0.085734 | 35.4% |
| configuration/4v/4x | 0.260998 | 0.154533 | 40.8% |
| configuration/16v/1x | 0.239350 | 0.117554 | 50.9% |
| configuration/16v/2x | 0.469761 | 0.236368 | 49.7% |
| configuration/16v/4x | 0.929336 | 0.448588 | 51.7% |

The one-voice rows of this single pass are within the spread that the paired one-voice runs above show as noise. Sixteen voices at 4x now take about a third of the 1.333 ms block on this container. These are measurements of the native engine. They say nothing about the Rack wrapper, Reason's scheduling or the SDK target's translation: that toolchain does not auto-vectorise but does schedule out of order on the host CPU, so the same overlap should be available there. The Rack adapter's parity and timing drivers (`Tests/RackCpuParity.md`) measure it once its pin advances.

## Tried and not taken

Each was measured against the tree it would have joined, and each was dropped because its gain was within the noise or it regressed one compiler:

- **Exact-argument `exp2` caches.** The ring carrier's exponent repeats on nearly every sample, but the cache saved only 0.07% of instructions at four voices/1x, below the noise. Caching the HP/LP cutoff factors' exponents hit only 14% of samples on the factory presets, which have SUB or brilliance moving the cutoffs, and added instructions: +0.3% (GCC) and +0.4% (clang).
- **`fastTanh`'s `exp` branch kept out of line and cold**, so the Newton loop need not spill around the call. Before the lockstep change: −0.8% instructions under clang, +0.4% under GCC, and timings within noise. After it: within noise.
- **`Envelope::advance` forced inline under clang** (GCC already inlines it): −2.0% instructions, timing +0.3% to −2.5% over 25 pairs, which is noise.
- **`solveFilters` specialised on the stage at compile time:** −2.4% instructions under clang (GCC unchanged), timing −0.2% to −1.3%, which is noise.
- **The lockstep Newton steps in SIMD pairs** (two boards per `vector_size(16)` lane pair, per-lane convergence masks, scalar fallback when any lane needs the `exp` branch). It was bit-exact, but the result was mixed: clang −6% at four voices and −14% at sixteen, GCC +7% at two voices and +1.5% at four. Not worth the complexity.
- **Pipelining HP of the next sample against LP of this one**, which would also help one voice, was not attempted. Host events between `process` calls would break it at 1x, so it could only be exact within an oversampled host sample.

## Tests

Release DSP CTest on the committed tree: unit **31/31**, fuzz **1/1** (`Ceres.DspFuzz`), audio **1/1** (`Ceres.AudioDemos`, every preset demo rendered twice). The build compiles `CeresDSPCxx17` (C++17, `CERES_EMBEDDED_TARGET`, no exceptions), `ceres_native` and `ceres_embedded`. The plug-in tree could not be configured here: the proxy refused the pinned JUCE archive (HTTP 403 from github.com), so `Ceres.PluginProcessor`, `Ceres.ParameterMatrix` and `Ceres.EditorRender` were not run this round. The change is inside `Source/DSP/ceres.cpp`, which the plug-in links unchanged in interface. No test, tolerance or timeout changed.

## Reproduce

```sh
python3 Tests/check_cpu_null.py --baseline-ref 94a4112
CXX="clang++ -fno-builtin" python3 Tests/check_cpu_null.py --baseline-ref 94a4112
python3 Tools/benchmark_cpu.py --baseline-ref 94a4112 --output /tmp/ceres-cpu.json
c++ -std=c++17 -O3 -DNDEBUG -I Source/DSP Tools/callgrind_presets.cpp Source/DSP/ceres.cpp -o /tmp/presets
/tmp/presets 4 1 720   # time against the same driver built on the 94a4112 core, alternating
```

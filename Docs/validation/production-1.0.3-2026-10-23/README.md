# Ceres 1.0.3 production engine validation

Candidate source/version commit: `820fb631338626aecc11af28d42840d62ebb3240`.
Baseline: previously approved `7733a4053b23a9f10cd86d0f0353154ec115687a`.
The candidate rebases the retained parity tests and evidence onto PR #7 head
`f8f7d3df7b2183e1cbf02833b29680ca3e77713c`; its complete `Source/` tree matches
that PR. CMake metadata is `1.0.3`. The prior branch is retained locally as
`backup/ceres-pre-production-7733a405`.

The additional DSP changes reuse the final Newton `tanh` value only when its
argument is bit-identical, and read the decimator history in two ordered runs.
The warm-start cache is retained. The nonlinear law, Newton updates, seven-step
ceiling, convergence threshold, electrical state, quality modes and latency are
unchanged. Decimator work applies to 2x/4x; the existing 1x delay bypass remains.

Fresh, separate CMake builds with plugin/CLAP OFF, tools ON and universal OFF
passed:

- **Release: 33/33**, including seeded fuzzing and the complete preset-demo
  finite/audible/ceiling/determinism check.
- **ASan/UBSan/float-cast-overflow: 32/32**, at `-O1 -g -DNDEBUG`. Only the
  duplicate long preset-demo render is omitted. Both builds have no compiler
  warnings.

[Release commands/results](release-summary.json) and
[sanitizer commands/results](sanitizer-summary.json) bind their logs. Full test
details and JUnit XML are retained alongside them.

The same frozen-source drivers compiled with identical C++17 `-O3 -DNDEBUG`
flags, both with and without `-fno-builtin`, produced byte-identical streams:

| Protocol | Values per comparison | SHA-256 (baseline and candidate, both modes) |
| --- | ---: | --- |
| Core audio | 439,296 float32 samples | `6039d981aaf29c4846cc7400e51091c1ac44db8ce25b5d4c1791884c70a24012` |
| Filter output/state | 1,253,376 float64 values / 208,896 frames | `aff69220e393524ac8dc720944e6d90f3548d5f4bffd38562f9a9fe658439ad4` |

All values are finite. Byte comparison preserves signed-zero distinctions.
The core protocol covers all research presets, 1/4/16 voices, 1x/2x/4x quality,
four sample rates, controllers and reset/reconfiguration. The direct filter
protocol covers six rates from 8 kHz to 1.536 MHz, conductances from 1e-12 to
0.1 S, both outputs, cutoff floor/nextafter-floor/clamping, resonance endpoints
and motion, signed high-level input, live network changes, initialization and
reset. [Exact-parity commands and streams](exact-parity.json) record each run.
Raw streams, frozen cores and binaries remain in the ignored local
`build-production-audit-20261023/` directory.

[Source identity](source-state.json) records the compiler, exact input hashes,
verified SDK 500028 / TargetVersion 5.0 and unchanged source bytes through the
runs. The [earlier validation evidence](../cpu-exact-tanh-2026-10-23/README.md)
remains byte-identical and keeps its original scope and provenance.

This evidence covers native canonical correctness. It supplies no new CPU
percentage, Rack build, host, installation or publication claim. Rack parity,
SDK-module performance, production packages and fresh host validation are
recorded separately in the Rack Extension repository.

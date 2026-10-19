# Independent code review: native pitch-route intervention

19 October 2026. **Bounded PASS: no concrete correctness defect found.** Reviewed the frozen isolated worktree `/tmp/ceres-pitch-route-20261019` against its parent `ff2dd42e71f545bd52e621bac896d1e46c132d53` and the source-routing review below. This was a read-only code, test-design and saved-evidence review. The reviewer did not execute the model, compiler, tests, renderer, optimizer or host, and did not read public audio or source targets.

## Scope and implementation

- **Native boundary:** the enum, setter and `Synth::Impl` fields are all guarded by `!CERES_EMBEDDED_TARGET`. They do not enter `Patch`, serialization or the embedded parameter ABI. The added embedded compile-time detection rejects exposure of the setter. CMake continues to compile the embedded library with its target macro and without exceptions.
- **Default preservation:** the old expression `(keyNote-60+bend)*keyTrack/12+tuneCents/1200` remains literally present with its original evaluation order. Shared performs no replacement. The oscillator frequency/phase path, source generation, envelope update order, random calls and event dispatch are untouched by the diff.
- **Correct coordinate:** OscillatorOnly removes the whole incoming bend from the filter key factor. StaticMaster replaces that contribution with `staticMasterSemitones/12`, outside both `keyTrack` and the full-network alpha. The same result feeds the legacy cutoff and coupled-network branches. The passive resistor frequency floor remains additive and unscaled. No first-bend inference or pitch fitting occurs inside this API.
- **State lifecycle:** the setter only assigns its two validated configuration fields; it does not reset or advance any audio state. `setPatch`, panic and reset leave those fields intact. Reset still clears ordinary bend/controllers and capacitor states according to the existing behavior. A live route change takes effect at the next internal sample; this is an unsmoothed offline intervention, not an additional production automation control.
- **Atomic invalid arguments:** invalid enum values, nonfinite offsets, offsets outside −48..48, or nonzero offsets for non-StaticMaster modes throw before either field is assigned. The CLI validates and configures the synth before opening its output WAV. It rejects an explicit master-offset flag outside StaticMaster even when the value is zero. StaticMaster with an omitted offset intentionally means zero and is permitted by the declared API; a diagnostic using the common high-Flute offset must nevertheless pass the frozen number explicitly.

These statements are implementation findings, not evidence that the original recording used either alternate route. The source review's one common high-Flute reference, `+0.367919921875` semitones for both passages, belongs in the separately frozen runner configuration. The generic API deliberately does not hard-code it.

## Test design and retained results

`PitchRouteTests.cpp` exercises three network modes, keyTrack 0/.5/1, positive and negative tune/bend, repeated notes/stealing, sustain/pedal, pressure, panic and reset. The comparisons go beyond repeating one formula: they require exact oscillator frequency/phase, envelope, gate/ownership, SUB/ring, Q-state, common-VCA and control-network equality across routes. Frequency-ratio assertions retain the passive floor; keyTrack-zero cases specifically discriminate master tuning from held key-CV semantics. Zero-bend/zero-offset nulls require exact audio equality. Setter nonmutation, invalid-input nonmutation and persistence are also exercised. Filter/audio capacitor histories are correctly allowed to diverge once their cutoff changes.

The author-retained logs report **11/11 Release CTest checks** and **3/3 targeted ASan/UBSan checks** passing; the latter is not a rerun of the entire suite and uses leak detection disabled. The separately compiled parent/default/explicit-Shared comparison reports three byte-identical streams of **96,000 float32 samples each**, covering four rates and all three network modes. Its frozen parent source digests match the preceding full-network baseline. The reviewer checked the helper's comparison/count logic and the saved report, but did not rerender these streams.

The validation manifest's 23 distinct retained source, test, log and binary identities were independently hashed and matched. Parent parity raw streams and its temporary parent executable were removed by the authored helper; therefore the saved equality result is bound to its reproducible source/driver/commands and hashes, not independently remeasurable retained audio. This limitation is explicitly stated in the implementation report. Neither synthetic contracts nor the default-path parity claim hardware fidelity or establish the original pitch route.

## Frozen bindings

All paths in the following table are relative to `/tmp/ceres-pitch-route-20261019`.

| Reviewed input | SHA-256 |
|---|---|
| `Source/DSP/ceres.cpp` | `7b0bfc9b9ff2377390ca43ebc4491bf804d53e83ebcff273dfa20cbcfd780306` |
| `Source/DSP/ceres.hpp` | `dac1311689d81dfc62b7d2e3bf7e67e7c8a1c85c93dd91a8facaf17db2562990` |
| `Tools/render_main.cpp` | `46cbb7e66c4bb96fb01269dfeaadd489405081624a73af465e58b23bfaf2597b` |
| `Tools/build_identity.hpp.in` | `6f1f6f52eb98835e1fa5e5dc89c128d024423cea9bd94a173b0643a982f1876b` |
| `CMakeLists.txt` | `a8c09de8c6d62f0fa75293d92a0c35887cdff929a3e878492803b30410b32896` |
| `Tests/PitchRouteTests.cpp` | `6beda0f2f55cc5f1e595df0be43cf34598d34cbe0516aa66e2cb2122a8e0930f` |
| `Tests/PitchRouteParityDriver.cpp` | `5ba3f505d447c019cb36ff54f57f145acbf8be779c87e09aa68ca872c1600f17` |
| `Tests/check_pitch_route_cli.py` | `5a528ce657e4fb366c2810aa3991a923847c83ea46b165e34307a23988303b19` |
| `Tests/EmbeddedContract.cpp` | `c52ba1fc98d5e9e959816140c53e07a3a6a2f679880d54fb5f17f9fec0a94b15` |
| `Tools/check_pitch_route_shared_parity.py` | `a3fd0f4f26a6b0ce86a1bdc86add390cb7f91a99cdcbc9c63054f4719270b592` |
| `Docs/research/pitch-route.md` | `52fac11768f142011bb96989057c62bc623817e102ef0026e2d0267cafb46be1` |
| `Docs/research/pitch-route-validation/validation.json` | `336c6f6898614c48bf602c0c554fead851f358c3cf115276f3645130b50acaa1` |

The source interpretation is bound to `Docs/accuracy/pitch-route/source-review.md` in the historical sound-path repository, SHA-256 `02565bfb5eff66f7d1d6d486a8c72b2233337501be604457a48f559932e3b261`. No implementation, old evidence or frozen plan was modified by this review. The next runner still requires its own review of route flags, common offset, unchanged patch/events/gains and reference/transfer separation.

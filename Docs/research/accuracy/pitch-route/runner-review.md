# Independent pre-execution runner review

19 October 2026. **Bounded PASS: no concrete matrix, routing or scoring defect found in the reviewed helper.** Read-only inspection of `Tools/accuracy/pitch_route.py` and the declared method; no helper import, model evaluation, source-target measurement, audio read, renderer, optimizer or host execution. This review does not replace checking the final declaration, which remains separately owned and was not yet frozen at review time.

## Checked implementation

- The matrix is exactly two raw Flute clips × three cutoff contexts × three pitch routes (18 WAVs), plus the six other frozen battery cases in Shared (24 total). Verification checks every expected triple, unique IDs/output paths, the original case dictionaries and the six compatibility IDs. Eleven exact references are selected by identity: eight frozen battery outputs, two unfitted BR=0 network outputs, and the existing high-Flute BR=−15 anchor. The new lower-Flute BR=−15 Shared output has no fabricated historical reference.
- The common offset is read from the first chronological saved high-Flute bend and checked against `0.367919921875`. Every StaticMaster command passes that same `.17g` value explicitly; no lower-clip offset, refit or recentering occurs. Only the native filter-route flags change between routes. Events, preset, rate, frame count, seed and complete original case metadata are unchanged.
- Frozen-context patches are reused by exact path/hash. Network contexts set mode1, HP/LP bias0, alpha1, and actual brilliance0/−15 once. The original `octave=-1` is preserved. Only ineffective legacy cutoff/envelope-scale override keys are removed in these network patches; the full-network branch supplies its own coupled EG/control terms. Q, all timing, source controls and gains are retained. No stopped brightness callback supplies parameters.
- All 24 native renders and all 11 known byte-exact gates finish before `run_battery.measure` is called. During rendering only native-file format, finite samples, positive peak/no clipping, command receipt and hash checks occur. The all-renders receipt is written before source scoring. A rendering/control failure retains a failed attempt and prevents scoring.
- Each altered result compares to its own clip/context Shared output through unchanged `preset_dc_loading.contrast`; all 12 comparisons are retained. Six additional direct StaticMaster versus VCO-only comparisons report the common constant exponent difference, complete numeric metric differences and unnormalized waveform differences. Canonical measurement uses the original source masks, offsets and fixed gains; no result-based selection or fitted gain is present.
- Output creation is exclusive, with existing run/replay folders rejected. Inputs and compiled renderer identity are verified before the run, again before scoring and after scoring. Logs, per-render receipts, WAVs, runtime details, producer copy and declaration copy are retained and hash-bound. Failure paths preserve partial attempts. The replay checks artifact hashes, case commands, exact references and receipt identity before recomputing scores; it saves actual results and every exact difference, including differences within the predeclared `1e-10` absolute scalar tolerance. Types, nonfloat values and booleans remain exact. This is a numerical reproducibility check, not a relaxed scientific acceptance criterion.

## Resolved review points and final declaration requirement

The initial helper compared resolved binding paths with unresolved required paths; `/tmp` versus `/private/tmp` aliases could falsely reject a valid binding. The final helper resolves both sides. It also now requires the native code review and a nonempty `state_invariance_evidence` list whose paths must all be input-hash-bound. The declaration owner must populate that list with the retained native validation report, relevant test sources and test/log evidence—not an unrelated placeholder—so the claim that oscillator invariance is established by bound native state tests is supportable. The generic helper enforces presence/binding, not a semantic reexecution of those tests.

No further helper correction was requested. The final declaration must still be independently checked for the promised retained binary/source snapshot, exact24 commands,11 references, common offset and actual evidence list before execution. The observations remain diagnostic: no original hardware MIDI, no independent new recording holdout, no physical VIB-law identification and no production/default promotion.

## Frozen review bindings

| Artifact | SHA-256 |
|---|---|
| `Tools/accuracy/pitch_route.py` | `0832a6bc2f926ec227f7ea8cd342eeac4fe1f60cac40fc843d7fddf6cc9879be` |
| `Docs/accuracy/pitch-route/method-review.md` | `19b25153ab335654f8b2f5579f87a053cfd09f0d13ee05d6ce41e628788eae18` |
| `Docs/accuracy/pitch-route/source-review.md` | `02565bfb5eff66f7d1d6d486a8c72b2233337501be604457a48f559932e3b261` |
| `Docs/accuracy/pitch-route/code-review.md` | `83eb09c9f57966d2ddae9ed6a9ac6590036f82b6ac925621752819cb713b321a` |

The previous native-code review and all frozen earlier reports remain unchanged. This new file records only the runner review.

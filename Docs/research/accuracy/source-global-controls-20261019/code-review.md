# Independent pre-execution review: dark-endpoint calibration helper

19 October 2026. **Bounded static review passes with no remaining implementation blocker.** This review read the helper, its method document, the pinned parent helper and reducers, and the root's declaration builder. It did not import or execute the helper, evaluate a model, run an optimizer or renderer, or read source-target arrays, audio, or transfer results. It is not a numerical qualification result.

## Control mapping

`BoundaryCache` applies the change from the old actual BR coordinate to −15 V through each stage's physical BR branch in the five-node RHS. Flute receives the difference from 0 V; Brass's existing −15 V receives zero difference. The resulting constant node shift is added to both scoring-window and full-trace pins before the inherited `alpha*kappa*u` mapping. Key/master-pitch scaling and the fixed feed-resistor floor remain outside alpha. An independent Schur elimination and KCL check compare the complete four-EG history; diode corners include all four EG values and both bias extremes. The parent cache asserts zero filter LFO and pressure in these frozen cases. This does not introduce a second BR frequency offset or re-add the old nominal/own-EG terms.

The native patch transformation removes the old explicit HP/LP cutoff overrides and sets the matching full-network mode, shared bias/alpha and actual BR once. Other preset controls, gates, MIDI, phase, Q, recording gains and timing remain inherited. BR=−15 V is a source-conditioned endpoint hypothesis, not a measured cap-to-voltage calibration.

## Objective and constraints

The objective is high-Flute ratio-trajectory mean squared error over its existing valid records. The 13 inequality values are `frozen_disabled_limit − current_error`, with a negated error Jacobian: one Flute fixed-gain level MSE, six pooled Brass cell ratio MAEs, and six Brass H1-level MSEs. The original early/release boundary and masks are retained. The six ratio cells contain 243 records total; the H1 constraints use 70 unique note/window records rather than duplicating a note's level for each harmonic. The MAE derivative is the mean of `sign(residual)*dresidual`; MSE derivatives are the mean of `2*residual*dresidual`.

Limits come from the frozen disabled native training outputs, while predictions are anchored to the new full-network theta0 outputs. Those roles are not conflated. The added six-cell reducer oracle uses the unchanged native reducer; its placeholder weak flags are explicitly not a weak-count qualification. Training baseline residuals, masks, times and gains are retained for later independent arithmetic review.

The small-step derivative check records any MAE sign crossing or exact-zero residual and fails the gate rather than discarding that record. This is conservative but consistent with the declared policy. The 1e−10 scalar feasibility allowance is numerical roundoff in each constraint's own units: MAE uses dB, MSE uses dB². The field name `numeric_tolerance_db` does not make these mixed metrics a common physical dB relaxation. Final native comparisons retain their original strict directions.

## Budget, freezing and validation

One exact-tuple memo serves objective, gradient, constraint, constraint-Jacobian and callback access. The 80-distinct-theta guard occurs before a new feature-model call; the initial and derivative-probe points count toward the cap. Final predictions reuse the cached returned point. A budget exception freezes the last evaluated callback point, or theta0 if none, with explicit failure/diagnostic status and no permission to validate. Callback points are not assumed feasible. A normal unsuccessful or infeasible solver return may proceed only as the root-authorized diagnostic, never as an adoption claim.

Qualification and rank gates precede training-target access. The single candidate is hash-bound before the lower-Flute endpoint control and eight validation cases are rendered. One qualification anchor plus those nine outputs gives ten distinct new WAV paths; no transfer-guided second candidate is selected. Validation calls unchanged `run_battery.measure` and `preset_dc_loading.contrast` against frozen and matched endpoint controls, with four unchanged compatibility cases checked for exact audio hashes and metrics.

Two requested evidence improvements are now present: every completed phase artifact—including copied producer, declaration, render plan, logs and receipts—is hash-bound; and the frozen-disabled residual arrays needed to independently recompute all 13 limits are saved. Gate reuse and candidate validation verify these artifact bindings. The declaration builder explicitly includes the baseline/native-controls reports and inherited old inputs. The actual immutable new plan still needs to be generated and verified by the root before execution.

This is partial-metric training feasibility: the 13 constraints do not certify Flute median-harmonic error, native weak counts or the separate 2 ms tail measurements. Those remain final native rejection gates. Neither solver success nor failure identifies physical trims or proves a globally feasible/infeasible calibration region.

## Reviewed identities

| File, relative to repository root | SHA-256 |
|---|---|
| `Tools/accuracy/brightness_boundary.py` | `1ddba1abfc20bd496567978c79d259ebae77073a5929f43d84c41b9bdba79c05` |
| `Docs/accuracy/brightness-boundary/method.md` | `6ab46f2c004a8a97e30c6d1411f9ab928be40103851bc16a4338164a66d82d3c` |
| `Tools/accuracy/joint_fit.py` | `f563762e55ab2b0c41ff4c4ba34cac103c936dc55115b5683a8a2364f330dab6` |
| `Tools/accuracy/joint_calibration.py` | `5c4b5aabafc28cc5d8dcaf502a2956e03899f0dd8977d8f7d94410a4a06d7114` |
| `Tools/accuracy/metrics.py` | `34cba33ccf34b62be818518cc34a5d4d48ece032366b1b238a5075f94b9935cd` |
| `Tools/accuracy/run_battery.py` | `19153bec22f120d0cd4d643dd87c37dee096d5c6d24adc9aa6368a18538b71ea` |
| `Tools/accuracy/preset_dc_loading.py` | `8eeaabb54c3b431ca21147da5c5d0f253583570c62e4124725277788121ea9e7` |

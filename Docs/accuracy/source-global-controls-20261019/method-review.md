# Next conditional calibration experiment: source globals and feasibility

19 October 2026. Read-only methodological review; no model evaluations, audio reads, fitting, renders, or changes to frozen tools and plans.

The smallest useful new experiment is a **shared dark-endpoint Brilliance hypothesis in the existing full dynamic network**, beginning with one new high-Flute native anchor. Set BR = −15 V for both named presets, keep the shared calibration at `(HP bias, LP bias, alpha) = (0, 0, 1)`, and retain every other frozen assumption. Brass history20 already has this exact unfitted control. A subsequent calibration search should require training no-regression feasibility before selecting a candidate. Its objective must not permit a large Flute improvement to compensate for Brass or level regressions.

This is a proposal for a new declaration. The completed [joint experiment](../joint-calibration/RESULT.md), its candidate rejection, old bounds, criteria, and reports remain valid and unchanged. No outcome of the proposed experiment is asserted here.

## Why this is a distinct, source-conditioned question

The retained source note at `/tmp/ceres-cs50-1x8LGd/research/whitmore-modulation-controls.md` supports negative/darker BR for both Flute excerpts. Its visible near-MIN position does not establish an exact stop or voltage. The `whitmore-modulation-controls-erratum.json` explicitly preserves that BR direction while withdrawing the unsupported mapping from low mechanical RES position to low electrical RES voltage. Therefore:

- **BR −15 V is a maximum-darkening endpoint hypothesis**, supported as a direction but not recovered as a voltage. It is fixed before audio inspection, not another fitted per-clip coordinate.
- **RES/Q remains frozen.** Low visible RES travel supplies no justified assignment to 0 V, 10 V, or a new Q law.
- The previous fit's Flute BR 0 V versus Brass BR −15 V contrast was conditional, not a measured difference between those passages. Replacing that contrast is a substantive input change and requires a new plan.
- An endpoint is especially economical because the ideal center-tapped BR potentiometer is stiff at its rail/ground contacts. Intermediate wiper settings require an explicitly modeled source impedance and shared loading; simply treating every proposed intermediate BR voltage as stiff is another hypothesis. There is no need to introduce a continuous BR search now.

This does not duplicate the previous [DC-intercept study](../preset-dc-loading/README.md). That study already tested BR −15 V with all ranks and both diode drops, retaining the old own-EG scale and omitting the full network's envelope coupling. It improved Flute spectra while worsening its level trajectory; all four-clip no-regression criteria failed. The present full-network tool solves every voice's EG into the shared bus, scales the complete non-key exponent, and was qualified only with **Flute BR 0 V**. Its `Cache` hardcodes that choice. Thus the exact full-network, Flute BR −15 V trajectory is new; repeating the old DC-only grid would add little.

The model's shared-bias/common-BR gauge ambiguity remains. Changing the BR contrast between recordings cannot generally be absorbed by one common offset while preserving both presets. Once a common BR is fixed, its absolute magnitude is still not independently identified from shared bias. The endpoint is a declared coordinate choice, not a solved hardware calibration.

## What the rejected fit actually established

The [post-rejection prediction check](../joint-calibration/candidate-prediction-check-001/README.md) found native-versus-surrogate ratio RMS errors of 0.0759 dB for high Flute and 0.0623 dB for Brass history20. The native weighted objective was 89.5741 dB² versus 89.3962 predicted. The large Brass regression was already predicted: its contribution rose from 13.4530 to 59.5415 dB² while Flute fell from 165.1666 to 30.0326. A roughly 0.178 dB² prediction discrepancy cannot explain that compensation.

Equal clip/domain weights therefore did not implement the desired no-regression contract. Reweighting the same sum after observing failure would still permit compensation. The next experiment should make the admissible region explicit and select only within it. A failed bounded search establishes only that no feasible candidate was found in the declared search, not global impossibility.

## Smallest staged procedure

1. **Freeze the new source hypothesis and controls.** Rank B, diode 0.6 V, fixed 355 kΩ bias branches, zero incremental chip input current, touch zero, the previous Q, EG times/levels, MIDI/gates, bend/key behavior, oscillator/source, modulation, phase, masks and recording gains remain fixed. Set both presets' BR to −15 V. Preserve the original shared bias ±15 V and alpha 0.5–2 engineering bounds; do not enlarge them in response to the old lower-bound hit. Keep the prior candidate as rejected evidence.
2. **Render one high-Flute endpoint control at `(0,0,1)`.** Compare it to the retained full-network BR 0 control, both against source and as a native change. Reuse the exact retained Brass history20 BR −15 control. Qualify the analytical BR-change prediction using the previously declared numerical limits, including full-window ratios/levels, coherent Brass sine, and fine/coarse quadrature. The fresh high-Flute native control becomes its new prediction anchor. A failed qualification stops subsequent fitting and is retained.
3. **Close the metric bridge before claiming feasibility.** Use the existing native controls and rejected-candidate training renders to test the prospective constraint calculations; no new recordings or renders are needed for this implementation check. The limitations below specify which metrics are already available and which need a separately qualified predictor. A training no-regression feasibility claim requires the actual metrics or demonstrated predictors for them.
4. **Recheck local sensitivity at the changed anchor.** The old HP weakness and LP/alpha rank check were measured at Flute BR 0. Reapply the same declared HP threshold and normalized-SVD check after the BR change. If the study intends only two active parameters, predeclare that it stops for review when HP is no longer weak. Do not silently add a parameter or assume the old sensitivity result proves the new one.
5. **If those gates pass, run one bounded feasibility search on high Flute plus Brass history20.** Use the original engineering box, a frozen starting point `(0,0,1)`, and a predeclared modest evaluation cap (at most the prior 80 forward calls). No fitted BR, per-preset gains, Q, gates, timing, or separate calibration coordinates. Freeze zero or one feasible candidate; a least-bad infeasible point is a diagnostic, not a candidate for adoption.
6. **Verify the one frozen candidate natively before inspecting transfer performance.** The final unchanged eight-case battery includes lower Flute and Brass history10, plus the four compatibility cases. These remain reused development transfers, not independent hardware holdouts. No transfer-driven second candidate. With one new training anchor and at most one final eight-case validation, the proposed phase requires at most **nine new WAVs**; old controls are reused by identity. A failure before selection needs fewer.

The endpoint anchor is a useful result even if it fails the no-regression criteria. It resolves a source-supported input discrepancy in the dynamic model and directly tests whether that discrepancy changes the earlier tradeoff. It does not justify a default change by itself.

## Feasibility contract, with no cross-domain compensation

For each training diagnostic `M_j`, set its no-regression limit to the tighter of the historical frozen-control value and the newly declared unfitted endpoint-control value. Preserve both comparisons explicitly. For spectral metrics requiring improvement, use the same strict improvement direction as the existing native gate. Keep numerical comparison tolerance distinct from any scientific relaxation; a solver tolerance is not permission to worsen an audio metric.

The complete training constraints are:

- High Flute median H2–H7 MAE and ratio-trajectory MSE improve; fixed-gain RMS error does not increase.
- Brass six-cell mean ratio MAE improves; each of its six cell MAEs, its total weak-partial count, and each of its three floored tail RMS errors do not increase.
- All original source-defined masks remain fixed. Weak candidate partials remain evaluated and cannot be dropped.

A feasibility-first scalar is `max_j((M_j(theta) - limit_j) / scale_j)`, with positive conditioning scales declared in advance. Its maximum cannot hide a worsened metric behind an improved one. Spectral strictness and integer weak counts still require explicit checks. A secondary summed objective can break ties **only after** all constraints are satisfied. Alternatively, use an explicit constrained solver with the same inequalities. The solver choice does not change the admissibility contract.

Do not declare a feasible region empty from one local optimizer termination. Preserve each evaluated parameter vector, every constraint value, active bounds, and the exact reason a candidate was rejected or selected. If no candidate is feasible, stop this declaration without relaxing limits or expanding the box.

## Concrete analytical and numerical gaps to close

**The 628-component fitting vector is not the native adoption gate.** It includes Flute ratio MSE and RMS level, plus six equally weighted Brass ratio and H1-level domains. The native gate uses median Flute harmonic MAE, six Brass **MAEs**, explicit weak counts, and per-note tails. The necessary changes are metric definitions and constraints, not just new weights.

- Flute median harmonic MAE is computable from predicted and source ratio arrays: take each harmonic's median separately, then average their absolute differences. It is not median frame error or the mean absolute value of all residuals. The existing source/native first-seven harmonic measurements are compatible; the measurement's extra harmonic count does not change those first seven estimates.
- Brass cell MAE is computable directly from the existing fixed arrays and masks; use the exact six cells and source time boundary. Preserve native-versus-surrogate qualification for these derived constraints.
- Predicted Brass H1 levels and Hh/H1 ratios permit reconstruction of each required absolute harmonic power under the unchanged recording gain. They can produce the **same threshold flags** as `metrics.comparison`, retaining all 243 source-valid records. The old fit never used weak-count constraints. Discontinuous flags require explicit evaluation, and near-threshold predictions need final native validation. A new surrogate margin, if desired, must be declared rather than quietly altering the native gate.
- **The current H1 window levels cannot stand in for the exact tail metric.** `metrics.envelopes` applies the declared fourth-order bandpass, forward/backward filtering and Hilbert envelope to audio; `metrics.tails` compares all three notes at 2 ms intervals over 137.08–137.65 s with a source-derived 40 dB floor. The fit contains only 160 ms spectral H1 windows and 70 unique source-valid note/level records. Replacing the tail metric with those windows would change the question. Either add and qualify a tail predictor against the retained native training renders before feasibility selection, or explicitly call the search **partial-metric feasibility** and keep tail no-regression as a final native rejection gate. The latter cannot certify complete training feasibility in advance.
- Verify the new BR mapping in a new helper: change the voltage in the full RHS, so alpha scales its contribution together with preset, EG, and SUB; retain key voltage outside alpha. Replace old baked Brass cutoff/BR terms once. Do not merely alter a displayed label or multiply a final frequency by a second BR correction.
- Recheck selected/manual diode inequalities over the full declared EG/bias box with Flute BR −15, along with cutoff-clamp bounds. Existing trace samples cover the retained trajectory but are not an exact internal-192 kHz proof. Any new exact state extractor must bind its driver and archived C++ inputs and discard audio; it does not change the synthesis equations.
- The synthetic source-following bends currently also affect keyboard filter tracking. The source note permits oscillator modulation against a more stationary filter and does not identify an exclusive route. Hold this assumption fixed for the BR study and keep it as a limitation; simultaneously changing pitch routing would confound the result.

None of these gaps requires changing the pinned old helper or interpreting the previous result as invalid. A separate tool and frozen declaration can reuse its independently checked network, coherent source, full-window integration and native-delta machinery.

## Reviewed identities

The source paths below remain historical evidence. Hashes bind the material read for this method review; no file in this list was modified.

| Input | SHA256 |
|---|---|
| `/tmp/ceres-cs50-1x8LGd/research/whitmore-modulation-controls.md` | `489401d1b4ee331e7587db9fd9ad014ac0fab04b5300920eb8fc99ba4972f076` |
| `/tmp/ceres-cs50-1x8LGd/research/whitmore-modulation-controls-erratum.json` | `538c57aae15fd048855562d079abf15a6ed875376930365c581f66669e78665e` |
| `joint-calibration/RESULT.md` | `2f7514405ccb5cd2dc5f10b8110f47ae191cbfb8db2a75bc240c5386e3813641` |
| `joint-calibration/fit-plan.json` | `0f096e6270700c568dba9a04e5548f497cf0cabb46f7149fb2644f50afaee3b1` |
| `Tools/accuracy/joint_fit.py` | `f563762e55ab2b0c41ff4c4ba34cac103c936dc55115b5683a8a2364f330dab6` |
| `preset-dc-loading/README.md` | `f38acfdd0c5ce106bdbf1ea96f73ce578b85e030995d18c236e579bcbab566fd` |
| `preset-dc-loading/source-review.md` | `e5bc4a83b6527760312b731ed606855cb51348c756635f8d6fa80e1526194de8` |
| `preset-dc-loading/report.json` | `316b7ba6c5d0dd5432f36a2c3df4a0aa350357837ea74b84d86326f426b17cb6` |
| `joint-calibration/candidate-prediction-check-001/report.json` | `705ed7314556e9faf39a90106c956d45fa8ef940c01921afe457780002ce2999` |
| `Tools/accuracy/run_battery.py` | `19153bec22f120d0cd4d643dd87c37dee096d5c6d24adc9aa6368a18538b71ea` |
| `Tools/accuracy/metrics.py` | `34cba33ccf34b62be818518cc34a5d4d48ece032366b1b238a5075f94b9935cd` |

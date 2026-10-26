# Conditional joint-fit method (implementation, not executed)

[joint_fit.py](../../../../Tools/accuracy/joint_fit.py) is a separate tool. The
qualified DC helper, its driver and its declaration remain unchanged. This tool
requires a new controller-owned `fit-plan.json` before any execution. It never
renders audio, and only its gated `fit` command reads hardware target values.

The training clips are raw high Flute and Brass history20. The lower Flute and
Brass history10 remain reserved for the single candidate's later native
evaluation. They are reused development transfers, not blind measurements.

## Exact fixed network

The five-node network is the frozen preset DC network with rank B, diode drop
0.6 V, fixed 355 kΩ bias branches and zero incremental chip current. All four
native EG trajectories enter the shared preset node simultaneously. The three
modulation-input shunts remain present. The sole nonzero optional source is the
existing SUB input, converted from the old LP depth convention to volts before
the solve. It is zero for both frozen training patches. Actual BR is fixed to
0 V for Flute and −15 V for Brass; pressure is zero.

The three shared coordinates are HP bias volts, LP bias volts, and alpha:

`u_stage(t) = solve(A_stage, preset + BR + all four EGs + SUB + bias/355k)`

`fc_i(t) = feed_floor + anchor_stage * 2**(key_i(t) + alpha*kappa_stage*u_i(t))`

The anchors are 12 Hz and 180 Hz. The fixed feed floor and keyboard KV remain
outside alpha. The old neutral cutoff, envelope scale and BR-baked intercept
are replaced completely. Keyboard exponent is recovered from the disabled trace
after subtracting its own-EG exponent; independent HP and LP recoveries must
agree. The disabled BR, filter SUB and pressure contributions must be zero and
its reported cutoff must be unclamped.

The same saved control traces are reused: cutoff does not feed the source,
envelopes, allocator or oscillator. No new trace producer is needed. All four
EGs are retained even when a board is silent. Cached audio work excludes only
voices with exactly zero amplitude throughout the measured span. Oscillator,
coupling, decimator and full-window integration factors are cached. The
filtered saw and direct cosine remain a coherent complex sum.

The whole saved startup/held/release EG trace must stay inside a conservative
−10..+10 V box; both factory attack targets lie inside it. Every box corner and
both ±15 V bias endpoints must retain the assumed diode conduction states.
Actual evaluated filter-clamp occurrences are reported; stage clamping and its
zero derivative outside the interior follow the native stage law.

## Ordered gates

1. **Dynamic surrogate gate.** For each training clip, compare the retained
   native unfitted-minus-disabled change with the analytical full-network
   theta0-minus-legacy change. Theta0 is `(0 V, 0 V, 1)`. Use every original
   valid frame and the same six DC-qualification thresholds. Cached legacy
   predictions must additionally agree with the pinned qualified helper within
   1e-9 dB. Any failure stops fitting.
2. **Audio Jacobian gate.** At theta0, evaluate centered finite differences
   using steps `(0.25 V, 0.25 V, 0.01)`, with the same feature weights as the
   prospective objective. If the HP column norm is below 0.02 weighted dB/V,
   fix HP bias at zero. Normalize each surviving column to unit Euclidean norm;
   require its smallest-to-largest singular-value ratio to be at least 1e-3.
   Otherwise stop. A separate tiny-step check verifies the analytic complex
   transfer/power derivative to within 1e-6 per weighted feature.
3. **One conditional fit, only if authorized.** Anchor predictions to the
   retained native **unfitted** render:

   `prediction(theta) = native(theta0) + analytical(theta) - analytical(theta0)`

   Fixed recording gains apply only to level predictions. No gain, gate, Q,
   waveform, phase, timing, mask or per-clip calibration is optimized.

The two dynamic validation renders qualify only theta0's trajectories. They do
not prove accuracy elsewhere in the engineering box. The one fitted candidate
must receive the remaining native verification battery before any decision.

## Exact objective and bounded solver

All terms are squared dB residuals. The loss is:

`0.25 * (mean_Flute_ratios + mean_Flute_RMS_levels`
`        + mean_of_6_Brass_ratio_cell_means + mean_of_6_Brass_H1_level_cell_means)`

Flute uses all H2..H7/H1 records and one RMS level per window. Brass ratio cells
are each note × early/release, retaining the frozen source-valid partial mask.
Each H1 level cell uses its unique note/phase frames with at least one valid
partial; H1 is not duplicated once per harmonic. Thus clips have equal weight,
ratio/level domains have equal weight, and the six Brass cells have equal weight.
The feature weights' squares sum to one, so the Jacobian HP norm has a defined
weighted RMS dB/V interpretation. Weak native frames are not excluded.

The solver is SciPy trust-region reflective least squares with linear loss,
analytic Jacobian, initial point `(0,0,1)`, bias bounds ±15 V and alpha bounds
0.5..2. These are engineering bounds, not physical tolerance claims. `max_nfev`
is 80; analytic derivatives avoid hidden finite-difference objective calls.
The candidate and every model evaluation are retained before any transfer
metrics are read. Solver termination, improvement and native validation status
are reported separately. No production default changes occur in this tool.

The tool refuses to overwrite attempt directories and binds each stage to the
current inputs and the preceding gate report. Numerical environment details are
retained. The earlier strict bitwise-recomputation failure remains historical
evidence; this method does not relabel it as a pass.

# Source-conditioned dark-end experiment

This is a new conditional experiment following the rejected asymmetric-BR
joint fit. It preserves that fit, its bounds, its failed checks and its adverse
results. It does not change the old declarations or production DSP.

The retained Whitmore frames show both global Brilliance caps in comparable
lower/darker mechanical regions during Brass and both Flute passages. They do
not establish an identical voltage, a precise stop, or a potentiometer taper.
The experiment fixes **actual BR = -15 V for both presets** as one explicit
maximum-darkening boundary hypothesis. It does not fit BR or infer -15 V from
pixels. RES, Q, waveforms, source phase, EG timing, gates, keys, source masks and
recording gains remain unchanged.

The model is the already-tested instantaneous four-voice cutoff network at
research commit `ff2dd42e71f545bd52e621bac896d1e46c132d53`, disabled by default.
The same shared HP-bias, LP-bias and alpha coordinates use bounds
`[-15,-15,.5] .. [15,15,2]` and initial point `[0,0,1]`. These are engineering
bounds, not measured component tolerances. The separate retained SUB-polarity
correction is unaffected. No source recording enters synthesis.

## Ordered work

1. Freeze the tool, source reviews, renderer, five compiled source inputs,
   previous declarations, training traces and input data in a new `plan.json`
   before rendering, model evaluation or source-target access for this study.
2. Render one new high-Flute BR=-15 theta0 anchor. Reuse the old high-Flute
   BR=0 theta0 render and Brass20 BR=-15 theta0 render; do not rerender them.
   Qualify the analytical change from BR=0 to BR=-15 against the native change
   at theta0 using all original windows and masks. Ratio RMS <=1 dB, ratio
   p95 <=2 dB, level RMS <=1 dB, level p95 <=2 dB, Brass signed-cell mean <=.5 dB
   where applicable, and 8/16 kHz quadrature difference <=.1 dB. Reused Brass
   dynamic qualification must retain its exact input identities and pass.
3. At the new theta0, repeat the declared Jacobian checks without targets:
   central steps `[.25,.25,.01]`, weighted HP norm threshold .02 dB/V,
   normalized surviving-column singular ratio >=.001, and tiny-step analytic
   derivative error <=1e-6. A weak HP coordinate stays fixed at zero.
4. Only after those gates, read high-Flute and Brass20 training targets. Fit
   at most **one** shared candidate with SLSQP and analytic derivatives, at
   most 80 distinct forward evaluations, `maxiter=80`, `ftol=1e-9`. No transfer
   data or metrics enter selection. Freeze the returned point and every
   attempted point, including an infeasible/unsuccessful return. A call-budget
   exception stops and preserves the attempt; it does not authorize a restart.
5. The objective is high-Flute ratio-trajectory MSE. Constraints prohibit
   increasing the following training errors against the original frozen native
   controls: high-Flute fixed-gain RMS-level MSE, each of six Brass ratio-cell
   MAEs, and each of six Brass H1-level-cell MSEs. All source-valid records
   remain, including candidate weak records. No gain or timing parameter is
   optimized. Predicted features are anchored to the source-conditioned native
   theta0 plus the analytical delta from that theta0.
6. Freeze the candidate before any transfer render/score. Then render one
   lower-Flute BR=-15 theta0 control and the unchanged eight-case candidate
   battery. Total new WAV ceiling is **10**, including the training anchor.
   Existing BR=-15 Brass20/10 theta0 controls are reused. No second candidate
   is selected from this validation.

## Interpretation and rejection gates

These training constraints are deliberately narrower than the canonical native
validation. The 160 ms Brass H1 levels do not reproduce the separate 2 ms
bandpass/Hilbert tail metric. The surrogate also does not certify native weak
counts or Flute median-harmonic errors. None is silently replaced or waived.

The final unchanged `run_battery.measure` / `preset_dc_loading.contrast` gates
require both raw-Flute median H2..H7 MAE and trajectory MSE to improve, both
Brass six-cell MAEs to improve, and no increased Flute fixed-gain RMS error,
individual Brass cell MAE, weak count or per-note tail RMS. Report comparisons
against both the frozen native controls and the matched BR=-15 theta0 network
controls. Preserve unchanged effective-Flute and dry-source compatibility
cases. All final constraints and all adverse results remain visible even if
the optimizer reports success.

Numeric replay tolerance is 1e-10 dB for stored analytical feature arrays;
derivative checks have their separately declared 1e-6 limit. This does not
retroactively change the earlier failed exact-replay check. Diode-conduction
assumptions, full-trace cutoff/clamp diagnostics and bound hits remain material.
Any candidate needs native verification; a successful conditional fit would
still not identify physical chip trim, Q, actual BR voltage or original MIDI.
All excerpts are reused development evidence rather than independent holdouts.

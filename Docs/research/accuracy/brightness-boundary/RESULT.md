# Dark-endpoint input improves Flute spectra; no calibration passes

The retained public footage does not support the preceding fit's asymmetric
global-brightness assumption. Brass and both Flute excerpts show comparable
lower/darker Brilliance positions. The new [visual review](../source-global-controls-20261019/independent-review.md)
and [electrical review](../source-global-controls-20261019/electrical-review.md)
support a negative BR direction for both; neither recovers an exact voltage,
stop, taper, resonance setting, or unchanged continuous control history.

This separate [declaration](plan.json) therefore tests one common **BR=-15 V
boundary hypothesis**, keeping the previous circuit, source, Q, envelope
timing/levels, gates, MIDI, phase, masks and gains fixed. The earlier asymmetric
fit remains rejected evidence. No production DSP, GUI or patch default changed.

## Native input check

Exactly **one new WAV** was rendered: high Flute at the full four-voice network's
unfitted `(HP bias, LP bias, alpha)=(0,0,1)` and BR=-15 V. The old BR=0 network
anchor and BR=-15 Brass anchors were reused without rerendering. Analytical
versus native brightness-change errors passed the predeclared limits: ratio
RMS **0.05639 dB**, level RMS **0.00763 dB**, and 8/16 kHz quadrature difference
**0.00000534 dB**. This qualifies the change prediction at this trajectory,
not the physical calibration or an entire parameter box.

The [canonical training-anchor score](training-anchor-metrics-001/report.json)
uses the unchanged benchmark measurements:

| High-Flute native measurement | Frozen DSP | Full network, BR=0 | Full network, BR=-15 |
|---|---:|---:|---:|
| Median H2..H7/H1 MAE |21.841 dB|23.500 dB|**6.689 dB**|
| Ratio-trajectory MSE |579.376 dB²|660.254 dB²|**47.186 dB²**|
| Fixed-gain RMS-level error |0.668 dB|0.642 dB|**1.016 dB**|

The source-conditioned endpoint substantially reduces the spectral discrepancy
but worsens the amplitude trajectory. Its native RMS slope is -1.558 dB/s
versus the recording's -0.289 dB/s. It fails the unchanged no-regression gate.
This result concerns the one training passage; no new lower-Flute or Brass10
transfer score was computed.

## Constrained calibration stopped

The new local Jacobian check passed. HP sensitivity was only 0.01014 weighted
dB/V, below the unchanged 0.02 threshold, so HP bias stayed zero. The normalized
LP-bias/alpha singular ratio was 0.28126; analytic derivative errors were below
6.33e-11 in the feature check. The separate objective/constraint derivative
check passed with no MAE sign crossings; its largest error was 9.22e-9.

Unlike the preceding compensating weighted sum, the new fit minimizes Flute
ratio-trajectory MSE subject to 13 explicit training constraints: Flute level
MSE, six Brass ratio-cell MAEs, and six unique-record Brass H1-level MSEs must
not exceed the frozen native control errors. Exact native median, weak-count
and 2 ms tail gates remain additional requirements; H1 windows do not replace
them.

The search exhausted its predeclared **80 distinct forward evaluations**,
including derivative probes. None of those points satisfied all 13 constraints.
There was **no normal solver return** and no validation permission. The last
accepted callback is retained only as a diagnostic:

`HP bias=0 V, LP bias=-2.408947723514567 V, alpha=1.3298454744496089`.

At that diagnostic point the predicted Flute ratio MSE falls 47.186→30.976 dB²,
but Flute level MSE remains 0.9632 versus its frozen 0.4456 limit. E4's Brass
release ratio MAE is 8.4996 versus 3.6458 dB; all three early Brass ratio cells
also exceed their limits. The [complete stopped attempt](fit-001/candidate.json)
preserves every requested/evaluated point, derivative check, constraint,
residual, mask, source identity and callback. No bound, criterion or call cap
was relaxed. No candidate or transfer WAV followed the budget stop. One failed
local search does **not** prove global infeasibility.

The [independent stored-array audit](review-001/review.json) verified 290 unique
bindings and all five compiled source inputs, then reconstructed every logged
objective and all 13 constraints from the saved residuals/masks: 101,426
arithmetic comparisons, maximum difference 1.42e-14. It did not rerun a model,
optimizer, target measurement or renderer.

## Remaining fidelity work

The rejected result narrows the problem: a source-supported brightness input
helps the spectra, but a shared cutoff bias/slope alone did not fix the current
envelope/Brass tradeoff. The frozen public-reference MIDI remains reconstructed,
and global RES/Q, pressure, ring contribution, gate history and chip trim are
unmeasured. The raw factory mapping is still uncalibrated; no independent
known-settings holdout exists. No recording enters synthesis.

The retained pitch-routing audit also remains material before another fit:
the reconstructed Flute pitch-bend events move both oscillator pitch and
filter key tracking, whereas the visibly raised SUB-VCO control physically
uses the oscillator-only VIB input. The source does not identify an exclusive
route or separate a constant master-tuning offset from modulation. A next
useful bounded diagnostic is to compare those explicitly declared control
routes, preserving pitch trajectories, Q, envelopes and gains, before adding
more calibration coordinates. It must not relabel this stopped fit as a pass
or silently rewrite the frozen MIDI/control evidence.

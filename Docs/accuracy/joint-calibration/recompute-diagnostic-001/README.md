# Exact-recomputation failure diagnosis

The failed `actual == saved` assertion is caused by low-order floating-point
differences in the Flute analytical calculation and its derived errors and
statistics. It is not a changed input, native audio measurement, qualification
condition, eligibility mask or pass/fail decision.

[diagnose.py](diagnose.py) calls the unchanged, hash-verified helper's `check()`
and retains the calculation object that the original check kept only in memory.
It neither changes its numerical operations nor relaxes its comparison. The
original assertion fails again. The complete [actual result](actual.json),
[exact typed recursive diff](exact-diff.json), [summary](summary.json), and
[field localization](localization.json) are retained.

- There are 989 differing numeric leaves and no nonnumeric differences.
- The first difference is baseline high-Flute
  `analytical_ratios_db[7][4]`: `-14.27553471067737` versus
  `-14.275534710677366`, a difference of `3.552713678800501e-15` dB.
- The largest difference is case 11 `analytical_delta_ratios_db[23][5]`:
  `-26.752554738984497` versus `-26.75255473898451`, a difference of
  `1.4210854715202004e-14` dB.
- All native baseline measurements and native candidate deltas are exactly
  equal. All Brass predictions are exactly equal. Every qualification gate and
  both overall `qualification_pass` statuses are exactly equal.
- All source, trace, binary and artifact hashes passed the original check before
  its final equality assertion. Python, NumPy and SciPy versions also match the
  qualification declaration exactly. Synthetic numerical oracles match exactly.

The diagnostic [declaration](declaration.json) records the runtime, NumPy BLAS
configuration, available threadpool metadata and thread-related environment
variables before calculation. This diagnostic used OpenBLAS 0.3.23.dev with ten
threads and no explicit thread-count environment variables. The first attempt
did not capture threadpool state, so a difference in threading is **not** an
established cause. Repeated analytic decimator evaluations matched bit-for-bit
in a separate synthetic [probe](decimator-repeat-probe.json). The particular
low-level floating-point kernel responsible for the variation is not isolated.

The observed differences are at ordinary double-precision rounding scale and
are over twelve orders of magnitude below the tightest declared 0.1 dB gate.
That describes their size; it does not convert the exact check to a
pass. The original failed exact-equality record and original report remain
unchanged. No fitting, new audio synthesis, source-target evaluation or transfer
evaluation occurred during this diagnostic.

Report SHA-256 identities:

- Original: `5ae567e7dbdbce2ac63569f367c74e2867b0f432186313391cb3e740b80ae176`
- Captured recomputation: `ab33fdcfff5f20054dd4a282cbb744f017c32b879f781fa592942c626a69dee1`
- Exact diff: `a70cbdf352d3b3d42de3b15e34b317053214424e4d1a45f2e75dc4c3c148bf89`

These files establish numerical agreement at the stated observed scale, not
bitwise reproducibility and not the accuracy of a full dynamic network against
hardware recordings.

# Brass short-window harmonic-ratio diagnostic

This source-only study used the existing recording and pitch track. It rendered no Ceres audio and changed no DSP or default. All settings and thresholds were declared before estimates. The excerpt spans 2.134 cycles of the inherited fixed 6.6692178847 Hz rate; every overlapping frame is retained.

| Target | Window | Source fixed-rate peak | Flat-ratio null max | Static-filter null max | Weak frames | Declared gates |
|---|---:|---:|---:|---:|---:|---|
| C4_H3_H2 | 40 ms | 0.205 dB | 0.004 dB | 0.823 dB | 0 | fail |
| C4_H3_H2 | 60 ms | 0.188 dB | 0.004 dB | 0.786 dB | 0 | fail |
| C4_H3_H2 | 80 ms | 0.169 dB | 0.003 dB | 0.739 dB | 0 | fail |
| C4_H4_H2 | 40 ms | 1.293 dB | 0.041 dB | 0.853 dB | 0 | fail |
| C4_H4_H2 | 60 ms | 1.251 dB | 0.002 dB | 0.816 dB | 0 | fail |
| C4_H4_H2 | 80 ms | 1.244 dB | 0.002 dB | 0.766 dB | 0 | fail |

The strict predeclared coloration criterion did not pass. Failures remain reported; no physical route is identified or excluded.

The 36 static-filter controls include LP corners at 400, 800, 1600 Hz and at 2/3/4 times the previously measured early C4 fundamental, with Q 0.5/2/5 and AM before/after each fixed filter. They are a limited illustration, not a universal bound on unknown coloration. Carrier offsets ±2 Hz are sensitivity cases, not an established error bound.

Complex coefficients for all 36 carriers, source masks, target powers, singular values, VIFs, residuals, every control curve and positive-recovery errors remain in report.json and arrays.npz. The old power masks came from 160 ms band measurements; their thresholds applied to local coefficient power are explicitly only a proxy. No weak row was dropped.

Same-note ratios can reject a tested spectrally flat common-gain explanation but cannot identify HP versus LP motion, ring versus VCA, hardware control positions, or the capture chain. A null does not rule out filter modulation: some filter regions act nearly as common gain over these harmonics. No original hardware MIDI is available.

Reproduce with `python3 Tools/accuracy/brass_ratio_modulation.py check --recompute` from the repository. The immutable plan, implementation declaration and artifact hashes bind this run.

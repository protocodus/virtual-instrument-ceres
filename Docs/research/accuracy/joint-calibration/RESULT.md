# Joint full-network calibration: candidate rejected

The isolated native DSP now implements the declared instantaneous four-board cutoff network, including each board's envelope contribution to the shared preset bus. The research mode replaces the old cutoff intercept and own-envelope contribution together, applies one common exponential multiplier to all non-key controls, and keeps keyboard KV and the passive conductance floor separate. Native/offline mode is disabled by default; the Rack source and the separately retained SUB-polarity correction are unchanged.

The engine at `/tmp/ceres-joint-calibration-20261018` passed9 Release tests,2 targeted sanitizer tests and656 independent node comparisons. Its disabled state reproduces384,000 native regression samples exactly. More directly, all8 frozen public-recording demos reproduce byte for byte when the new mode is disabled. The [first declaration](plan.json) binds the renderer, source snapshot and12 disabled/unfitted controls.

The analysis surrogate passed all32 retained DC-cutoff-change comparisons, then passed the two new full-network trajectory checks. Its strict exact-recomputation check failed on989 last-bit Flute analytical/derived values, at most1.42e−14dB; native measurements, Brass and decisions were exact. The original failure and complete numerical diagnostic are preserved. This is numerical agreement, not a claim of bitwise reproduction. The later fit phase declared1e−10dB numerical replay tolerance in advance.

The [fit declaration](fit-plan.json) fixed rankB, diode drop0.6V, fixed355k bias branches, BR=0V for Flute and−15V for Brass, touch0 and all previous timing, Q, gates, waveform, phase, masks and gains. HP sensitivity was below the declared threshold, so it stayed0. One candidate was frozen after11 forward model evaluations: **LP bias−10.423952819965173V, alpha0.5**. Alpha reached the engineering lower bound. The weighted training objective improved178.6196→89.3962dB²; that objective permitted compensation between presets and ratio/level domains. It did not prove transfer or physical calibration.

Exactly20 new WAVs were rendered:8 disabled identity controls,4 unfitted network controls and the final8-case candidate battery. The [native validation](native-validation-001/report.json) rejects the candidate against both the frozen and unfitted controls.

| Native case | Frozen spectral MAE | Unfitted network | Candidate | Frozen→candidate level/tail diagnostic |
|---|---:|---:|---:|---|
| Flute high, raw factory |21.841dB|23.500dB|10.738dB|Fixed-gain RMS error0.668→1.041dB |
| Flute lower, raw factory |19.165dB|20.919dB|8.470dB|0.388→1.172dB |
| Brass history20 |3.510dB|4.745dB|13.504dB|All three note-tail errors increase; weak partial frames3→39 |
| Brass history10 |3.514dB|4.004dB|12.467dB|All three note-tail errors increase; weak frames0→24 |

Flute's trajectory MSE also improves579.376→119.046 and468.051→74.227dB², but its amplitude trajectory regresses. Both Brass aggregates, all six ratio cells in each history, and all per-note tail errors regress. All4 effective-Flute/PTK compatibility outputs remain byte-identical with identical metrics. These metrics measure different quantities; they are not one overall fidelity percentage. Lower Flute and alternate Brass history are reused development transfers, not independent hardware experiments.

The [training-only prediction diagnostic](candidate-prediction-check-001/README.md) rules out a large surrogate extrapolation error at this candidate: ratio prediction RMS is0.0759dB for Flute and0.0623dB for Brass; level prediction RMS is0.0057/0.0054dB. Native weighted MSE89.5741 is close to89.3962 predicted. The Brass objective contribution rises13.4530→59.5415 while Flute falls165.1666→30.0326. Independent stored-array reviews reproduce the gates, weights, evaluation count, native contrast flags and4,128 diagnostic arithmetic comparisons.

No default or production DSP change is supported by this fit. All declarations, failed checks, input identities, commands, patches and adverse results remain retained. No GUI was changed and no recording enters synthesis. The research mode is a tested experiment, not a newly calibrated CS50 model.

A useful next source action is to recheck the already retained Whitmore footage for the actual BR/RES states around the two preset passages. The conditional fit assumed Flute BR0 and Brass BR−15; neither is a measured voltage. Another fit should not silently adjust those inputs to rescue this result. The source/model distinction and the earlier [identifiability audit](../calibration-identifiability/README.md) remain material.

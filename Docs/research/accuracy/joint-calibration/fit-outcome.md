# Frozen shared-calibration candidate

Native validation is now complete and the candidate is **rejected**. See the
[current result](RESULT.md). The text below preserves the earlier fit stage.

The dynamic surrogate and audio-Jacobian gates passed. One conditional candidate
was frozen after 11 forward model evaluations. This file records the fit stage;
native candidate validation is separate and remains necessary before any
accuracy or promotion decision. No production DSP was changed by this tool.

The controller's [fit declaration](fit-plan.json), SHA-256
`0f096e6270700c568dba9a04e5548f497cf0cabb46f7149fb2644f50afaee3b1`,
preceded all calculations. The [method](fit-method.md) and pinned helper were not
changed after declaration. The only training clips were raw high Flute and Brass
history20. No lower-Flute or Brass history10 metrics were read during selection.

## Qualification and observability

The [dynamic report](dynamic-qualification-001/report.json) compares the two
retained full-network theta0 renders with their disabled controls. Both pass
every original threshold:

| Native-minus-analytical delta error | High Flute | Brass history20 |
|---|---:|---:|
| Ratio RMS | 0.005801 dB | 0.038862 dB |
| Ratio absolute-error 95th percentile | 0.011674 dB | 0.076442 dB |
| Level RMS | 0.006733 dB | 0.014272 dB |

The largest coarse/fine quadrature delta difference is 0.000004962 dB.
Cached legacy predictions differ from the pinned qualified helper by at most
2.8422e-14 dB. The report SHA-256 is
`7bf8fd9b3bfaa9503d8eb0c22b6ddce043d158f779bb790d8bdd98bf1edeef8b`.

The [Jacobian gate](jacobian-001/report.json) measured weighted HP-bias sensitivity
of 0.011455 dB/V, below the declared 0.02 dB/V minimum. HP bias was therefore fixed
at zero before any hardware-target fit. The two surviving LP-bias/alpha columns
have normalized singular-value ratio 0.173071, above the 0.001 requirement. The
analytic derivative check's largest discrepancy is 7.520e-10 per weighted
feature, below its 1e-6 numerical limit. The report SHA-256 is
`8223ccf87d6e434336698ea79cbc6141c9881c572594053f4a365740e9b801b2`.

These gates establish local observability and surrogate agreement for the
declared trajectories. They do not identify physical component calibration.

## One candidate

[candidate.json](fit-001/candidate.json), SHA-256
`0d77a1fe3e27119869e1be4f2fef2f5bfab779b3dcd5dc0d61edda8aac021245`,
fixes the shared coordinates as:

| Coordinate | Value |
|---|---:|
| HP bias through 355 kΩ | 0 V, fixed by observability gate |
| LP bias through 355 kΩ | −10.423952819965173 V |
| Common exponential multiplier alpha | 0.5000000000000001 |

Alpha reaches the declared lower engineering bound. The bound was not widened,
and no second candidate was selected. The optimizer reports successful `ftol`
termination after 11 forward evaluations, below the 80-call limit. The declared
equally weighted training objective decreased from 178.619567 to 89.396224 dB².
This is a surrogate training objective, not a native recording-accuracy score.
All gates, Q, envelopes, source waveforms, recording gains, BR assumptions and
eligibility masks remained fixed.

No filter clamps occurred in the candidate audit over all retained 16 kHz
startup/held/release trace samples. The diode-state proof covers the conservative
four-EG voltage box and full engineering bias endpoints. Neither fact establishes
that the assumed diode law or bias coordinates match the recorded hardware.

## Numerical replay

[fit-replay-001](fit-replay-001/report.json) recomputes the frozen initial and final
training predictions without calling an optimizer. Every weighted residual
agrees with the candidate record; the observed maximum difference is zero,
within the new phase's declared 1e-10 dB numerical tolerance. The replay also
retains predicted ratios and raw/fixed-gain levels for native validation. Its
report SHA-256 is
`61583571f31be2b95bd1b860558e2e54d6b80397842c88be6a72bd85d74c5587`.

This is a numerical-replay result. The earlier DC qualification's failed strict
bitwise comparison remains preserved and is not relabeled as a pass. No new WAV,
transfer score, alternate candidate or production promotion was produced by the
fit or replay tools.

An independent [stored-evidence review](fit-001/review.json) reproduced all 50
dynamic-gate summary statistics, the Jacobian norms/SVD decision, both objective
dot products and the 11-evaluation candidate history. It found no arithmetic or
selection inconsistency. This review did not rerun audio or inspect transfers.

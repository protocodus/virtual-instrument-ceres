# Shared cutoff calibration: local identifiability audit

The declared eight-parameter control model has **rank five**, even with twelve ideal, known cutoff-control observations. Three independent parameter directions leave its first-order output unchanged. A joint Flute/Brass fit under this model therefore cannot uniquely recover shared bias and exponential calibration while each recording's BR/touch voltage and the common EG amplitude remain unknown. Fixing those quantities would make a conditional experiment possible; it would not establish their hardware values or justify changing production DSP.

This is an algebraic source audit. It reads no recording, renders no audio, fits no recording score, and changes no DSP or previous plan. The [machine-readable report](report.json) contains every matrix, singular value, null vector, input hash and numerical check. The [pre-computation declaration](attempt-001/declaration.json), [exact tool snapshot](attempt-001/calibration_identifiability.py), and [original result](attempt-001/report.json) preserve attempt 001, which passed without a failed attempt.

## Declared observations and parameters

The calculation uses the retained [five-node network](../preset-dc-loading/network.py) and its [source review](../preset-dc-loading/source-review.md). Rank B, a fixed 0.6 V selected-diode drop, centered fixed 355k bias branches, and zero incremental chip input current are assumptions. The retained HP EG220k/LP EG47k choice remains conditional: direct-M connectivity and the related HP100k alternative are unresolved. The provisional .7/.65 slopes use that study's actual-bus convention. These are not measured CS50 calibration constants.

There are twelve rows: Flute and Brass I, HP and LP, each at EG zero, initial level and peak. All four voices have the same known EG voltage in each row. These optimistic control states are not inferred from audio and do not represent twelve independent recording observations. The output is voice 1's log2 variable-gm shift, rather than total cutoff Hz or harmonic amplitude. Holding the floor and anchor fixed, the positive exponential mapping preserves the same local null directions in cutoff Hz.

The eight columns are two shared HP/LP bias source voltages; BR voltage per preset; touch-feed voltage per preset; one fractional multiplier of both exponential slopes; and one fractional multiplier of the complete EG voltage amplitude. The EG multiplier acts on initial and peak together, retaining zero as zero. It is not a release-only adjustment. The two shared bias coordinates assume equal voltage changes across the four voices; they do not measure the individual hardware trims. Touch is one existing modulation-feed source with HP150k/LP47k resistance, not a recovered pressure trajectory.

For stage j, preset p and voice i, the retained network is:

```text
gp = 1/39k; g0 = 1/10k; gs = 1/Rpreset (0 for absent Brass HP branch)
D = gp + 1/Rrank + 1/RBR + 3/Rmod + 1/REG + 1/Rbias
(gs + g0 + 4gp) P - gp sum(u_i) = gs (10 - Vdiode)
D u_i - gp P = bias_j/Rbias + BR_p/RBR + touch_p/Rmod + E_i/REG
kappa_j = s_actual,j D/gp
z_i = (1 + exponential_fraction) kappa_j u_i
E_i = E_reference,i (1 + EG_fraction)
```

The analytic Jacobian is independently assembled from this five-node matrix, then compared with central differences through the retained helper. The helper's original node solutions also match exactly. The smallest inactive manual-diode margin is 0.595615 V at the declared reference states; selected preset branches conduct. These checks establish consistency of the assumed branch state, not the actual diode law.

## Rank and explicit unchanged directions

Each column is normalized by its Euclidean norm before SVD; the relative rank threshold is 1e-10. Condition numbers describe only the nonzero subspace in these normalized coordinates, not noise-weighted audio estimation uncertainty.

| Included unknowns | Matrix | Rank | Nullity | Nonzero-subspace condition |
|---|---:|---:|---:|---:|
| Shared bias and per-preset BR | 12 × 4 | 3 | 1 | 1.415 |
| Add per-preset touch | 12 × 6 | 4 | 2 | 247.411 |
| Add common exponential scale | 12 × 7 | 5 | 2 | 302.382 |
| Add common EG amplitude | 12 × 8 | 5 | 3 | 23.100 |

Three explicit, independent directions explain the full nullity:

1. Raising both BR source voltages by dV and changing shared HP/LP bias by `-355/680 dV` and `-355/220 dV` cancels exactly at each current-summing node.
2. Raising both touch source voltages by dV and changing shared HP/LP bias by `-355/150 dV` and `-355/47 dV` does the same. The BR and touch HP/LP resistor ratios, 680/220 and 150/47, are close; separating their surviving effects is also poorly conditioned.
3. Increasing the common exponential fraction while decreasing the EG fraction cancels the first-order EG-dependent part. A two-by-two algebraic BR/touch solve per preset cancels the remaining neutral offset. Its large coefficients are tangent-vector coordinates, not feasible recommended settings or evidence of component tolerance.

The largest null-vector residual is 1.78e-15 octaves. The maximum analytic-versus-central-difference error is 8.15e-11 octaves per parameter, using a 1e-5 step. Projecting the exponential-scale column onto the other seven columns leaves relative residual 3.82e-15. All three null vectors are independent, and SVD reconstruction errors are below 5.56e-16.

An independent read-only review eliminated the shared bus analytically, using `h=gs+g0+4gp` and `u=[gp*gs*(10-Vdiode)/h + E/REG]/[D-4gp²/h]` at the reference point, then assembled all eight derivative columns without calling this audit helper. It reproduced every subset rank; maximum Jacobian difference was 9.99e-16, and the largest null residual from that separate reduction was 1.51e-14 octaves. No concrete implementation or interpretation defect was found.

These are identities of the declared local fixed-impedance model. They do not prove global non-identifiability of every nonlinear hardware model. Real trim impedance, source impedance, chip input current, diode conduction changes, measured voltage limits, or additional known controls can change the result. The common exponential and EG-scale parameters are diagnostic hypotheses, not established tolerances of a shared component. In particular, adding the eighth column changes the nonzero-subspace condition number without resolving the additional null direction.

## Practical decision and discriminating evidence

This audit does not support an unconstrained shared-calibration optimizer or a production mapping change. The [DC-loading experiment](../preset-dc-loading/README.md), [release isolation](../release-isolation/README.md), and [harmonic-ratio diagnostic](../brass-ratio-modulation/README.md) retain their separate results. No score or physical-route conclusion from them is replaced by this rank calculation.

A useful next source would fix the unknown global inputs and independently constrain the scale: for example, two measured cutoff-control-voltage/response points on one instrument with unchanged trims, known BR/touch, and EG disabled, followed by independently measured EG voltage range. A named-preset recording with documented fixed globals, no pressure or filter LFO, and known notes/gates would constrain a conditional audio comparison. Q and oscillator/recording-chain uncertainty would still need their own evidence. No currently qualified public recording is asserted to supply these measurements.

Using high Flute and Brass history20 for adjustment, then lower Flute and history10 for transfer, would be reused development data. History10 and history20 are alternate assumed histories against the same Brass recording, not independent observations. A better score under those assumptions could be useful, but would not identify the actual hardware calibration.

## Reproduce and preserve identity

Run from the repository root with Python 3 and NumPy (the retained run used NumPy 1.26.4):

```sh
python3 -B Tools/accuracy/calibration_identifiability.py check
```

The check verifies the archived tool, declaration and result, rechecks all pinned inputs, recomputes the audit, and requires exact report equality in this numerical environment. Cross-platform byte equality is not promised. The original creation command was `python3 -B Tools/accuracy/calibration_identifiability.py run --attempt attempt-001`; it refuses to overwrite a completed report. Use the repository tool for reproduction; the archived script is an identity snapshot, not a standalone relocated executable.

The report binds the exact network, source evidence, source review, current DSP source identities and all inherited source-file hashes. Cached primary PDFs and prior evidence retain their original absolute paths under `/tmp/ceres-cs50-1x8LGd`; those files must be present with matching hashes. They are not audio inputs. Missing or changed inputs fail the audit rather than silently substituting sources. A new failed attempt is retained with its declaration, tool and traceback; this report and its original attempt must remain unchanged.

# Rejected candidate: native versus frozen prediction

The surrogate accurately predicted the two training renders at the single frozen candidate. The failed adoption criteria chiefly reflect the declared objective's tradeoffs: its large Flute spectral improvement outweighed Brass spectral and Flute level regressions. This diagnostic does not change the rejection, fit another candidate, or identify physical hardware calibration.

The candidate remains HP bias 0 V, LP bias −10.423952819965173 V, alpha 0.5000000000000001, with alpha at its lower engineering bound. Candidate SHA256: `0d77a1fe3e27119869e1be4f2fef2f5bfab779b3dcd5dc0d61edda8aac021245`.

## Prediction errors

Errors are native measurement minus the frozen `fit-replay-001` prediction. All original fitting masks remain in place, including candidate partials that become weak. Levels use the original fixed recording gains; applying the same gain to both sides leaves their prediction error unchanged.

| Training clip | Ratio RMS | Ratio p95 absolute | Ratio maximum absolute | Level RMS | Level p95 absolute |
|---|---:|---:|---:|---:|---:|
| High Flute | 0.075921 dB | 0.148522 dB | 0.197150 dB | 0.005674 dB | 0.014036 dB |
| Brass history20 | 0.062310 dB | 0.133515 dB | 0.454181 dB | 0.005413 dB | 0.013388 dB |

The largest absolute signed mean among the six Brass ratio cells is 0.056808 dB. Both clips satisfy all comparable numerical limits from the earlier qualification (ratio RMS/p95, level RMS/p95, and Brass signed cell mean). Quadrature was not recalculated, and this comparison is not a new adoption gate.

## Objective decomposition

These values reproduce the fitting objective, using its exact masks and six equal Brass cells. “Initial” is the native **unfitted full-network** branch. It is not the historical frozen production baseline, and these objective components are not interchangeable with the separate battery's aggregate metrics.

| Contribution to weighted training MSE | Initial unfitted | Predicted candidate | Native candidate |
|---|---:|---:|---:|
| Flute ratio and level | 165.166607 | 29.748461 | 30.032608 |
| Brass ratio and level | 13.452960 | 59.647763 | 59.541505 |
| Total, dB² | 178.619567 | 89.396224 | 89.574112 |

Flute's improvement contributes −135.1340 dB²; Brass's regression contributes +46.0885 dB². Equal domain weights do not prevent such compensation when starting error magnitudes differ greatly.

The Flute level residual RMS was predicted to worsen from 0.642400 to 1.042154 dB and measures 1.041499 dB natively. Every Brass note/phase ratio cell was predicted to worsen; every cell also worsens natively. Thus these regressions do not arise from a previously hidden large surrogate error at this candidate. They are compatible with reducing the chosen summed objective while failing independent no-regression criteria.

For frozen predicted residual `r` and weighted native-minus-predicted error `e`, the report verifies:

`||r + e||² = ||r||² + 2 r·e + ||e||²`

The native-minus-predicted MSE difference, 0.177888 dB², comprises a 0.174081 dB² cross term and 0.003807 dB² prediction-error energy. This small difference does not explain the roughly 46.09 dB² Brass objective regression.

## Reproduction and boundaries

Run from the repository root:

```sh
python3 Docs/accuracy/joint-calibration/candidate-prediction-check-001/check_predictions.py check
```

The existing `reproduction.json` records a numerical reproduction with maximum difference **0.0**, against the declared 1e−10 tolerance. The helper intentionally refuses to overwrite evidence; running `check` again requires preserving the existing reproduction under a new named attempt first. Runtime, tool, input audio, patch, candidate, replay, and native-validation identities are retained in `declaration.json` and `report.json`. The main report SHA256 is `705ed7314556e9faf39a90106c956d45fa8ef940c01921afe457780002ce2999`.

Only the two training WAVs were measured, using unchanged `joint_calibration.native_measurement`. No transfer audio or metrics were inspected. The native aggregate validation report was hashed without parsing its metrics. No source recordings or target files were opened; source residuals were reconstructed algebraically from the already frozen training replay. No model evaluation, refit, new render, gain adjustment, exclusion, or production edit occurred. The prior strict equality failure remains preserved in its original phase; this numerical reproduction does not retroactively change it.

The result is local evidence about this one conditional candidate. It neither establishes that the entire search region is accurately represented nor proves that no other useful common calibration exists. It supplies no basis for promoting the rejected candidate or reopening the bounds without a separately declared experiment.

An independent stored-evidence review (`review.json`) reproduced 4,128 scalar comparisons exactly, including all 14 objective components, 628 weighted residuals, masks, six Brass cells, and the objective identity. The reviewer did not import this helper, read audio or source targets, inspect transfers, or run the model or optimizer. No arithmetic or interpretation defect was found.

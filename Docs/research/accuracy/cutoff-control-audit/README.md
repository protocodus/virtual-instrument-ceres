# Ceres filter audit — next experiment and cutoff-capacitor bound

Read-only source audit, 2026-10-18. Canonical DSP inspected at commit `55869df8ba728e25a6b5792aa07dfd55f134439c`; cpp SHA256 `3478c02cc7c134a698bf0a32b708b2bda8e7eb0e38f97d27ee458a98c481baaa`. No production DSP, default, test, GUI, package, or pre-existing report was changed. This durable audit and its fresh calculation report were added separately. No new audio render was made.

**The large raw-Flute mismatch is still an unidentified factory-control calibration, not a demonstrated new filter implementation bug.** Source/DSP/ceres.cpp:1009–1024 explicitly uses provisional 12/180 Hz variable-gm anchors and .7/.65 octaves per nominal preset volt. The prior effective patch changes LP reference from1930.585 to217.043 Hz and resonance to its engineering maximum, as well as amplifier timing/level. Its <1 dB harmonic error cannot validate the nominal electrical conversion.

The selected preset table is an unloaded resistor-divider coordinate. Actual preset bus, physical pin4 and the servicing guide's voltage-law coordinate are different quantities. Rank shunts and finite shared loading establish circuit ratios, but not the missing internal voltage-to-current scale or trims. Neither the single documented cutoff point nor related-model tone-matching adjustments identify a replacement absolute slope. Rechecked primary crop `cutoff-m-eg-continuity.png` and rank table `q-m-rank-table.png` visually; source PDF hashes are bound in the JSON.

## A real omission with a useful new bound

The M sheet shows **0.01 µF at each filter pin4**. Current `sample()` computes the envelope/BR/SUB-to-cutoff exponent algebraically and has no corresponding capacitor state. These are separate from the10 µF pin7 resonance capacitors and the0.22 µF EG timing capacitor.

Nominal grounded rank resistors give HP R6=2.7/2.2/1.8 kΩ and LP R7=3.0/2.4/2.0 kΩ for ranks A/B/C. Consequently their local rank-only time-constant ceilings are **27/22/18 µs HP** and **30/24/20 µs LP**.

This bound can be extended beyond the earlier stiff-source analysis. With only these eight10 nF capacitors as dynamic states, any additional passive resistive source/loading mesh yields, after elimination of algebraic shared nodes:

`G_reduced = diag(1/Rrank) + Schur_complement(G_passive) >= (1/3000 Ω) I`.

Thus the slowest natural time constant is at most **30 µs**, even with finite passive FH/FL, BR and SUB source resistance and mixed ranks. The script independently checks2,000 full12-node passive networks, using unequal ranks, four shared nodes and nonnegative chip shunts; the largest observed tau is29.242 µs. This numerical check supports the algebra; it is not hardware measurement. Additional upstream capacitors, active negative incremental input impedance, uncertain component tolerances, or omitted active dynamics lie outside the bound.

The earlier explicit stiff-source Brass I reduction gives HP common-mode tau24.28/20.16/16.75 µs and LP22.54/18.98/16.39 µs. These retain the conditional HP220k EG branch and330k+25k bias resistance; neither choice resolves the documented drawing disagreement. At the worst30 µs pole, a100 Hz CV component changes magnitude by−.00154 dB and phase by−1.080°. This is a **control-voltage** response, not an audio-spectrum error bound. It preserves DC cutoff and cannot explain an absolute factory-cutoff error or a long envelope time discrepancy. Current Flute and Brass I attacks are84.918 and382.825 ms;30 µs is only .0353% and .00784% of those durations.

If implemented later, integrate the **pin4 control coordinate**, not the final total cutoff frequency: the known key voltage enters separate pin2, and the fixed22.575 Hz audio-integrator feed path must stay outside exponential modulation. A source-derived rank/default must remain explicit. Preserve control-capacitor continuity across note allocation, coefficient edits and idle. Verify static equality, independent RC step response, and equal-timing test cases before considering fast-envelope/audio-rate consequences. This is a worthwhile small circuit detail, but a poor candidate for repairing the current Flute/Brass mismatch.

## Avoid repeating completed sensitivity studies

The cached `reports/filter-placement-checkpoint.md` and adjacent studies already retain these outcomes:

- Increasing LP Q alone worsens all aggregate comparisons.
- A−0.5 octave LP variable-gm placement improves every A3/C4 release spectral cell but worsens their early tone and amplitude tails; all four declared transfer families fail on additional weak records.
- Shared LP-bus coupling is small under its stated assumptions; changing only a matched-common slope does not identify the factory law.
- Direct Flute Q-loading/preset-transition and Flute-to-Brass cutoff transfers were tested; these are not unexplored hypotheses.

The next unexecuted checkpoint experiment is combined, **fixed before rendering**: source-derived initial/release range (related CS60−4 V versus direct CS50−5 V), faster-decay diagnostic, and LP envelope-range mapping, preserving both gate histories, all three notes, source-valid weak bands, early and late spectra, and amplitude tails. Apply the same IL target at reset/attack start and release; never add a release-only offset. The faster-decay factor remains explicitly in tension with CS50's documented1ms–1s attack and10ms–10s decay ranges. No result may be promoted merely from a minimum aggregate score. An independent release-isolation diagnostic is being run first to discriminate whether that combined experiment is worth executing.

## Reproduce and inspect

From the repository root, run `python3 Docs/accuracy/cutoff-control-audit/check_cutoff_capacitor.py`; stdout should equal [result.json](result.json) while the bound source files remain unchanged. The script resolves the repository from its own location. Cached primary PDFs, inspected crops, and the prior loading note are read from `/tmp/ceres-cs50-1x8LGd/research` by default; pass `--research-root /absolute/path/to/research` when those exact files have moved. Their hashes must match the saved report for the same evidence binding. Relocating sources changes path keys in `source_sha256`, not numerical results. The script reads cached originals and computes matrices; it neither invokes synthesis nor modifies canonical state. NumPy is required. The saved script hash refers to this durable repository copy; numerical fields were compared exactly with the original `/tmp/ceres-filter-next-20261018/cutoff-capacitor.json`. Primary sources: [CS50 large-sheet service manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf), PDF12/printed16; [R7 schematic in small-sheet manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%201%20of%202%20%28%208.5%20x%2011%20-%20Double%20Sided%20%29.pdf), PDF8/printed14. Prior detailed coordinate, source disagreement and loading analysis: `/tmp/ceres-cs50-1x8LGd/research/cutoff-voltage-scale.md`, `filter-control-coordinate-review.md`, `cutoff-control-netlist.md`, `lp-bus-coupling-source.md`, and `vcf-release-path-audit.md`.

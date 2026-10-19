# Preset loading: completed DC-intercept experiment

None of the twelve declared rank/diode/BR combinations improves all four frozen recordings, against either the original controls or the matched stiff-source controls. All 68 renders completed and were independently remeasured. The four original controls reproduce the prior WAVs byte for byte. No production DSP or preset defaults were changed.

The [source review](source-review.md) defines the physical network and its provisional voltage calibration. The [immutable plan](plan.json) fixes four unchanged controls, sixteen stiff-source controls and forty-eight loaded cases before audio rendering. The [complete report](report.json) preserves every metric, command, audio identity and transfer decision; `status: pass` means the experiment executed and verified, not that a candidate passed the accuracy criteria.

| Measurement | Frozen control | Loaded, BR −15 V, all six rank/drop cases |
|---|---:|---:|
| Flute high harmonic MAE |21.841 dB|6.938–7.691 dB|
| Flute lower harmonic MAE |19.165 dB|4.940–5.676 dB|
| Flute high held-gain RMS error |0.668 dB|1.024–1.045 dB|
| Flute lower held-gain RMS error |0.388 dB|1.176–1.198 dB|
| Brass history20 six-cell MAE |3.510 dB|4.559–4.780 dB|
| Brass history10 six-cell MAE |3.514 dB|3.904–4.029 dB|

The Flute spectral gain is genuinely associated with loading under the declared BR −15 V hypothesis: it improves median MAE by 12.407–12.583 dB high and 12.125–12.306 dB lower versus the **same diode/BR stiff controls**. It does not fix the amplitude trajectory. At BR 0, loading instead worsens both Flute spectral measures. All twelve loaded Brass results are worse than their original frozen controls.

The Brass tradeoff is systematic. At BR −15 V, A3/C4 release spectra improve, but every early-note cell worsens. History20 E4 release worsens by 4.25–4.84 dB and weak native records increase from three to four. Against the matched stiff controls, A3/C4 joint tail errors worsen in both histories. These results support a cutoff-placement sensitivity, not a correct common factory mapping or envelope calibration. No per-recording gain, gate, window, mask, oscillator, resonance or envelope adjustment was made.

All twelve settings fail both the four-clip spectral-transfer criterion and the stronger no-regression criterion, against both control families. The public excerpts remain reused development evidence with reconstructed MIDI and unknown hardware trims/global voltages; none is an independently controlled holdout.

Reproduce verification without rerendering:

```sh
python3 Tools/accuracy/preset_dc_loading.py check
```

Plan SHA256: `1a3883fbba336a58f4d051c95eb8587e18a0780ae57a3ad2c8ac73a7339dc4be`.
Report SHA256: `316b7ba6c5d0dd5432f36a2c3df4a0aa350357837ea74b84d86326f426b17cb6`.

A distinct next investigation is a source-only short-window harmonic-ratio modulation check. Existing 160 ms windows span roughly one cycle of the frozen 6.6692178847 Hz common modulation hypothesis. Pitch-following 40/60/80 ms H3/H2 and H4/H2 estimates, with fixed quadratic detrending and synthetic common-AM recovery controls, could test whether common gain alone explains the motion. This has not been executed. Even a positive ratio-modulation result would not uniquely identify a VCF, ring or VCA route.

# Independent visual review: Whitmore global filter controls

19 October 2026. Source-only review of retained images from [Andy Whitmore's CS-50 demonstration](https://www.youtube.com/watch?v=Fj21t8MIS_o). No audio measurements, model evaluation, fitting, renders, downloads, or changes to historical evidence were performed.

## Finding

**Both BRILLIANCE and RESONANCE occupy comparable lower/forward mechanical regions during Brass I and both Flute excerpts. No meaningful cross-passage displacement of either cap is resolved in the inspected images.** The observations do not supply a visual basis for treating Flute brilliance as neutral while treating only Brass brilliance as dark. They also do not establish identical voltages or a continuously unchanged setting between passages.

| Passage | Directly inspected evidence | Independent cap-position finding |
|---|---|---|
| Brass I, 136.40–138.00 s | Existing event contact sheet: eleven samples at 160 ms intervals; full 137.00 s frame; enlarged ring/global crop | RES and BR caps are down/forward below the visible upper portions of their slots/stems. No hand reaches either global control in these event-sheet samples. Their positions remain visually stable within this sampled sequence. |
| High Flute, 205.75–207.65 s | Global crops at 205.80 and 207.00 s in the contact sheet; individual 205.80 crop and full frame | Both caps again occupy the lower/forward region. Neither appears centered or conspicuously raised relative to the Brass geometry. |
| Lower Flute, 210.80–212.00 s | Global crops at 210.90 and 211.50 s in the contact sheet; individual 210.90 crop | Both caps retain the same broad placement as high Flute. No resolved global-control movement appears between these inspected samples. |

The comparison uses the printed RESONANCE/BRILLIANCE labels, the cap top/front edges, exposed slot/stem above each cap, and the fixed lower panel line. It does not use cap color as a voltage indicator. Some adjacent ring controls differ across passages, so the observation is specific to the two global caps, not a claim that the entire panel is unchanged.

Confidence is moderate to high for the broad near-lower-end classification and moderate for the absence of a visually resolvable cross-passage difference. The exact mechanical stop is not visible underneath the caps. Blur, perspective, cap size and differently resized crops prevent a trustworthy displacement fraction or numerical position uncertainty. No pixel-equality or registered-image measurement is claimed. In particular, “no resolved change” is weaker than “the settings are identical.”

## Electrical interpretation remains conditional

The retained Servicing Guide text, printed page 5 (`cs-servicing-guide.txt`, lines 646–649), states that moving BRILLIANCE toward MAX raises both filter cutoffs and moving toward MIN gives a softer spectrum. The earlier visual audit interprets the low cap as the darker/MIN direction. That supports a **source-conditioned dark/negative BR hypothesis for both passages**, rather than evidence for a neutral Flute setting. The present images do not independently identify the electrical lug orientation, actual loaded wiper voltage, taper, or exact stop. Thus −15 V remains an endpoint assumption; neither −15 V nor any alternative negative voltage is recovered by this review. A common negative setting is a plausible restricted hypothesis, not a measured equality constraint.

For RESONANCE, the prior erratum is essential: **a lower mechanical cap does not establish 0 V, 10 V, or voltage direction.** The direct circuit audit identifies a 0–10 V potentiometer but does not map its electrical ends to visible lever travel. The observed similarity therefore cannot be used to substitute a particular RES voltage or Q value into either preset. Preset loading and the uncalibrated chip-control law are separate uncertainties.

The source audit is independent of the rejected joint-fit result. It supplies a control-position constraint only, not evidence that another calibration will succeed. Existing analyses using Flute BR=0 and Brass BR=−15 tested a declared asymmetric electrical scenario; that asymmetry was not recovered from these cap positions.

## Evidence scope and provenance

The earlier three requested Markdown notes and Brass visual JSON were read before reinspection. The original RES-voltage inference is explicitly withdrawn by its existing erratum. The full event extraction reportedly contains 41 frames; this review directly inspected the eleven-frame event contact sheet and the additional full frame, and does not claim a new frame-by-frame review of all 41. Likewise the Flute conclusions concern the four listed samples, not every frame or the intervening 68 seconds. The 212.40 s row is outside the lower comparison interval and was not used as its endpoint.

The retained extraction metadata gives 25 fps, 40 ms frame granularity and no independently measured audio/video offset. Coarse frame filenames can carry at least one-frame extraction uncertainty. Exact continuous control history, hidden interventions between passages, pressure, pedal state and control voltage remain unresolved. No original hardware MIDI is available.

All paths below are relative to `/tmp/ceres-cs50-1x8LGd`. SHA-256 values were recomputed for this review; originals were only read. The images were already-existing ordinary crops/resizes, with no new image enhancement.

| Read artifact | SHA-256 |
|---|---|
| `research/whitmore-modulation-controls.md` | `489401d1b4ee331e7587db9fd9ad014ac0fab04b5300920eb8fc99ba4972f076` |
| `research/whitmore-modulation-controls-erratum.md` | `064113c7e1ae18d1f8ae00177b78403f5beaf53e3642d8e941b531bc0d9c33b2` |
| `research/whitmore-brass1-modulation-visual.md` | `22b481f8a5dc89f6978f7e5a5efe8aecdc250bdffc48f3271271ca481ded7119` |
| `research/whitmore-brass1-modulation-visual.json` | `88a38aa59f5c72f3321f0ed917c012225feb535abf2c18d49f0ea11e114470c4` |
| `research/whitmore-modulation-controls/controls-contact-sheet.png` | `98bdcd7d84bf98d49c2fb9b1cd2ed0e1530da19d89db3885e33e883f9ff72b42` |
| `research/whitmore-modulation-controls/high_205.80-global.png` | `3dca8afef6176d175602bedc178c211b847674bc62e6a7f40030c5ac1f75cc3c` |
| `research/whitmore-modulation-controls/lower_210.90-global.png` | `56652b2d6a748b1a91f2ebee72ac5d74b2cbaa370e656944e83f591705b7796b` |
| `research/whitmore-brass1-modulation-visual/brass-event-controls.png` | `7dd0faa476b36ace678e1c27c0edfaa91597ab1b89004ce9b889dfccda25abd4` |
| `research/whitmore-brass1-modulation-visual/ring-brass.png` | `daa4749f09b260dfe2a0b3387249db557288e7c89b2fda1f76ef46d8105e6dae` |
| `research/whitmore-brass1-modulation-visual/137.00.png` | `38769d9482defba1a0edbb1c9f030249979ba11c66b30907a8fdeeee22f00e18` |
| `research/whitmore-frame-205.8.png` | `516d83cc1835a32e3116652fa79ee67a3fb54e363901f9dbb0ba071f14d142d3` |
| `research/cs-servicing-guide.txt` | `602674a552fb6ca2c45b3fa6f90af37878e60720b098b93c36aca75011b3f478` |
| `research/global-resonance-loading.md` | `34d14e4c46e4527b65f83deb58af2220053850e254c4965010ff99e86891e41f` |

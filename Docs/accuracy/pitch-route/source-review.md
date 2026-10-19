# Reconstructed Flute pitch: source review of two fixed routing hypotheses

19 October 2026. Source-only review; no new target measurement, model evaluation, audio render, or fit. This note preserves the preceding brightness-boundary failure and does not reinterpret its final callback as a calibrated candidate.

## What the primary circuit establishes

The direct CS-50 M schematic (service manual part 2, PDF page 12 / printed 16; `q-full-m.png`) separates three paths:

- **Shared keyboard/master reference:** K(37) reaches IG00153 oscillator KV pin 2 and both IG00156 filter KV pin 2 inputs.
- **Oscillator-only modulation:** V(36) reaches IG00153 VIB pin 15; this connection does not branch to the filter KV pins.
- **Oscillator-only transpose:** FT pin 1 changes oscillator range independently of those filter KV inputs.

R7 (part 1, PDF 8 / printed 14; `service1-08.png`) sums ordinary SUB VCO V1 and touch VCO V2 into each voice VB through separate 100 kΩ resistors. PN3 (part 2, PDF 21 / printed 27; `pitch-modulation-pn3-routing.png`) sends the common selected SUB waveform through independent VCO, VCF and VCA depth controls. SUB VCO reaches the VIB path; SUB VCF separately reaches HP/LP cutoff controls through 150 kΩ / 47 kΩ. Observing oscillator modulation therefore does not establish that the filter modulation depth is zero, or that only one physical route is active.

The earlier master-pitch trace follows PN2 VR1 → SUB I50 / buffered O49 → KAS TU67 → keyboard ladder → SH → M K37 (part 2 PDF/printed pages 20/26, 6/8, 7/9, 3/3 and 5/7). The servicing guide PDF 71 / printed 62 identifies PITCH as changing the ladder reference. Its filter law, PDF 43 / printed 34, is `fc = 125 * KV * 2^Vfc`; PDF 45 / printed 36 illustrates octave changes with KV. Consequently a **static master-pitch ratio** affects oscillator and filter variable transconductance together. This supports placing master tuning outside Ceres's adjustable, engineering key-tracking exponent. It does not resolve the instrument's sampling/update behavior under rapidly changing PITCH or released keys.

The guide PDF 36 / printed 27 gives a ground-centered VIB input and 50 cents / 1.5 V, while PDF 38 / printed 29 describes linear frequency modulation. The internal drawing (IC Guide PDF 35 / printed 2-4) omits the pin-15 circuit. Test key, FT setting and complete transfer are not identified. Do not convert the reconstructed bend into purported pin-15 volts or infer a universally calibrated exponential VIB law. Keeping the already reconstructed oscillator frequency trajectory unchanged avoids introducing either claim.

## What the recording establishes—and does not

The retained Flute visual review reports SUB VCO visibly raised relative to the adjacent low VCF/VCA caps. FUNCTION is unresolved; low caps do not establish exact zero. Touch pressure and simultaneous routes remain unknown. The accompanying erratum also prevents treating the low RES cap as a known electrical voltage. These observations make an oscillator-only component plausible; they do not identify its amount, exclusive cause, phase or DC mean.

There is no original hardware MIDI. The saved continuous bends are reconstructed pitch trajectories, not recovered hardware bend messages. Oscillator tuning/drift, SUB phase, pressure-dependent VIB bias, master PITCH and reconstruction error can contribute to an apparent constant offset. In particular, the primary touch-VCO circuit contains bias **before** controlled gain (guide PDF 67 / printed 58), so a mean oscillator-only displacement is possible without a changed master reference.

The frozen high-Flute event file's first bend is +0.367919921875 semitones (+36.7919921875 cents). The lower passage's first bend is +0.077392578125 semitones (+7.7392578125 cents). The difference is 29.052734375 cents. These are read from saved input files, not newly estimated from audio. Their difference is a reason not to label two per-clip initial values as one recovered master setting.

## Two fixed alternatives, with one common reference

Let `b(t)` be each passage's unchanged saved bend in semitones, `k` its existing engineering key-track amount, and `Kbase` the unchanged note/key-track term plus any existing master tune. The current shared-bend convention has

    Kold(t) = Kbase + k*b(t)/12.

Predeclare exactly these two alternatives, without fitting or recentering:

1. **All reconstructed bend oscillator-only:** `KA = Kbase`. The oscillator retains all of `b(t)`; the filter removes `k*b(t)/12`. This is a routing sensitivity, not a claim that a zero-centered SUB oscillator creates every DC offset.
2. **One common static master reference plus oscillator-only residual:** select `b0 = +0.367919921875` semitones from the first chronological saved **high-Flute** bend, and use that same value for **both** passages. `KB = Kbase + b0/12`; the oscillator is interpreted as common `b0` plus residual `b(t)-b0`, preserving its exact original trajectory. Relative to the old convention, `ΔKB = b0/12 - k*b(t)/12`.

The common `b0` is a deterministic reference hypothesis, **not measured DC or recovered master PITCH**. It is not the lower passage's first bend, a fitted mean/median, or a newly optimized control. A literal held-key-CV convention would instead give `k*(b0-b(t))/12`; that equals the master interpretation only when `k=1`. The retained Flute cases have `k=1`, but implementation tests at other key-track values must preserve this semantic distinction. Do not add `b0` again to any existing master-tune field.

Both alternatives modify only the filter's variable-gm factor. For the conditional full-network model this is

    fc = f_fixed + gm_anchor * 2^(Kroute + alpha*kappa*u).

The fixed resistor frequency floor stays outside the exponential. Existing transpose, Q/loading/coupling states, HP/LP envelope controls, brilliance, resonance, source/gain masks, note gates, oscillator phase trajectory and timestamps remain frozen. No new fitted control or selection between alternative offsets is justified here. Oscillator trajectory equality should be checked independently; complete audio equality is not expected after intentionally changing filter tracking.

## Interpretation and retained limits

The preceding brightness-boundary attempt stopped at its declared 80 distinct forward calls with no feasible point; its callback was diagnostic only. This routing study neither repairs that failure retrospectively nor authorizes restarting the fit. Use a separately declared existing control point and compare fixed routes without changing other controls.

Holding oscillator pitch constant while removing synthesized filter-KV movement isolates one benchmark assumption. It does not identify physical SUB versus touch versus master motion. Pitch motion through a static filter can itself change harmonic ratios; VCF modulation, envelope motion and unknown controls remain additional explanations. Either outcome is limited to these two explicitly chosen partitions, not a proof of the original instrument's routing or calibrated fidelity.

## Evidence identities

The ten primary PDF/image/note identities in `pitch-modulation-routing.json` relevant to this review were verified against their retained bytes: both CS-50 service PDFs, servicing guide, IC Guide, M and R7 images, PN3 routing crop, VCO-pin crop, and the two pitch-routing notes. No primary file was changed. The source manifest contains their original URLs and page/crop mappings:

- [CS-50 service scans](https://bama.edebris.com/manuals/yamaha/cs-50/)
- [Yamaha CS servicing guide](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf)
- [Yamaha IC Guide](https://bama.edebris.com/download/yamaha/ic_guide/Yamaha%20IC%20Guide%20Book.pdf)

SHA-256 bindings (absolute paths for archive inputs, repository-relative paths otherwise):

| Input | SHA-256 |
|---|---|
| `/tmp/ceres-cs50-1x8LGd/research/pitch-modulation-routing.json` | `97c49f83f18dbe61fdd3c46870a939aa88322395d2060dd5f9a3eebbbbcb4f81` |
| `/tmp/ceres-cs50-1x8LGd/research/pitch-modulation-routing.md` | `362dac857bb5227366b8869259dbc34aeb545731241b5da57361896f41b6e655` |
| `/tmp/ceres-cs50-1x8LGd/research/keyboard-tracking-and-master-pitch.md` | `d3b865472270f5c21d27e69d2f486c18e9836941127a82de8352a17b5d5b92a5` |
| `/tmp/ceres-cs50-1x8LGd/research/whitmore-modulation-controls.md` | `489401d1b4ee331e7587db9fd9ad014ac0fab04b5300920eb8fc99ba4972f076` |
| `/tmp/ceres-cs50-1x8LGd/research/whitmore-modulation-controls-erratum.md` | `064113c7e1ae18d1f8ae00177b78403f5beaf53e3642d8e941b531bc0d9c33b2` |
| `Docs/accuracy/plan.json` | `3482bca5ed0305c2a384125d05da73d19c17ed3c7f699e8cde262c045151071a` |
| `Docs/accuracy/inputs/calibration/whitmore-transpose/high-physical.events.tsv` | `d4f8d7749e38e5cc07d49300e14c206e679523a7b1c589ccb13f248c802da55d` |
| `Docs/accuracy/inputs/calibration/whitmore-transpose/lower-physical.events.tsv` | `fe9e6a9e45782cc099590ba2d0fa97a34f0a8cfb649d7ec77a25f84c894a2b2f` |
| `Docs/accuracy/brightness-boundary/checkpoint.json` | `ef2484f2b2111746ce6d5edcd3ef64544d75c32977e0fe04f3889eb2e243b63f` |
| `Docs/accuracy/brightness-boundary/RESULT.md` | `613cd5d4fc777daeffa930de680fa70b551f38d5ea2591ddddfb5b8d01d77b3c` |

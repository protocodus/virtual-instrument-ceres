# Ceres — decision log

Directions chosen by ear, or taken on the owner's word without a listen,
recorded per the A–Z listening-test convention in the repository's
`CLAUDE.md`. Each entry says which: a choice made by ear is recorded as made
by ear, and an unheard one as unheard.

## 2026-10-20: the owner chose B in every open set

The owner listened to the ten sets rendered for the component authenticity
pass and chose B in each ("use B", then "B meaning – use candidates
everywhere"). Every entry below is a choice made by ear, not a measurement.
In each set A was the shipping engine of the day; the sets themselves were
not committed, and each `key.md` held the rendering details summarised here.

### VCO sine shaper (`2026-10-20-vco-sine-shaper`)

- **Decided:** the IG00158 WSC sine transfer (STI to SIO), which Yamaha does
  not publish.
- **A:** an ideal shaper, sin(π/2·tri) on the folded ramp.
- **B:** an undegenerated bipolar differential pair, tanh(k·tri)/tanh(k) at
  k = 1.4020, the drive that minimises THD for an ideal triangle (1.36 %).
- **Chosen:** B.
- **Licensed:** the differential-pair shaper at its own minimum-THD drive,
  with the 0 dBm output reference and the preamp rails re-derived from its
  mean square. The shaper stays unmeasured: a public CS-80 sample set
  measured afterwards fits neither letter
  ([VCO research](research/vco-authenticity-2026-10-20.md)).

### VCF printed cutoff law (`2026-10-20-vcf-printed-law`)

- **Decided:** whether both filters follow the IG00156 cutoff law as printed
  (fc = 125 Hz/V · KV · 2^Vfc), zero point and slope together.
- **A:** the shipping estimates, 12 Hz (HP) and 180 Hz (LP) at middle C for
  0 V, then 0.7 and 0.65 oct/V, with no printed source.
- **B:** the printed law with Vfc read ahead of the pin-4 network: 1 oct/V on
  both levers, 62.5 Hz of gm at middle C for 0 V, and the filter EG at
  39k/220k (HP) and 39k/47k (LP) oct/V.
- **Chosen:** B.
- **Licensed:** the printed law in that reading, with the BRILLIANCE, SUB,
  touch and EG depths rescaled as 39k over their own input resistors. The
  reading of which side of the network Vfc is measured on is the by-ear
  part. It supersedes the zero-point and slope sets below. The shipped
  presets also solve their filter rows under the R7 load, a schematic
  correction made after this set was rendered, which the set did not contain
  ([VCF research](research/vcf-authenticity-2026-10-20.md)).

### VCF cutoff zero point (`2026-10-20-vcf-cutoff-zero-point`)

- **Decided:** where the cutoff sits for 0 V on the preset or panel bus.
- **A:** the 12 Hz and 180 Hz anchors. **B:** the printed 62.5 Hz zero for
  both stages, with the provisional slopes kept.
- **Chosen:** B.
- **Licensed:** the printed zero point, which ships as part of the printed
  law above.

### VCF HP against LP slope (`2026-10-20-vcf-hp-lp-slope`)

- **Decided:** whether the HP and LP levers move their stages by the same
  octaves per volt.
- **A:** 0.7 (HP) and 0.65 (LP) oct/V. **B:** both 0.7. **C:** both 0.65.
- **Chosen:** B.
- **Licensed:** equal slopes. The 0.7 value itself does not ship: the printed
  law above sets both to 1 oct/V.

### VCF Q law (`2026-10-20-vcf-q-law`)

- **Decided:** how damping moves between the printed endpoints, Q 5 at
  VQ 0 V and Q 0.5 at VQ 10 V.
- **A:** damping linear in VQ, d = 0.2 + 0.18·VQ. **B:** damping exponential
  in VQ, d = 0.2·10^(VQ/10).
- **Chosen:** B.
- **Licensed:** the exponential law; mid-travel resonance settings are more
  resonant than before (Q 1.58 instead of 0.91 at VQ 5 V), with the endpoints
  unchanged.

### Preset Q through the pin-7 divider (`2026-10-20-brilliance-resonance-preset-q-divider`)

- **Decided:** how a preset's Q column reaches IG00156 pin 7, which takes the
  preset bus and the RESONANCE bus through 220k each.
- **A:** the preset bus straight to the pin, r = 1 − P/10. **B:** the
  divider with RESONANCE at MIN (+10 V), r = (10 − P)/20. **C:** the lever at
  half travel. **D:** the lever at MAX.
- **Chosen:** B.
- **Licensed:** factory presets assume RESONANCE at MIN, so +10 V is the
  lever's neutral and front ends form `resonance_offset` as
  `resonanceLeverVolts − 10`.

### Board-to-board tolerance (`2026-10-20-voice-variation-board-tolerance`, revision 2)

- **Decided:** whether the documented board tolerances are on by default.
- **A:** four identical boards. **B:** pitch, filter and envelope amounts
  all at 1. **C:** filter and envelope only. **D:** pitch only.
- **Chosen:** B.
- **Licensed:** `board_pitch_tolerance`, `board_filter_tolerance` and
  `board_envelope_tolerance` default to 1, factory presets included. The set
  predates a correction to the VCA EG part: the ±2 % check now applies to
  the attack peak and the floor, and the full-sustain level is nominal on
  every board, as the VR14 trim leaves it. That part follows the alignment
  procedure, not the ear.

### Ring wet trim (`2026-10-20-ring-wet-trim`)

- **Decided:** how the PRA wet buffer's VR4 is trimmed; its factory setting
  is not printed.
- **A:** level-neutral by power, VR4 57.8k, gain 6.78. **B:** level-neutral
  by peak, VR4 38.0k, gain 4.80, 3.0 dB less wet.
- **Chosen:** B.
- **Licensed:** the peak-neutral trim. It is not a measurement of VR4.

### Ring release (`2026-10-20-ring-release`)

- **Decided:** which law turns the fixed 2DT voltage (4.901 V) into the ring
  envelope's release time.
- **A:** the M-board pin-sheet law unshifted, 0.468 s T60. **B:** the same
  law shifted by the PRA's own alignment (31 ms attack at AT = 10 V, CS-60
  procedure on the same board), 14.5 s T60.
- **Chosen:** B.
- **Licensed:** the PRA-aligned law for the release and for the ring
  ATTACK TIME and DECAY TIME levers. It assumes the PRA's fixed 100k gives
  the M board's 0.3 decade/V slope.

### Output rail anchor (`2026-10-20-vca-output-rail`)

- **Decided:** which load the 0 dBm HO anchor is read into, which sets where
  the output preamp clips. Removing the old tanh was decided by the parts,
  not by this set.
- **A:** the old `.9·tanh` rail. **B:** 0 dBm read at an unloaded HO
  terminal, rails about 12 dB above the reference. **C:** 0 dBm read into
  600 Ω, rails 5.7 dB lower.
- **Chosen:** B.
- **Licensed:** the unloaded reading, which Ceres already shipped in 0.5.0.

## 2026-10-21: the owner took every pending candidate, unheard

The owner had not listened to the open set and asked for all of its
candidates ("take all candidate changes from the listening test", then
"listening test was not performed … so pick all proposals"). Neither the ear
nor a measurement chose what follows; each candidate stands only on its own
sources.

### Board EG times and VCA balance (`2026-10-20-voice-variation-eg-time-vca`)

- **Decided:** whether the EG-time and VCA board amounts become default,
  together or alone.
- **A:** the shipping engine: pitch, filter and envelope tolerances at 1 and
  `drift_cents` 1. **B:** A plus each board's EG time-trim residual, both
  EGs' attack trims within ±5 % at 4 ms and 125 ms (CS-80 procedure), decay
  and release on the converter's log-time line through them. **C:** A plus
  each board's CP4 level-trim residual, VR11 sine ±2 % and VR12 filtered
  ±2.5 % (CS-80 procedure), with VR13/VR14 restoring the filtered path and VR6
  the four-sine 0 dBm reference (CS-60 procedure). **D:** both.
- **Chosen:** D, unheard.
- **Licensed:** `board_envelope_time_tolerance` and `board_vca_tolerance`
  default to 1, factory presets included (Ceres 0.6.1). `board_temperature_rise`
  was not a candidate and stays 0: a default would need an invented
  temperature.

### VCO sine shaper, kept

- After a public CS-80 sample set was found to fit neither letter of
  `2026-10-20-vco-sine-shaper` (B's 5th and 7th harmonics 3.5–20 dB too
  strong, A's 5th 1–11 dB too weak), the proposal was to keep B. The owner
  took it with the same instruction, without a new listen. The shaper stays
  as recorded above: chosen by ear, unmeasured.

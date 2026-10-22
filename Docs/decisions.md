# Ceres — decision log

Directions chosen by ear, or taken on the owner's word without a listen,
recorded per the A–Z listening-test convention in the repository's
`CLAUDE.md`, and the source readings that later superseded or refined them.
Each entry says which: a choice made by ear is recorded as made by ear, an
unheard one as unheard, and one a source settled as settled by that source.

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
- **Supported 2026-10-21:** the CS-30's VCF alignment moves its low-pass
  peak 0.96 octave per volt of the CUT OFF lever, ahead of the same kind of
  39k and rank-resistor network, with one octave per volt inside its
  tolerance ([sibling evidence](research/sibling-evidence-2026-10-21.md)).
  The choice stays one made by ear; the sibling supports its reading.

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
- **Superseded 2026-10-21** by the CS-80 procedure, which states the
  alignment voltage (below).

### Output rail anchor (`2026-10-20-vca-output-rail`)

- **Decided:** which load the 0 dBm HO anchor is read into, which sets where
  the output preamp clips. Removing the old tanh was decided by the parts,
  not by this set.
- **A:** the old `.9·tanh` rail. **B:** 0 dBm read at an unloaded HO
  terminal, rails about 12 dB above the reference. **C:** 0 dBm read into
  600 Ω, rails 5.7 dB lower.
- **Chosen:** B.
- **Licensed:** the unloaded reading, which Ceres already shipped in 0.5.0.
- **Refined 2026-10-21** by the CS-80 procedure, which names the meter's
  load (below).

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

## 2026-10-21: a sibling procedure settles two readings

The owner allowed evidence from sister instruments where the CS-50's own
documents are silent ("you can also explore sister synths (cs60 etc) if
original unit is unavailable"). The CS-80 Adjustment Procedure trims the same
PRA ring envelope circuit, and its output-level step names the meter and load
Yamaha's bench used: two conditions the CS-50 and CS-60 documents leave out.
Nothing below was chosen by ear; the source decides both, as sibling
evidence.

### Ring release, re-read (`2026-10-20-ring-release`)

- **Decided earlier, by ear:** B, the PRA law aligned to 31 ms at
  AT = 10 V, 14.5 s T60.
- **What the source says:** the CS-80 PRA is the same circuit (TC through
  100k, VR10 B-100K through 100k, 0.22 µF), and its procedure trims VR10 for
  31 ms ±5 % from key-on to the EO peak with ATTACK TIME set for +5.0 V on
  the A terminal (printed p.19). The CS-60 sheet's "Alignment by 31mS" names
  no voltage; 0.6.0 read it at the 10 V of the zeroing step drawn beside it.
  At 5 V the M-board pin-sheet law already gives 31.6 ms, so the trim leaves
  the PRA envelope on that law within 2 %.
- **Result:** the release is 0.459 s T60, within 2 % of letter A (0.468 s).
  B's reading is rejected by the source, and a by-ear choice cannot keep a
  reading the sources reject (the limit in `CLAUDE.md`), so B is not
  shipped. The owner's preference for the slower return is recorded here.
  The ring ATTACK TIME and DECAY TIME lever laws move with it (attack about
  1 ms–1 s, decay 13.5 ms–13.5 s T60); `ring_attack` and `ring_decay` are seconds and do not
  change. The 0.3 decade/V slope stays an assumption, but at 0.1 V from the
  trim point it moves the release by 2.2 % at most.

### Output rail anchor, refined (`2026-10-20-vca-output-rail`)

- **Decided earlier, by ear:** B, 0 dBm read at an unloaded HO terminal.
- **What the source says:** the CS-80's output-amplifier step reads the
  output jack on an AC voltmeter into a 10 kΩ load: 0.78 V RMS (0 ± 3 dB),
  sine lever, SUSTAIN and LEVEL at 10 (printed p.29).
- **Result:** HO drives 10 kΩ through its 560 Ω, so the rails sit 0.47 dB
  below B's; C's 600 Ω would have been 5.3 dB lower still. This keeps B's
  direction, a high-impedance RMS reading, and fixes its load. Still
  assumed: that the CS-50 was trimmed on the same bench load, which neither
  CS-50 source names.

## 2026-10-21: the boards part away from their trim point

Not a listening choice: the sources decide it. Verdict B on
`2026-10-20-voice-variation-board-tolerance` (revision 2) turned the
documented filter spread on by default, on a model whose static cutoffs were
identical on every board. The term that moves them apart away from the trim
setting was listed as a gap then, for want of the cutoff's volts.

- **Decided:** where each board's static cutoff sits when its lever is away
  from the point the boards were matched at.
- **What the sources say:** VR7 and VR9 are offsets on the Vfc node, matched
  by ear with each filter's lever at its centre (CS-60 SM printed p.38,
  III-IV, sibling evidence for the shared M board), and the rank residual
  scales every input on that node, the lever's volts included. The 0.6.0
  printed law, fc = 125 Hz/V * Kv * 2^Vfc, gives those volts.
- **Result:** at `board_filter_tolerance` 1 a board's gm is
  gm_c * (gm/gm_c)^s about the lever centre's gm; Q, and a cutoff with its
  lever at the centre, stay every board's (Ceres 0.6.3). The amount still
  runs from 0, identical boards, to 1. The verdict chose the documented
  spread; this adds the part of it 0.5.0 could not express, unheard in that
  set. A labelled before/after preview, not a blind test, is at
  `/mnt/project-files/ceres/listening/2026-10-21-board-static-cutoff-preview/`
  (not committed).

## 2026-10-21: every plug-in instance is its own unit

Not a listening choice: the owner asked for it ("you can randomize within a
tolerance on every start"), closing the gap that every plug-in instance was
the same unit.

- **Decided:** which unit a plug-in instance is.
- **Before:** every instance was seed 20261017, the README demos' unit.
- **Result:** a new instance draws its seed at random (Ceres 0.6.4). The seed
  only picks where each board falls; the tolerances and their amounts bound
  the offsets as before. The saved state keeps the unit, so a saved session
  reopens as the same unit, and a state that names none, saved before 0.6.4,
  opens as seed 20261017. The demos and the parameter-extremes matrix keep
  that fixed unit. Keeping the unit when a session reopens is the reading
  recommended to the owner, who was asked to choose between it and a new
  unit on every load.
- **Every voice (2026-10-22):** the owner asked that all voices be
  randomized, depending on the voice count. Each voice slot up to 16 was
  already a board of its own, drawn from the same bands after the four
  physical boards, and the tests now check every slot. The PRA's VR6 stays
  the four physical boards' trim, so a further board keeps its own sine
  error. `voice_spread_cents`, a fixed engineering spread, was centred only
  at four voices and is now centred on the voices in use.

## 2026-10-22: the schematics settle two circuit readings

Not a listening choice: the sources decide both. Each refines a direction
chosen by ear on 2026-10-20 without changing what was chosen.

### Preset Q through the pin-7 divider, re-read (`2026-10-20-brilliance-resonance-preset-q-divider`)

- **Decided earlier, by ear:** B, the divider with RESONANCE at MIN,
  r = (10 − P)/20, on a pin 7 taken to draw no current.
- **What the sources say:** the CS-30 drives the same IG00156's VQ through
  22k into a 390–470 Ω rank shunt (CS-30 SM pp.8–9) and prints Q 0.5 at
  10 V, as the CS-50 does through its 220k pair. A pin that read voltage
  would see at most 0.2 V on the CS-30 against 10 V on the CS-50, so it
  takes current. The CS-15's CPB board (CS-15 SM printed p.10) and the
  CS-40M's FA board (overall circuit diagram) wire VQ as the CS-30 does,
  and the CS-80's R51·52 board (SM printed p.39) feeds its pins through the
  CS-50's 220k pair. Equal current at the two printed 10 V points puts the
  input at four times the shunt, 1.72 kΩ at rank B: an inference, not a
  datum.
- **Result:** the pin current is (P + R)/2 through 110k plus the input, so
  B's law reads exactly as before. What changes is the network around it
  (Ceres 0.6.5): the RESONANCE lag is 17 ms instead of 1.1 s, each preset Q
  row sees 55.4k returning to 0.78 % of RES instead of 110k to RES, so a
  zero column sits at 0.012 V instead of 0.83 V and factory Q rises by up
  to 10 %, and the RESONANCE lever sees 27.7k instead of 55k. The inferred
  value mostly sets the lag: an input anywhere from 0.5 to 5 kΩ would put it
  between 5 and 48 ms.

### VCO sine shaper, coupled (`2026-10-20-vco-sine-shaper`)

- **Decided earlier, by ear:** B, the differential-pair shaper
  tanh(k·tri)/tanh(k) at k = 1.4020, auditioned on the triangle as TO
  delivers it.
- **What the source says:** the CS-50's M board couples TO into STI through
  4.7 µF NP with 22k to ground (SM2 p.16), so STI is the triangle less its
  mean, a 1.54 Hz high-pass into the pair's base, which the 22k biases.
- **Result:** the coupling now sits in front of B's shaper, which is
  unchanged (Ceres 0.6.5). The set omitted it. It matters where the reset
  plateau makes the triangle lopsided, a mean of 2r² of peak: 3 % at C6 on
  8', 29 % at the 6.33 kHz top. The sine's second harmonic moves by up to
  5 dB, and a large pitch jump re-centres STI over about 0.3 s. The shaper
  stays chosen by ear and unmeasured.

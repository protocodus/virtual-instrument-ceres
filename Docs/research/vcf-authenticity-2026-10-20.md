# VCF authenticity pass (2026-10-20)

Scope: the two IG00156 stages (`OtaSvf`, `FilterChain`), the cutoff control
law (`modulatedFilterFrequency`, key tracking, the preset CV conversion), the
Q law and the input/saturation scales. Score before this pass: 6/10.

Page references are printed page / PDF page. Sources: Yamaha CS Series
Servicing Guide ("Guide"), CS-50 Service Manual 1 and 2 ("SM1", "SM2"), the
CS-50 Overall Circuit Diagram ("Overall"), and, as labelled sibling evidence,
the CS-60 Service Manual ("CS-60 SM", same M-board family).

## What the sources fix

**The IG00156 law.** Guide p.34 / PDF 43: `fc = 125 · Kv · 2^Vfc`, "Kv: key
voltage, Vfc: cut-off frequency control voltage". SM1 p.19 / PDF 9 (VCF IC
pin sheet): pin 2 KV is 0.25–4.0 V; pin 4 Vf "When the VK is 0.25 V and Vf is
5 V, the cut off frequency is set to just 1 kHz", which is the same law
(125 · .25 · 2^5 = 1000). Pin 6 Q0 sets the standing Q current that gives
Q = 5 at VQ = 0; pin 7 VQ: 0 V → Q 5, 10 V → Q 0.5.

**Kv is the oscillator's key voltage.** SM2 p.16 / PDF 12 (M circuit): the K
terminal (37) feeds VCO III pin 2 and both IG00156 pin 2s directly. SM1 p.19
VCO III table: 0.25/0.5/1/2/4 V → 130.8/261.6/523.2/1046/2093 Hz (Yamaha's
C2..C6, transposition NORMAL). So Kv is linear in key Hz, and the cutoff
follows the key exactly one octave per octave. TRANSPOSITION is a current into
VCO III pin 1 (FT) only (SM1 p.19; Guide p.65 tuning steps 2–5), so it moves
the oscillator and not the filters. Glide (KAS integrator, before the SH) and
PITCH (the octave-ladder reference, Guide p.62) are inside K and move both.
The engine already does all of this (`keyTrack = 1`, `octave` VCO-only,
glide/PITCH in `keyOctaves`); `Tests/VcfKeyVoltageTests.cpp` now holds it to
the printed Kv table.

**The zero point.** With every input at 0 V and the trim at mid-travel, pin 4
sees no current, so Vfc = 0 and the printed law gives a gm cutoff of
125 · Kv = 62.5 Hz at middle C (Kv = 0.5 V) for BOTH stages, at every key 2.06
octaves below the fundamental. The trim (VR7 HP / VR9 LP, B-100k from +15 V to
−15 V into the node through 330k on SM2 p.16; 150k on Overall and on the CS-60
SM p.17 M circuit) is centred on 0 V. The preset conversion instead anchors the
variable-gm part at 12 Hz (HP) and 180 Hz (LP) for 0 V; neither number has a
source. Guide p.35 / PDF 44 ("cut-off effects", LPF lever at its lowest: "the
fundamental is also cut off, sound eliminates") agrees qualitatively with
62.5 Hz than with 180 + 22.6 Hz: at Q 0.5 and middle C the fundamental is
25 dB down with the first and 8.5 dB down with the second (two coincident
poles, |H| = 1/(1 + (f/fc)²)).

**The cutoff node.** SM2 p.16 and p.18 / PDF 12, 14: pin 4 has a rank resistor
to ground (R6 HP 2.7k/2.2k/1.8k for chip ranks A/B/C; R7 LP 3.3k/2.7k/2.2k on
NA03574, 3.0k/2.4k/2.0k on NA03655; round 2: p.16 heads the same two columns
"CS80" and "CS50,60", so the CS-50's R7 is 3.0k/2.4k/2.0k), 0.01 µF to
ground, the R7-board inputs
(SM1 p.14 / PDF 8: FH 39k, BR 680k, SUB/TBR/VCF 150k each; FL 39k, BR 220k,
SUB/TBR/VCF 47k each), the trim, and the filter EG through 220k (HP) and 47k
(LP) on Overall. The SM2 M sheet draws the EG line straight onto the HP node
and omits it from the LP node; the CS-60 sibling (CS-60 SM p.17) shows the EG
buffer through 100k to HP and 47k to LP. The CS-50's own Overall value, 220k,
is kept.

The Guide's R7 reference (p.59 / PDF 68: every input at +10 V, outputs into
1 kΩ) is reproduced by these values for fH (0.45 V) and for VB, QH and LC, but
not for fL: the five LP resistors give 0.86 V, the Guide prints 0.64 V. The
schematic is followed.

**The Q block.** Guide p.33 / PDF 42: the Q path is a second "variable gm
circuit" (gm = K · Icont, p.33) from BP to FB, driven by a "Q control" block
that takes pin 6 (standing current, trimmed by VR8/VR10 B-50k to +15 V) and
pin 7 (VQ, direct). The damping that block adds is proportional to its
control current, so the shipping law, damping linear in VQ between 0.2 and
2.0, is exactly what a linear VQ-to-current converter gives. The sheet labels
KV "EXP input" and Vfc "Linear input" and leaves VQ unlabelled, so an
exponential VQ converter through the same printed endpoints is also
derivable.

**Alignment.** Neither the CS-50 Guide's tuning method (p.65 / PDF 74) nor its
SM gives a VCF calibration. The CS-60 SM section VI (sibling evidence, PDF 40)
trims VR7 (cutoff) and VR8 (Q) "so that the tone color of the M circuit board
may be the same as that of other M circuit boards" with the HPF lever at its
centre, and VR9 (CS-60 numbering) for LP Q, by ear. It sets board matching,
not an absolute cutoff or slope.

## What the sources do not fix

**Lever slope (oct/V).** A 10 V lever through 39k into the rank resistor puts
about 0.50 V (HP, rank B, every other source at 0 V and the EG path included)
or 0.53 V (LP, rank B NA03574) on pin 4. At the pin sheet's 1 V per octave
that is half an octave of travel, which cannot be the panel's range; so pin 4
in circuit is not the pin sheet's stiff 1 V/oct input, and the IG00156's
pin-4 input current law is not published. One hobby build (Eddy Bergman,
"Synthesizer Build part-65", 2025) drives pin 4 with 0–0.21 V into 470 Ω for
its full range, but the same page treats pin 2 as an exponential input, which
contradicts Yamaha's linear 0.25–4 V KV, so it is not used. The .7/.65 oct/V
slopes therefore stay provisional. Their ratio to the BR, SUB, touch and EG
paths (39k/Rx) is a resistor ratio and stays exact.

**HP against LP slope.** The rank shunts are paired so the 39k lever reaches
both pin 4s with nearly the same gain. Counting every input on each node
(R7 board, 330k trim, EG 220k HP / 47k LP), the HP/LP gain ratio is 0.991,
0.960, 0.939 for ranks A/B/C with the NA03574 LP column and 1.063, 1.051,
1.013 with NA03655; without the EG paths 0.951/0.926/0.911 and
1.023/1.018/0.986. Exact equality needs LP shunts of 3.26k/2.56k/2.03k
(EG counted) or 3.09k/2.45k/1.97k (not counted), against the printed
3.3k/2.7k/2.2k and 3.0k/2.4k/2.0k. Both chips are the same part and rank, so
the HP and LP levers move their cutoffs by the same octaves per volt to within
about ±6 %. The shipping .7/.65 split (1.077) is outside that band except for
the NA03655 rank-A case. (First noticed by the BRILLIANCE/RESONANCE pass;
recomputed here with the EG paths included. Round 2 narrows this to the
CS-50's own column; see below.) Which common value is right is
the unsourced absolute slope above, so the split is put to a listening test
and kept until a verdict. Any change must move every consumer of the two
slopes together: `hpBrillianceScale`/`lpBrillianceScale`,
`hpSubRelativeScale`, the preset `hp/lpEnvelopeScale`, and the SUB pass's
`subFilterLeverOctaves` (.65·39/47).

**Q bus divider.** Each Q pin node takes the preset/lever Q column through
220k and the shared RES bus through 220k (SM1 p.14), with 10 µF to ground.
If pin 7 draws negligible current the pin sees the average of the two, so a
single 0–10 V column spans only half of the printed 0–10 V VQ range around
whatever the RES bus sits at. The preset conversion `(10 − cv)/10` reads the
column as VQ directly. The RES bus's neutral voltage belongs to the
BRILLIANCE/RESONANCE pass, which has rendered this as its own listening test.

**Input voltage scale and saturation.** The mixer, its 150k branches, the
33k AI shunt and the 1 µF couplings are modelled; the IG00156's internal
AI-to-OTA attenuation and the OTA linear range are not published, so
`.3·drive` and the unit tanh voltage remain unidentified.

**Q converter law.** As above.

## Listening tests (rendered, verdicts pending)

Both under `/mnt/project-files/ceres/listening/`, each with `key.md`:

- `2026-10-20-vcf-cutoff-zero-point/`: the shipping preset conversion against
  the printed 62.5 Hz zero point with the same provisional slopes. Only the
  zero point is sourced; because the slopes are not, the preset cutoffs in
  both letters still depend on an unsourced number, so the shipping
  conversion is kept until a verdict.
- `2026-10-20-vcf-q-law/`: damping linear in VQ (shipping) against
  exponential in VQ, same endpoints.
- `2026-10-20-vcf-hp-lp-slope/`: the shipping .7/.65 split against both
  stages at .7 and both at .65 oct/V.
- `2026-10-20-vcf-printed-law/` (round 2): the shipping conversion against
  the printed law in full, read at the 39k lever (62.5 Hz gm at middle C for
  0 V, 1 oct/V on both levers, the EG at 39k/220k and 39k/47k oct/V). A
  choice of B would supersede the zero-point and slope sets.

## Second search (sibling and clone sources)

Searched 2026-10-20 for published IG00156 measurements: the Pilve iG00156AP
clone (2026), CS-80 restorer and DIY material, and Yamaha's own IC
documentation. Nothing measured on an original chip turned up.

- Yamaha "IC Guide Book" (synthesizer, combo keyboard, P.A.; scan at
  manuals.fdiskc.com), section 2-5 / PDF 37: IG00156 "(+) VCF", made by
  Mitsubishi. It prints the same block diagram as Guide p.33 and the same
  pin list. It gives no transistor-level schematic and no numbers. Its CS-5
  application (PDF 38) has the same arrangement as the CS-50: a 39k
  cutoff input into the VF pin, with a rank resistor R1 to ground, and a
  rank resistor R2 on the VQ pin as well. This supports the rank-shunt
  reading above, but gives no transfer.
- Pilve iG00156AP press coverage (Synth Anatomy, MATRIXSYNTH, 2026-08): it
  says the chip was "re-engineered from the original" and has a "linear
  cutoff voltage control input and an exponential keyboard-follower input".
  That is the Guide's own labelling. No datasheet, measurements or
  sensitivities are published, and a clone's design values would not be
  original-chip evidence anyway.
- Juergen Haible's CS-80 VCF SPICE analysis and the ModWiggler threads
  ("Reverse Engineering Yamaha's IG00156", "Yamaha CS Filter Analysis")
  could not be retrieved from this environment: the hosts are gone, and
  the archive and forum returned 403. As quoted elsewhere, the ModWiggler
  analysis is an idealised model (about 23 Hz offset, 7.6 kHz pole, gain
  1.7), not a bench measurement.

## Round 2 (later on 2026-10-20)

### Where the printed Vfc is measured

Guide p.33 / PDF 42 draws pin 4 fed from "Vfc" through a variable-resistor
symbol (pin 3 through a fixed resistor to ground, pin 6 through a variable
resistor, pins 2 and 7 direct). So the `fc = 125 · Kv · 2^Vfc` of p.34, and
the pin sheet's "VK 0.25 V and Vf 5 V gives 1 kHz" (SM1 p.19), describe the
control voltage ahead of a set resistance, not the pin voltage. That removes
the contradiction noted above: pin 4 is far more sensitive than 1 V/oct,
and the resistance in front of it is the rank-selected divider. On the CS-50
that divider is the 39k lever resistor into R6/R7, so read at the lever the
printed law gives 1 oct/V on both levers and the 62.5 Hz zero point, every
other input at 39k/Rx of that.

It is not adopted, for two reasons:

- The series reference behind the printed "Vf" is not printed. Rank shunts
  imply that Yamaha set the lever gain per chip, but not that the IG00156
  test circuit used the 39k lever path.
- With the printed zero, it moves the factory HP settings (Guide p.64 /
  PDF 73, column g) up 1.3–4.1 octaves. At middle C the HP lands at 1.7 kHz for
  string1 (5.00 V), 2.0 kHz for clavichord (5.24 V) and 4.2 kHz for
  harpsichord (6.41 V); shipping puts them at 141, 155 and 249 Hz. In the
  middle of the range the LP lands within about 0.4 octave of shipping.
  Whether Yamaha voiced its strings that thin is a question for the ear, so
  this went to the listening test above (`2026-10-20-vcf-printed-law`).

### HP against LP, CS-50 column only

SM2 p.16 heads the rank table's two R7 columns "CS80" and "CS50,60" (p.18
labels the same columns NA03574 and NA03655). So the CS-50 has R6
2.7k/2.2k/1.8k against R7 3.0k/2.4k/2.0k. Over the R7 inputs and the trim
(330k or 150k agree to 0.001), the HP/LP lever-gain ratio by rank
A/B/C is:

| EG paths included | Rank A | Rank B | Rank C |
| --- | --- | --- | --- |
| EG 220k HP / 47k LP (CS-50 Overall) | 1.063 | 1.051 | 1.013 |
| EG 100k HP / 47k LP (CS-60 sheet) | 1.049 | 1.039 | 1.004 |
| No EG paths | 1.023 | 1.018 | 0.986 |

The LP shunts that would make the two gains exactly equal, without the EG
paths, are 3.09k/2.45k/1.97k. Their nearest E24 values are exactly the printed
3.0k/2.4k/2.0k. With the EG paths counted, E24 rounding would give
3.3k/2.7k/2.0k (220k) or 3.3k/2.4k/2.0k (100k).

So the CS-50/60 LP column reads as a deliberate equal-gain design, rounded to
E24, with the EG paths left out of the calculation. Equal HP/LP slopes are
the design intent. The as-built HP slope runs 0–6 % steeper with the EG
loading. The shipping split, 1.077, sits 1.3–7 % above that band.

The common absolute value is still unsourced unless the printed-law reading
above is accepted. Equal slopes are therefore not implemented this round.
The consumers to move together are unchanged:

- `hpBrillianceScale` and `lpBrillianceScale`
- `hpSubRelativeScale`
- the preset `hp/lpEnvelopeScale`
- `subFilterLeverOctaves`

### Other printed details

- **Pin sheet output levels.** SM1 p.19 labels the LP output "−6dB/1V" and
  the BP output "−12dB/1V". A BP peak 6 dB below the LP passband is Q = 0.5,
  which is consistent with Q = BP peak / LP passband and the minimum-Q end
  of VQ. The test network is not printed, so these labels give no input
  scale.
- **The fL reference.** Guide p.59's fL = 0.64 V equals, to three figures,
  the R7 LP network with the FL 39k left out: BR 220k plus three 47k into
  1 kΩ gives 0.640 V. With FL in, the network gives 0.859 V. Its fH = 0.45 V
  does include FH 39k (0.450 V). The cause is unknown; the R7 schematic
  (FL 39k) is followed.

### Third search (patents, mirrors, recordings)

Nothing measured on an original IG00156 was found.

- **Google Patents.** I scanned every Yamaha (Nippon Gakki) filing with
  priority 1974–79 that matches "filter" and "control voltage": 196
  documents, searched by keyword for filter-control terms. A search for
  Mitsubishi "voltage controlled filter" filings in the same window returned
  nothing. None of these describes the IG00156 internals.
  - JP S59-49726 B2 (priority 1976-01-12, inventor Nagahama) is a Yamaha
    voltage-controlled resistor and VCF built from gm cells. Their tail
    current comes from a linear V-to-I amplifier, gm ∝ Ic ∝ Vc (figs. 5–6).
    That is sibling design practice in favour of the shipping linear Q law,
    not a measurement.
  - JP S59-21546 Y2 (1977) is a diode-bridge filter and does not apply.
- **Haible.** "Analyzing the CS-80 VCF" (J. Haible 2001, retrieved from
  jhaible.com/legacy) is a linear SPICE model. Its gm and gq are free
  parameters ("10uS == 1kHz", "approx. 20uS == minimum Q"). Its external
  network, 4.7M, 1.5n, 33k, 1.2M ∥ 180p, 150k and 1µ, is already the
  modelled one. It gives no control law. It drives the signal into the
  op-amp's feedback node, whereas the Guide, the IC Guide Book and the CS-50
  M sheet all put the input on AI, shared with FB. The model keeps Yamaha's
  drawing. The servicing-guide images on the same site returned 403 from the
  origin; they are pages of the Guide used here.
- **MOTM-480 manual** (Synthesis Technology, 2005). It is a re-design ("the
  exact circuit is not copied") that keeps the 1.2M ∥ 150p Q network and the
  HP-into-LP cascade. It is not original-chip evidence.
- **Other sources.** These gave nothing usable:
  - cs80.com adjustment and service-bulletin pages: origin 403.
  - The KVR "Yamaha CS filter: analysis" thread (2006): no figures.
  - Old Crow's NE10500 notes: they describe the GX-1 filter.
- **Recordings.** No public CS-50/60/80 recording with stated lever
  positions turned up, so no cutoff measurement could be taken from one.

## What would close the gap

1. A measured CS-50 (or CS-60/CS-80 IG00156) cutoff against lever voltage at
   one key: two points fix the slope; with the printed zero point that
   closes the panel-to-Hz law.
2. Pin-4 input characteristic of the IG00156 (voltage/current at the pin
   against cutoff), or any chip-level datasheet. The test circuit behind
   the pin sheet's "Vf 5 V gives 1 kHz" would settle the printed-law reading.
3. Q against VQ at three or more voltages on one chip, to choose the Q
   converter law.
4. Signal amplitude at IG00156 pin 15/10 for a known WSC input, or the onset
   of audible OTA compression at a known input level, for the input scale.

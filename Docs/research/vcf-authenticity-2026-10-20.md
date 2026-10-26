# VCF authenticity pass (2026-10-20)

Scope: the two IG00156 stages (`OtaSvf`, `FilterChain`), the cutoff control
law (`modulatedFilterFrequency`, key tracking, the preset CV conversion), the
Q law and the input/saturation scales. Score before this pass: 6/10.

The sections run in the order the work was done. Round 3 below supersedes
the earlier ones where they differ: the slopes, zero point and Q law are
no longer provisional, and the preset rows are now solved under load.

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

## Listening tests

All under `/mnt/project-files/ceres/listening/`, each with `key.md`. The
owner chose B in all four later on 2026-10-20; see Round 3.

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

## Round 3: owner verdicts (later on 2026-10-20)

The owner chose B in every listening set ("B meaning - use candidates
everywhere"). These are choices made by ear between candidates that each
rest on printed values; no measurement decided them, and the coordinator
records the verdicts in `Docs/decisions.md`.

### The printed cutoff law (`vcf-printed-law` B)

Factory cutoffs now follow `fc = 125 · Kv · 2^Vfc` in full: 62.5 Hz of gm at
middle C for 0 V and 1 oct/V on both levers. This supersedes
`vcf-cutoff-zero-point` and `vcf-hp-lp-slope`, whose B letters pointed the
same way.

- **Printed:** the law (Guide p.34 / PDF 43; SM1 p.19 / PDF 9, "VK 0.25 V,
  Vf 5 V → 1 kHz"); the 39k lever and every other input resistor (SM1 p.14 /
  PDF 8; Overall); the CS-50 rank shunts (SM2 p.16 / PDF 12).
- **The one inference:** Vfc is measured at the FH/FL lever, ahead of its
  39k into pin 4 (Guide p.33 / PDF 42 draws Vfc reaching pin 4 through a
  resistor). Neither the series reference behind the pin sheet's Vf nor
  pin 4's own input law is printed.
- **One slope for both levers:** the rank shunts are the E24 values nearest
  equal lever gain (Round 2). As built, the HP slope may run up to 6 %
  steeper with the EG paths; the model uses the design value.

Every consumer of the old .7/.65 oct/V slopes moved with it:

| Constant | Was | Now |
| --- | --- | --- |
| `hpBrillianceScale` | .7 · 39/680 | 39/680 |
| `lpBrillianceScale` | .65 · 39/220 | 39/220 |
| `hpSubRelativeScale` (SUB, touch, `pressure_wah`) | (.7/.65) · 47/150 = 0.3374 | 47/150 = 0.3133 |
| preset `hpEnvelopeScale` | .7 · 39/220 | 39/220 |
| preset `lpEnvelopeScale` | .65 · 39/47 | 39/47 |
| `subFilterLeverOctaves(1)` | .65 · 39/47 · 1.5 = 0.809 | 39/47 · 1.5 = 1.245 |

The SUB pass's figures of 0.809 LP and 0.273 HP octaves per SUB unit at full
lever (`sub-authenticity-2026-10-20.md`) are the old slope's; at 1 oct/V they
are 1.245 and 0.390.

Factory cutoffs at middle C, total Hz including the 22.6 Hz feed floor, from
the diode-loaded column g/i pin volts (Guide p.64 / PDF 73):

| Preset | HP was | HP now | LP was | LP now |
| --- | --- | --- | --- | --- |
| string1 | 141 | 1676 | 7264 | 18408 |
| string2 | 57.6 | 312 | 2285 | 3093 |
| brass1 | 34.6 | 85.1 | 2067 | 2649 |
| brass2 | 55.2 | 283 | 952 | 803 |
| flute | 43.3 | 159 | 1698 | 1956 |
| electric_piano | 44.2 | 168 | 368 | 193 |
| clavichord | 155 | 1956 | 2067 | 2649 |
| harpsichord | 249 | 4166 | 5121 | 10738 |
| guitar1 | 92.9 | 803 | 2285 | 3093 |
| guitar2 | 60.5 | 346 | 593 | 391 |
| funky1 | 141 | 1676 | 2285 | 3093 |
| funky2 | 187 | 2649 | 4115 | 7662 |
| funky3 | 97.8 | 884 | 1389 | 1436 |

Fixtures that encoded the old slopes now encode the printed one, each with a
comment saying so: the touch and SUB HP ratio (`ExpressionTests`,
`HardwareRealismTests`, `SoundPathTests`), the BR and SUB nodal sums
(`SoundPathTests`), the SUB VCF lever (`SubOscillatorTests`) and the factory
EG scales (`dsp_tests`). `VcfKeyVoltageTests` adds a contract that every
factory cutoff equals the printed law at its independently solved pin volts,
and the pin sheet's 1 kHz check point through key tracking.

### The Q law (`vcf-q-law` B)

Damping is now exponential in the pin-7 voltage through the printed
endpoints, `d = 0.2 · 10^(VQ/10)` (Q 5 at 0 V, Q 0.5 at 10 V; SM1 p.19 /
PDF 9), in place of the linear `d = 0.2 + 0.18 · VQ`. In the engine's
coordinate r = 1 − VQ/10 that is `d = 2 · 10^(−r)`, applied in `OtaSvf`
before the source-loading correction, so the RESONANCE offset moves VQ
itself. At VQ 5 V Q is 1.58 (was 0.91); at 2.5 V, 2.81 (was 1.54).

- **Printed:** the endpoints, and the variable-gm Q block (Guide p.33 /
  PDF 42).
- **Chosen by ear:** the converter law between them. The JP S59-49726 B2
  cell uses a linear converter; that remains sibling practice and does not
  identify the IG00156's.
- **Fixtures:** `dsp_tests`' physical-branch transfer functions took the
  linear damping at r = 0.5. They now take the printed endpoints'
  geometric interpolation, written independently of the engine's form.
  The r = 0 and r = 1 cases are unchanged. Adding a
  `std::pow` per stage per sample cost about 3 % in a paired run of
  `Tools/native_performance.cpp` (4 voices, 4× oversampling, noisy shared
  machine).

### The preset Q divider (`brilliance-resonance-preset-q-divider` B)

This set belongs to the BRILLIANCE/RESONANCE pass, but its change sits in
the same `preset()` block, so it is implemented here. Each pin 7 takes its
preset Q bus P and the RES bus through 220k each with 10 µF to ground
(SM1 p.14 / PDF 8; SM2 p.16 / PDF 12), so it settles at their mean. Factory
presets now assume the RESONANCE lever at MIN, where TVR6 sits on the +10 V
bus (SM2 p.26 PN2 sheet): `r = (10 − P)/20`, in place of the bus going
straight to the pin, `r = (10 − P)/10`, which no single lever position
gives for every preset.

- **Printed:** the divider and the lever wiring.
- **Chosen by ear:** the lever position the factory voicing assumed.
- **Consequences:** every factory pin 7 now sits between 5 V (Q column 0)
  and 9.71 V (a 10 V column, 9.42 V after its diode). With the exponential
  law the two changes nearly cancel for high columns, while a 0 V column
  goes from Q 5 to Q 1.58 (guitar2 both stages, funky1 LP, funky2 both).
  `resonance_offset` is now the RES change from MIN: the lever reaches 0 to
  −10 V, and positive values have no lever position. A front end that shows the lever sets it to
  `resonanceLeverVolts(t, mean bus) − 10`, with each preset bus at
  `10 − 20 · r`. The BRILLIANCE/RESONANCE note's inverse `P = 10 · (1 − r)`
  belongs to the old mapping.
- **Fixture:** `VcfKeyVoltageTests` checks every factory Q anchor against the
  divider at independently solved pin volts. No existing fixture encoded the
  old mapping.

Factory Q (HP / LP), before both Q changes and after them (linear law with
the bus on the pin, against the exponential law through the divider):

| Preset | HP was | HP now | LP was | LP now |
| --- | --- | --- | --- | --- |
| string1 | 0.80 | 0.81 | 0.53 | 0.53 |
| string2 | 0.53 | 0.53 | 0.53 | 0.53 |
| brass1 | 0.53 | 0.53 | 0.85 | 0.85 |
| brass2 | 1.17 | 1.04 | 0.65 | 0.67 |
| flute | 1.24 | 1.07 | 1.67 | 1.23 |
| electric_piano | 0.78 | 0.79 | 1.30 | 1.10 |
| clavichord | 0.83 | 0.83 | 1.03 | 0.96 |
| harpsichord | 0.53 | 0.53 | 0.61 | 0.63 |
| guitar1 | 0.88 | 0.87 | 1.60 | 1.20 |
| guitar2 | 5.00 | 1.58 | 5.00 | 1.58 |
| funky1 | 0.80 | 0.81 | 5.00 | 1.58 |
| funky2 | 5.00 | 1.58 | 5.00 | 1.58 |
| funky3 | 1.60 | 1.20 | 1.36 | 1.12 |

These are nominal chip Q at the pin, before the source-loading correction.

### The preset filter buses are loaded (schematic, not by ear)

The round-1 conversion read each factory column as the T-board divider into
its row's own 10k alone, on the premise that the destinations are op-amp
inputs, and the preset EG note left "four-voice loading" open. The board
wiring lists settle it for the four filter rows.

- **Route.** The board wiring lists print it: R7 FH (19) ← T72-g (27),
  FL (20) ← T72-i (24), QH (52) ← T72-h (26), QL (50) ← T72-j (23) (SM2
  p.15 / PDF 11), and T71 g (27) → T72-g (27), one wire, the T-boards'
  outputs being bussed (SM2 p.11 / PDF 9). No buffer sits on the preset
  path; the Overall diagram, traced at 300 dpi, agrees. The PRA "Buff Amp"
  (two NJM4558 followers, IC4 on the CS-50 PRA sheet, SM2 p.22 / PDF 16;
  the Overall drawing numbers the pair IC9, the number that sheet reserves
  for a CS-60-only part) serves only the PANEL HPF and LPF levers: T71 inputs
  7 (26') and 9 (23') come from PRA fHO (59) and fLO (57) (SM2 p.11 / PDF 9),
  and a diode ORs each onto the same row as the preset columns (SM2 p.10 /
  PDF 8 shows that input diode and the 10k to E on every row; Guide p.63 /
  PDF 72: "input to terminals 1 to 22 ... output to terminals a to v
  through diodes"). The R7 block diagram (Guide p.59 / PDF 68) names those
  inputs "HPF lever" and "LPF lever" whichever source drives them.
- **Cutoff load.** R7 (SM1 p.14 / PDF 8) takes each cutoff bus through four
  39k, one per board, to fH1–fH4 / fL1–fL4, which the M board wires straight
  to pin 4 (SM2 p.16 / PDF 12: f1 meets R6, 0.01 µF, the 330k trim feed and
  Vfc). Each pin-4 node holds R6 2.2k (HP) or R7 2.4k (LP) at rank B beside
  the node's other inputs (the 330k trim feed behind its centred B-100k,
  355k; BR 680k / 220k; SUB/TBR/VCF 150k / 47k each; EG 220k / 47k on
  Overall): 2.07k and 1.96k. Each cutoff row therefore carries
  (39k + 2.07k)/4 = 10.27k (HP) or 10.24k (LP) beside its own 10k, 5.07k in
  all: half the load the printed table was computed for.
- **Earlier work.** `Docs/research/accuracy/preset-dc-loading/` (2026-10-19) solved
  this same five-node network, as a hypothesis for the old 12/180 Hz
  anchors and .7/.65 slopes reinterpreted on the actual bus, and compared
  it with four public recordings: loading at BR −15 V improved the Flute
  spectra but worsened its level trajectory and both Brass clips, so no
  mapping changed then. Those anchors and slopes are gone; under the
  printed law the actual bus is the only reading the circuit offers, since
  the bare divider voltage exists nowhere in it. The recordings were not
  re-run under the new law.
- **Q load.** QH and QL feed four 220k into pin-7 nodes that return through
  220k to RES: 110k to the RES bus, +10 V at the presets' RESONANCE MIN.
  A zero Q column, which has no matrix path, sits at 10/12 = 0.83 V, not at
  ground. **Re-read 2026-10-22 (Ceres 0.6.5):** pin 7 takes current
  ([sibling evidence](sibling-evidence-2026-10-21.md#addendum-the-research-cross-check),
  [decision log](../decisions.md)), so each branch is 220k on to a node that
  the other 220k and the pin's inferred 1.72k hold near ground: 55.4k per Q
  row, returning to 0.78 % of RES, 0.078 V at MIN. A zero column now sits at
  0.012 V, factory Q is up to 10 % higher than in the table below, and away
  from MIN a pin moves at most 0.12 % more than half of each RES volt, not the
  13/24 of point (2) under Not modelled.
- **Solve.** `preset()` now solves each filter row with its 1S1555 (the same
  estimated curve) against the 10k and its R7 load, recovering the column
  resistor from the printed bare divider, `Rs = 10k · (10/V − 1)`. The printed
  values are those dividers to their rounding: string1 g 5.00 V is 10k, flute
  g 1.18 V is 75k (SM2 p.10 / PDF 8), harpsichord g 6.41 V is 5.6k (T72,
  SM2 p.12 / PDF 10).
- **Uncertainty.** Pin 4's input impedance is not printed; it is taken as
  high, as Guide p.33 / PDF 42 draws Vfc reaching the chip through a
  resistor. A virtual-ground pin 4 would make each branch a bare 39k and
  lower a cutoff bus by at most 0.061 V. The VR7/VR9 trims, set by ear per
  board, are taken as centred; at either rail they move a bus by at most
  0.05 V. Rank A or C instead of B moves it by under 0.01 V. The factory
  cutoffs are therefore good to about ±0.1 octave on this account. Pin 7 is
  still taken to draw no current.
- **Not modelled.** (1) The buses are shared and passive, so a board's EG
  (or BR), by lifting its own pin-4 node, unloads the bus and raises every
  board's cutoff: at most 0.03 octave per board at +5 V of LP EG on a weak
  column, 0.1 with all four, a quarter of that on HP. (2) Away from MIN, the
  RES lever also drags each Q row through the 110k, so a pin moves up to
  13/24 of each RES volt (zero columns) rather than 1/2; stiff columns stay
  at 1/2. (3) The PANEL follower is stiff, but the T71 diode still drops
  about 0.5–0.6 V, so a front end that shows the HPF/LPF levers should feed
  the printed law with the lever volts less that drop (effectively nothing
  below about 0.4 V), not the raw lever volts.

Factory filter values at middle C, unloaded reading (previous commit)
against loaded rows; cutoffs in total Hz with the 22.6 Hz feed floor, Q
nominal at the pin:

| Preset | FH V | HP Hz | FL V | LP Hz | Q HP | Q LP |
| --- | --- | --- | --- | --- | --- | --- |
| string1 | 4.73 → 3.17 | 1676 → 586 | 8.20 → 7.26 | 18408 → 9584 | 0.81 → 0.80 | 0.53 → 0.53 |
| string2 | 2.21 → 1.26 | 312 → 173 | 5.62 → 4.02 | 3093 → 1036 | 0.53 → 0.53 | 0.53 → 0.53 |
| brass1 | 0.00 → 0.00 | 85.1 → 85.1 | 5.39 → 3.79 | 2649 → 890 | 0.53 → 0.53 | 0.85 → 0.83 |
| brass2 | 2.06 → 1.17 | 283 → 163 | 3.64 → 2.27 | 803 → 325 | 1.04 → 1.00 | 0.67 → 0.67 |
| flute | 1.12 → 0.60 | 159 → 118 | 4.95 → 3.38 | 1956 → 671 | 1.07 → 1.03 | 1.23 → 1.16 |
| electric_piano | 1.22 → 0.66 | 168 → 121 | 1.44 → 0.79 | 193 → 131 | 0.79 → 0.78 | 1.10 → 1.05 |
| clavichord | 4.95 → 3.38 | 1956 → 672 | 5.39 → 3.79 | 2649 → 890 | 0.83 → 0.81 | 0.96 → 0.93 |
| harpsichord | 6.05 → 4.48 | 4166 → 1413 | 7.42 → 6.13 | 10738 → 4398 | 0.53 → 0.53 | 0.63 → 0.63 |
| guitar1 | 3.64 → 2.28 | 803 → 325 | 5.62 → 4.02 | 3093 → 1036 | 0.87 → 0.85 | 1.20 → 1.14 |
| guitar2 | 2.37 → 1.37 | 346 → 184 | 2.56 → 1.49 | 391 → 199 | 1.58 → 1.44 | 1.58 → 1.44 |
| funky1 | 4.73 → 3.17 | 1676 → 586 | 5.62 → 4.02 | 3093 → 1036 | 0.81 → 0.80 | 1.58 → 1.44 |
| funky2 | 5.39 → 3.80 | 2649 → 892 | 6.93 → 5.50 | 7662 → 2844 | 1.58 → 1.44 | 1.58 → 1.44 |
| funky3 | 3.78 → 2.39 | 884 → 349 | 4.50 → 2.97 | 1436 → 513 | 1.20 → 1.14 | 1.12 → 1.07 |

Factory cutoffs fall 0.5–1.6 octaves (brass1's empty HP column stays at its
floor). This is a correction from printed parts, not a choice between
candidates, so it ships without a listening test.

- **Fixture:** `VcfKeyVoltageTests` now solves each row by bisection on its
  own node equation, with R7's draw written out as the explicit pin-4 and
  pin-7 node solutions, and requires every factory cutoff and Q anchor to
  match. The earlier 10k-only expectation encoded the unloaded premise.

### The PANEL HPF and LPF lever law (schematic)

The scorecard's "panel-to-Vfc offset" was the missing step from a panel
lever to the cutoff. The engine keeps `hp_cutoff` and `lp_cutoff` as total
Hz at middle C; `hpfLeverHz` and `lpfLeverHz` now give front ends that show
the hardware levers the printed path from lever travel to that value.

- **Lever.** HPF and LPF are B-10K slide pots on PN1, across the line that
  the PANEL switch takes from the T71 +10 V regulator (Overall; Guide p.64 /
  PDF 73: "the control lever of Panel 1 ... the controllable voltage", input
  to T71 terminals 1–22). The Guide's sketch spans 0 to 10 V ("HPF lever
  normal at 0", "LPF normal at 10", Guide p.35 / PDF 44).
- **Buffer.** Each wiper drives an NJM4558 follower: PRA IC4, fHI (60) →
  fHO (59) and fLI (58) → fLO (57) (SM2 p.22 / PDF 16; note 1 lists IC4 as
  NJM4558D). The follower's input draws nothing worth counting (25 nA
  typical bias behind at most 2.5k of wiper is 60 µV), so the wiper is
  exactly 10 V × travel.
- **Diode and load.** fHO and fLO drive T71 inputs 7 and 9 (SM2 p.11 /
  PDF 9), each through one 1S1555 onto row g or i (SM2 p.10 / PDF 8), the
  same rows the presets drive, so the load is the one solved above: the
  row's 10k beside R7's four 39k branches, 5.07k in all. In PANEL the
  tablets are off, so the preset column diodes are reverse-biased. The bus
  solves V + Vd(V/5.07k) = 10 × travel with the same 1S1555 estimate.
- **Law.** The printed law then reads the bus as for a preset.

| Travel | Bus V | Diode drop V | Cutoff at middle C, Hz |
| --- | --- | --- | --- |
| 0 | 0.000 | 0.000 | 85.1 |
| 0.05 | 0.096 | 0.404 | 89.4 |
| 0.1 | 0.520 | 0.480 | 112.2 |
| 0.3 | 2.450 | 0.550 | 364.0 |
| 0.5 | 4.423 | 0.577 | 1363 |
| 0.7 | 6.406 | 0.594 | 5323 |
| 0.893 | 8.324 | 0.606 | 20056 |
| 1 | 9.389 | 0.611 | 41924 |

HP and LP differ by under 0.0002 V. Below about 0.03 of travel the diode
passes almost nothing and the cutoff stays within half a hertz of the
85.1 Hz it has at 0, so the bottom few percent of each lever do nothing.

The LPF lever's top tenth (from about 0.893, 20 kHz at middle C) is above
the `lp_cutoff` parameter's 20 kHz ceiling, which `setParameter` clamps to.
At middle C and above that changes nothing audible, since the stage clamps
its own cutoff at 0.22 of the internal rate anyway; but key tracking halves
the cutoff each octave down, so on the lowest C (Kv 0.25 V) a full LPF
lever gives 21 kHz on the hardware and 10 kHz here. The HPF lever's top
tenth meets the same `hp_cutoff` ceiling, which matters even less: by then
the high-pass has removed nearly the whole note. Raising both ceilings to
about 42 kHz would close this; it is a parameter-range change that the
front ends' own tables would have to follow, so it is left to integration.

- **Estimate carried:** the 1S1555 curve (as for the presets) and a stiff
  T71 +10 V under the PANEL levers' parallel load (about 0.5 kΩ); the
  regulator's output resistance is not printed.
- **Fixture:** `VcfKeyVoltageTests` solves the PANEL row as a current
  balance (the diode's Shockley current at lever − V against the 10k and the
  explicit R7 node draws) and requires both helpers to match at ten travels,
  to rise monotonically, to clamp travel and to return NaN for a nonfinite
  travel.

## What would close the gap

1. A measured CS-50 (or CS-60/CS-80 IG00156) cutoff against lever voltage at
   one key: two points fix the slope; with the printed zero point that
   closes the panel-to-Hz law. Since Round 3 this would confirm or correct
   the printed-law reading chosen by ear.
2. Pin-4 input characteristic of the IG00156 (voltage/current at the pin
   against cutoff), or any chip-level datasheet. The test circuit behind
   the pin sheet's "Vf 5 V gives 1 kHz" would settle the printed-law reading;
   the input impedance alone would also pin the preset-bus loading, now
   bounded to ±0.06 V.
3. Q against VQ at three or more voltages on one chip, to confirm or
   correct the exponential converter law chosen by ear.
4. Signal amplitude at IG00156 pin 15/10 for a known WSC input, or the onset
   of audible OTA compression at a known input level, for the input scale.

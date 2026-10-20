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
NA03574, 3.0k/2.4k/2.0k on NA03655), 0.01 µF to ground, the R7-board inputs
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
recomputed here with the EG paths included.) Which common value is right is
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

## What would close the gap

1. A measured CS-50 (or CS-60/CS-80 IG00156) cutoff against lever voltage at
   one key: two points fix the slope; with the printed zero point that
   closes the panel-to-Hz law.
2. Pin-4 input characteristic of the IG00156 (voltage/current at the pin
   against cutoff), or any chip-level datasheet.
3. Q against VQ at three or more voltages on one chip, to choose the Q
   converter law.
4. Signal amplitude at IG00156 pin 15/10 for a known WSC input, or the onset
   of audible OTA compression at a known input level, for the input scale.

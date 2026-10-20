# Envelopes: authenticity review, 2026-10-20

Scope: the VCF EG (IG00152 + IG00151, IL/AL/A/D/R) and the VCA EG (IG00159 +
IG00151, A/D/S/R) on each M board, the SUB-board sustain control that feeds
both release pins, and the factory CV-to-time conversion in `preset()`. The
ring-modulator envelope on the PRA board is out of scope.

Score before: 6/10. Score after: 8/10 (reasons at the end).

## Sources

| Short name | Document | Pages used |
| --- | --- | --- |
| Guide | Yamaha CS Series Servicing Guide | printed 42–45 (PDF 51–54), 61 (PDF 70) |
| SM1 | CS-50 Service Manual 1/2 | printed 14 (PDF 8, R7), printed 20 (PDF 10, IG00152 pins) |
| SM2 | CS-50 Service Manual 2/2 | printed 8 (PDF 6, SUB), 16–17 (PDF 12–13, M), 21 (PDF 15, IG00159 pins), 27 (PDF 21, PN3) |
| Overall | CS-50 overall circuit diagram | M board, EG area |
| IC Guide | Yamaha IC Guide Book | p.2-2 (PDF 31, IG00151), p.2-3 (PDF 33, IG00152), p.2-7 (PDF 41, IG00159) |
| CS-60 SM | Yamaha CS-60 Service Manual | printed 37–39 (PDF 39–41, alignment), PDF 17 (M circuit) |
| 2SC458 | Hitachi 2SC458/2SC2308 data sheet | VBE, hFE ranks |

The CS-60 evidence is sibling evidence. It is used only where the board is
shown to be the CS-50's: SM2 printed 16 draws the M board for "CS-80 / CS-60,50"
(one drawing, rank table columns for CS-80 and for CS-60/50), and the CS-60
M circuit (CS-60 SM PDF 17) has the same IG00151/IG00152/IG00159, 0.22 µF
integrating capacitors, 82k + VR19/VR20 (B50K) to EI, 100k + VR15/VR16
(B100K) offsets, 560k LI bias, VR17/VR18 (B50K) through 470k into +IN, and
100k LC and OUT feeds. Nothing in this review is a measurement of a CS-50.

## The circuit, as drawn

Each EG is an IG00151 used as a variable resistance (its EXP converter on EI,
linear converter on LI, Gm stage with +IN and −IN; IC Guide p.2-2) charging a
0.22 µF capacitor, buffered by the sequencer chip (SM2 printed 16):

- The sequencer's LC pin (level control) reaches +IN through 100k; the buffered
  capacitor voltage (OUT) reaches −IN through 100k. Equilibrium is OUT = LC.
- Its TC pin (time control) reaches EI through 82k + VR19 (VCF) or VR20 (VCA);
  VR15/VR16 add an offset through 100k. Two trims, span and offset, on one
  exponential converter.
- LI is biased from +15 V through 560k. VR17 (VCA) or VR18 (VCF), a B50K
  between ±15 V, injects a trim current into +IN through 470k.
- G1 takes G2's key-on edge through a 1000 pF ceramic ("K", SM2 printed 17
  notes 5). Each key-on edge starts an attack from whatever charge the
  capacitor holds. This is how the engine already behaves; existing contracts
  (`Ceres.native_dsp_contracts` retrigger, `Ceres.fast_note_envelope_contracts`)
  cover it, and the code now cites the source.

Inside the sequencers (IC Guide p.2-3 and p.2-7), AT, 1DT and 2DT are switched
to TC, and IL, SL and the attack level to LC. On the IG00159 the attack level
is +15 V through 30k with 60k to ground (10 V), and the sequencer compares the
buffer against **that same node** (Guide printed 43: “VA = VB (10V)”).

## Findings

### 1. The time law is pinned at two points (resolved)

CS-60 SM printed 37 (EG-VCF, items 1–2) and printed 38 (EG-VCA, items 2–3)
trim both EGs to an attack of **4 ms at 8 V and 125 ms at 3 V** on the A pin,
measured from key-on to the peak at TP1/TP2. The pin-sheet law already in
`preset()`, `t = 10^(−0.3 V)` s, gives 3.98 ms and 125.9 ms, under 1 % from both
points; the pin sheets' 1 ms–1 s attack range (SM1 printed 20, SM2 printed 21)
is the same law. `Ceres.envelope_circuit_contracts` checks it.

D and R are not trimmed separately: they drive the same TC → EI path through
the same VR19/VR20 and VR15/VR16, so their time per volt is the same 0.3
decade/V. Their absolute range comes from the pin sheets (10 ms–10 s).

**Why the attack decade sits below D/R.** Because the comparator taps the node
the attack aims at, the ideal attack never quite arrives; it ends when chip
offsets let the comparator trip, several RC in. An attack range one decade
shorter than D/R at the same pin voltage therefore needs a ten-times-shorter RC
per pin volt, that is, scaling inside the chip between AT and TC. The IC Guide
block diagrams draw AT straight to TC, but they also draw IL straight to LC,
while the IG00152 pin sheet prints IL 0..10 V → 0..−5 V. The block diagrams
omit internal scaling, so they do not contradict this. A reading in which A and
D share one RC at the same voltage would need the attack to stop at 39 % of its
target (0.5 RC against a 5 RC decay), which contradicts Guide printed 43.
No listening test was run: that candidate fails on its own source.

The decay/release *definition* (5 RC, from the related CS-20M/CS-40M IG00159
description) is unchanged and remains sibling-derived.

### 2. Release CVs pass a follower (implemented)

SM2 printed 8 (SUB board, “Sustain Control Circuit”) and Guide printed 61: each
release lever (via T71/T72) reaches a node through 10k; a 1S1555 from that
node, cathode to SUS, clamps it; the node drives a 2SC458 emitter follower
through 1k, emitter to −15 V through 10k, output to RF or RA. Decay levers go to
DF/DA directly. So

    RF/RA = min(lever, SUS + Vd) − VBE − Ib·11k.

D and R share the EG's single TC/EI trim (above), so this drop cannot be trimmed
out of R alone. The follower runs at (Vout + 15)/10k = 1.5–2.5 mA with VCE 5–15 V.
Hitachi's sheet gives VBE typ. 0.67 V at IC 2 mA, VCE 12 V; hFE ranks C 160–320
and D 250–500 (the boards allow C or D), taken at their geometric centre, 283,
giving about 7 µA of base current through 11k. The drop is 0.73–0.77 V. On the
0.3 decade/V law, **every factory release is about 1.7× its old value**
(Brass 1 amp release T60 0.021 → 0.035 s; String 2 0.145 → 0.244 s).

Engineering patches are unaffected: their times are defined at the EG pin. On
the SUS path the diode and VBE nearly cancel (a 1N4148-class estimate for the
1S1555 gives about 0.55 V at 0.5 mA against 0.67 V), leaving about 0.1 V, roughly
7 % in time, not modelled; the diode's soft knee near the crossover is not
modelled either. SVR5, the SUSTAIN lever, is a C-10K taper (SM2 printed 27) whose
curve is not published, so `global_release` stays a time.

### 3. VCF initial level is −4 V at IL = 10 V (implemented)

The IG00152 pin sheet (SM1 printed 20) describes IL 0..10 V as “0 to minus 5
volt” and LC up to +5 V. The alignment of the shared board (CS-60 SM printed
37, item 1) checks TP1 at **+5 V ±0.5 V for AL = 10 V and −4 V ±0.5 V for
IL = 10 V**, after VR18 has nulled the held level to 0 ±0.01 V (item 3). The
alignment window excludes −5 V, so factory IL now maps at 0.4 V/V and AL stays
at 0.5 V/V. Factory presets with an IL setting (String 1, Brass 1/2, Flute,
Funky 1/2, Guitar 1) start their filter sweep 20 % less far down.

### 4. VCA EG holds −0.2 V below every target (implemented)

CS-60 SM printed 38, EG-VCA item 1: with SL = 0 V and a key held, VR17 is set
so TP2 reads **−200 ±20 mV**; item 2 checks the peak at 10 ±0.2 V. VR17 injects a
constant current into +IN against the 100k feeds, so the offset is the same at
every target: SL while held and IL (0 V) after release. The engine now aims the
VCA EG at `S − 0.02` while held and at `−0.02` after release, starts idle boards
at −0.02, and the VCA, linear in the positive envelope (Guide printed 40–41),
passes nothing below zero. A decay to zero sustain and every release reach
silence after ln(51) ≈ 3.9 time constants instead of fading forever; a
full-sustain note holds at 0.98 of its peak. The VCF EG is nulled (VR18) and has
no floor.

The constant offset cannot simply apply to the attack as well: the attack would
then aim 0.2 V below the comparator node and never end. The peak stays at the
comparator level, within the printed ±0.2 V.

### 5. HP/LP coupling ratios (verified, unchanged)

Overall diagram and SM2 printed 16: the EG output reaches the LP Vfc node
through 47k and the HP Vfc node through 220k; the FL/FH buses reach them through
39k from R7 (SM1 printed 14), into the rank resistor R6/R7 and 0.01 µF. All
sources sum into the same node, so their relative weights are the conductance
ratios 39/47 and 39/220 whatever the node impedance. The `.7`/`.65` oct/V manual
slopes they multiply belong to the filter calibration.

## What still blocks 9/10

1. **Attack headroom.** Where an attack ends depends on the IG00152/IG00159
   comparator offset, buffer offset and IG00151 input offset after VR17/VR18.
   None is published; a TP1/TP2 scope capture of an attack near the top would
   settle it. Factory presets keep the 5 RC inference; engineering patches the
   1 % default.
2. **Decay/release definition.** The 5 RC meaning of “decay time” is from the
   CS-20M/CS-40M description. A CS-50 or CS-60 decay timing on TP2 (for
   example D = 3 V, S = 0) would close it; the alignment trims only attack.
3. **LC loading and the IG00151 input.** LC drives 100k into +IN, whose internal
   resistance to ground (IC Guide p.2-2) is not given. If it is small, the
   30k/60k node loads to about 8.3 V while a full SL lever stays near 10 V, so a
   full-sustain note would decay *up*. The CS-60 peak check (10 ±0.2 V) argues
   against heavy loading but does not bound the internal resistor.
4. **Part tolerances.** VBE, hFE and the 1S1555 curve are typical or
   class-estimated; the IG00152/IG00159 come in ranks A–D (SM2 printed 17)
   whose meaning is not published.
5. **SUSTAIN lever law.** SVR5's C-taper curve and the SUS-path residual
   (about 0.1 V) need a lever-to-SUS measurement.

## Score

6 → 8:

- +1: the lever-to-time law is pinned at two published trim points for both
  EGs, and the A versus D/R decade is explained from the circuit instead of left
  open.
- +0.5: the release path now includes the sustain-control follower, which
  lengthens every factory release by about 1.7×.
- +0.5: the VCA EG has its aligned −0.2 V floor, so releases and zero-sustain
  decays end in silence in finite time as on the board; the VCF IL scale takes
  the aligned −4 V.

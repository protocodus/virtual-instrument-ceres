# Ceres: BRILLIANCE and RESONANCE authenticity (2026-10-20)

Scope: the global panel 2 BRILLIANCE and RESONANCE levers, the BR and RES
buses they drive, and how those buses reach the IG00156 cutoff and Q pins.
The engine's `brilliance` and `resonance_offset` stay what they were, loaded
bus volts. This pass adds the lever-to-volts law that a front end needs, and
records what the sources say about everything around it.

Sources, printed page (PDF page in brackets):

- **SM1**, **SM2**: Yamaha CS-50 Service Manual 1/2 and 2/2 (S/N 1001~).
  SM1 parts list pages are numbered separately (parts p.N).
- **Guide**: Yamaha CS Series Synthesizer Servicing Guide.
- **OC**: CS-50 Overall Circuit Diagram (single sheet).
- **CS-60**: Yamaha CS-60 Service Manual (S/N 1001~), sibling evidence only
  where its part numbers or circuits match the CS-50's.
- **Alps**: Alps potentiometer catalogue, "Resistance Taper" page, p.408
  (<https://media.onlinecomponents.com/productfiles/mf-ale/alps%20-%20resistance%20taper%20definitions.pdf>).

## 1. The wiring

| Item | Value | Source |
| --- | --- | --- |
| BRILLIANCE TVR7 | terminal 1 → M4 −15 V, terminal 3 → M4 +15 V, centre tap 5 → ground, wiper 2 → R7 BR (29) | SM2 p.26 [20] PN2; OC (BRILLIANCE, PN2) |
| RESONANCE TVR6 | top end → T71 +10 V bus (OR12, shared with TVR1–5 and TS14), bottom → ground, wiper → R7 RES (51) | SM2 p.26 [20] |
| TVR6 terminal numbers | terminal 1 = +10 V, terminal 3 = ground | OC (RESONANCE, PN2: TVR6 drawn with +10 V on terminal 1 and terminal 3 to ground); SM2 p.26 TS14 cross-reference "PN2-TVR6-1" |
| R7 BR paths | 680k to each fH1–4, 220k to each fL1–4 | SM1 p.14 [8] |
| R7 RES paths | 220k to each QH1–4 and QL1–4; the preset buses QH/QL reach the same nodes through another 220k each | SM1 p.14 [8] |
| Q pin | M-board QH/QL go straight to IG00156 pin 7, with 10 µF to ground; pin 6 Qadj (VR8 B-50K to +15 V) sets the Q5 end | SM2 p.16 [12] |
| Q endpoints | pin 7 at 0 V (Max.) → Q = 5; at 10 V (Min.) → Q = 0.5 | SM1 p.19 [9] |
| Cutoff pin | R7 fH/fL go straight to pin 4, shunted to ground by the rank-selected R6 (HP) or R7 (LP) and 0.01 µF, with 330k from the VR7/VR9 offset trim | SM2 p.16 [12] |
| Functions | BRILLIANCE toward MAX raises both cutoffs; RESONANCE toward MAX gives more resonance | Guide printed p.5 |

**Lever orientation.** BRILLIANCE MAX raises the cutoffs, so the MAX end is
the +15 V end, terminal 3. If RESONANCE is mounted the same way, its MAX end
is also terminal 3, which is ground: the Q pins go toward 0 V, the Q5 end.
The two levers agree, which also confirms the reading of TVR6's numbering.
PN1's B-10K levers run the same way (terminal 1 ground, terminal 3 +10 V,
SM2 p.25 [19]).

**Yamaha's own check of the R7 matrix.** The guide (printed pp.58–59) prints
the voltages at the R7 outputs with every input at 10 V and every output
loaded with 1 kΩ. The drawn matrix reproduces them: fH = 10·G/(G+1 mS) with
G = 1/39k + 1/680k + 3/150k gives 0.450 V (printed 0.45 V); QH with two 220k
gives 0.090 V (printed 90 mV); LC gives 0.184 V (0.18 V); VB 0.196 V (200 mV).
The LP figure does not match: the drawn fL node (39k, 220k, 3×47k) gives
0.86 V where the guide prints 0.64 V. 0.64 V is what 39k with only two 47k
would give, so the guide's LP drawing may come from a different revision; the
SM1 drawing is the primary source and Ceres keeps it.
`Ceres.panel_lever_contracts` checks the fH and QH figures.

## 2. The lever pots are B-10K

The CS-50 PN2 sheet (SM2 p.26 [20]) prints **C-100K** for both TVR6 and TVR7.
Every other source says B-10K:

- The CS-50 parts list (SM1 parts p.9 [21]) has only two TVR parts:
  `40 10 00 HP 70 00 10 TVR B-10KΩ` and `40 10 00 HP 70 00 20 –do.– B-10KΩ,
  for Brilliance`. The second is a separate part, as the centre-tapped TVR7
  needs. No C-100K part appears anywhere, so the generic TVR B-10K covers
  TVR1–6, RESONANCE included.
- The CS-60 parts list (CS-60 Service Manual, parts p.9; sibling evidence)
  lists the same two part numbers, `40 10 00 HP 70 00 10 TVR B-10KΩ
  トーンボリウム` and `40 10 00 HP 70 00 20 –do.– (Center click type), for
  Brilliance`, both marked "Common models: CS-50". The BRILLIANCE pot is a
  centre-click (detented) B-10K; the detent is where the grounded tap sits.
- The CS-50 overall circuit diagram labels TVR6 "B10K".

The parts lists are right where the drawings disagree elsewhere too: PN3
SUSTAIN SVR5 is C-10K on the PN3 sheet (SM2 p.27) and in both parts lists
(CS-60 parts p.9 item 19, "C-10KΩ for Sustain"), while the overall diagram
calls it B10K. So when a lever really is a C taper the parts lists say so,
and for RESONANCE and BRILLIANCE they say B-10K. Ceres takes **B-10K, linear**
for both, and the PN2 sheet's C-100K as a drafting error.

For the record, what C would have meant: Japanese taper letters are A = log
(15 % at half travel), B = linear, C = reverse log. Alps's standard C(15C)
graph (V(2–3)/V(1–3) against travel from terminal 3) reads 0, .008, .026,
.076, .131, .176, .226, .466, .721, .974, 1 in tenths of travel. On a 100k
track that would have put RESONANCE at 1.4–2.4 V at half travel and behind
up to 25 kΩ. None of that applies to a B-10K.

The CS-60's own panel differs and is not used for the CS-50 wiring: its
BRILLIANCE ends come from the KEYBOARD CONTROL BRILLIANCE levers (KBC-HIGH),
not straight from ±15 V (CS-60 Service Manual p.32, PN2). Its RESONANCE VR11
is wired as the CS-50's, T61 +10 V on one end and ground on the other.

## 3. Loading

**BRILLIANCE.** The wiper drives 4×680k and 4×220k into the cutoff nodes,
41.56 kΩ in all. Those nodes are held within a few hundred millivolts of
ground by R6/R7 (about 2 kΩ), so the load returns to ground to within 0.1 %.
On the B-10K each half is 5 kΩ, so the wiper sits behind at most 1.25 kΩ and
the bus sags at most 2.9 %, at quarter and three-quarter travel. The ends
and the centre are exact.

**RESONANCE.** In steady state each pin 7 sits at the mean of its preset bus
P and the RES bus r, so RES feeds (r − P)/440k into each of the eight pins:
55 kΩ returning to the mean of the HP and LP preset buses, P̄. With the chip's
pin-7 input current neglected (it is not published; the 1 kΩ test above does
not identify it), the loaded bus is

`r = E − (E − P̄)·Rs/(55k + Rs)`, `E = 10·a`, `Rs = Rtrack·a(1 − a)`,

with a the fraction of the track between wiper and ground. The B-10K sags at
most 0.26 V (at a third of travel with P̄ = 0).

| Travel (0 = MIN) | `brilliance` V | RES V, P̄ = 0 | RES V, P̄ = 5 |
| --- | --- | --- | --- |
| 0.0 | −15.00 | 10.00 | 10.00 |
| 0.1 | −11.77 | 8.86 | 8.94 |
| 0.2 | −8.75 | 7.77 | 7.92 |
| 0.3 | −5.83 | 6.74 | 6.93 |
| 0.4 | −2.94 | 5.75 | 5.96 |
| 0.5 | 0.00 | 4.78 | 5.00 |
| 0.6 | +2.94 | 3.83 | 4.04 |
| 0.7 | +5.83 | 2.89 | 3.07 |
| 0.8 | +8.75 | 1.94 | 2.08 |
| 0.9 | +11.77 | 0.98 | 1.06 |
| 1.0 | +15.00 | 0.00 | 0.00 |

These are `ceres::brillianceLeverVolts` and `ceres::resonanceLeverVolts`.
In engine terms the preset buses are P = 10·(1 − r_anchor), the inverse of
`preset()`'s Q conversion.

**RC timing.** The engine's 1.1 s (`qControlTau`) assumes a stiff RES source.
A source resistance Rs on the shared wiper puts 8·Rs in series with each pin's
RES path for the common mode: τ = 10 µF / (1/220k + 1/(220k + 8Rs)). The
B-10K at half travel (2.5 kΩ) gives 1.15 s.
Ceres keeps 1.1 s: the B-10K difference is under 5 % and only exists at
mid-travel. The steady-state pin change is still exactly half the loaded bus
change, which is what `resonance_offset` is defined on.

## 4. How the preset Q bus reaches the pin: an open choice

Pin 7 = (P + r)/2 with stiff sources. A preset's Q column therefore moves the
pin only half as far as the column voltage, and where it lands depends on
the RESONANCE lever. `preset()` maps the column straight to the pin
(`r = 1 − P/10`, the engine's resonance coordinate, whose ends are the chip's
0 V/Q5 and 10 V/Q.5). No lever position reproduces that for every preset: it
equals the hardware only when the lever happens to sit at the preset's own
Q voltage. The hardware law is `r = 1 − (P + R)/20` for lever voltage R.

What is not known is which lever position the factory presets were voiced
for, so this is a choice between defensible settings, not a measurement. It
was rendered as an A–Z listening test (A shipping; lever at MIN, half travel
and MAX through the hardware divider) into
`/mnt/project-files/ceres/listening/2026-10-20-brilliance-resonance-preset-q-divider/`.
Until a letter is chosen, `preset()` keeps the shipping mapping. Whatever is
chosen, `resonance_offset` keeps its meaning: a front end that sets it to
`resonanceLeverVolts(t, P̄) − R0` with the chosen neutral R0 reproduces the
hardware divider exactly once the presets use the same R0.

## 5. The cutoff slopes (owned by the VCF work; reported here)

The M board shunts each pin 4 with a rank-selected resistor, chosen by the
IG00156's rank letter (SM2 p.16 [12] table, CS-50/60 column): HP R6 = 2.7k /
2.2k / 1.8k and LP R7 = 3.0k / 2.4k / 2.0k for ranks A / B / C. The LP node
already carries more R7 conductance (39k, 220k, 3×47k: 94.0 µS against the
HP node's 47.1 µS), and the R6/R7 pairs are exactly what make the 39k lever
reach both pins equally: equal gain needs 1/R7 = 1/R6 − 46.9 µS, which gives
3.09k, 2.45k and 1.97k against the printed 3.0k, 2.4k and 2.0k. The lever-volt
gain to pin 4 comes out HP/LP = 1.023, 1.018, 0.986 for ranks A, B, C.

So with both filters on the same chip rank, the schematic says the FH and FL
levers move their cutoffs by **the same octaves per volt, within about 2 %**.
Ceres's provisional 0.7 (HP) and 0.65 (LP) oct/V differ by 7.7 %, which the
circuit does not support. The absolute slope is still unknown: the chip's
pin-4 sensitivity is what the rank letter sorts, and SM1 p.19 gives only one
point (VK 0.25 V, Vf 5 V → 1 kHz). The BR ratios themselves (39/680 HP,
39/220 LP of the lever slope) hold whatever that slope is.

## 6. Alignment

The CS-60 alignment procedure (CS-60 Service Manual section VI, p.38, M
board III–IV; sibling evidence, same IG00156 M-board family) trims HPF Q
(VR8) and LPF Q (VR9) by ear, "so that the tone color of the M circuit board
may be the same as that of other M circuit boards", with RESH/RESL set to an
end. It gives no Q, voltage or RESONANCE-lever figure, so it adds nothing to
the Q law; it does say the four boards' Q were matched to each other, not to
an absolute value.

## 7. Score

Before: 6/10. After: 7/10.

- +1: the lever-to-bus law is now derived and encoded: the pot is B-10K on
  two parts lists (CS-50 and the CS-60 under the same, CS-50-common part
  numbers), the wiring and orientation come from the PN2 sheet and the
  overall diagram, and the loads from the R7 matrix, which itself reproduces
  Yamaha's own 1 kΩ test figures. Before, the taper was called unpublished
  and no lever law existed.

What blocks 9:

1. The preset Q divider (section 4): Ceres needs the owner's choice in the
   listening test, or a recording of a factory preset at a known RESONANCE
   lever position.
2. The absolute cutoff slope and the IG00156 pin-7 Q law between its two
   printed ends (VCF work), which every BRILLIANCE and RESONANCE volt passes
   through. Section 5 shows the HP and LP lever slopes should be equal.
3. The IG00156 pin-7 input current, which sets how far the 55k load and the
   1.1 s pole are from the stiff-source figures.

## Addendum 2026-10-22: pin 7 takes current

Item 3 above is answered as far as the documents go. The CS-30, CS-15 and
CS-40M drive the same chip's pin 7 through 22k into a 390–470 Ω rank shunt,
and the CS-30 prints Q 0.5 at 10 V there as the CS-50 does through its 220k
pair, which only a current input allows
([sibling evidence](sibling-evidence-2026-10-21.md#addendum-the-research-cross-check),
[decision log](../decisions.md)). Equal current at both printed points puts
the input at 1.72 kΩ, an inference. Ceres 0.6.5 uses it:

- **RC timing.** Each node's 10 µF sees 220k, 220k and the input:
  τ = 10 µF/(2/220k + 1/1.72k) = 16.9 ms instead of 1.1 s. A source
  resistance on the RES wiper now matters even less than in section 3.
- **RESONANCE load.** Each pin-7 branch, seen from RES, is 220k on to a node
  that the other 220k and the input hold near ground: 221.7k returning to
  0.78 % of its preset bus. The eight make 27.7k returning to 0.78 % of P̄,
  so the B-10K sags up to 0.50 V near a third of travel, almost whatever
  P̄ is (the table in section 3 is the 55k reading). These are
  `ceres::resonanceLeverVolts`.
- **Preset Q load.** Each Q row sees 55.4k returning to 0.78 % of RES, so a
  zero column sits at 0.012 V and factory Q rises by up to 10 %; see the
  [VCF research](vcf-authenticity-2026-10-20.md).

The steady-state pin current is still (P + R)/2 through 110k plus the input,
so section 4's divider and `resonance_offset`'s meaning are unchanged.

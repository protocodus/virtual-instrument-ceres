# Ring modulator authenticity, 2026-10-20

This pass covers the PRA ring modulator: the carrier oscillator (IC5,
IG00150), its envelope (IC7 IG00159 with IC6 IG00151), the passive carrier
network, the µA796 multiplier (IC8), the wet buffer and the trigger. The
MODULATION crossfade loading and the expression network were done earlier
(hardware-realism rows 22–23) and are unchanged.

## Sources

| Short name | Document | Pages used |
| --- | --- | --- |
| SM2 | CS-50 Service Manual 2/2 | printed 9 / PDF 7 (IG00150 pin sheet); printed 16 / PDF 12 (M board VCA EG); printed 21 / PDF 15 (IG00159 pin sheet); printed 22 / PDF 16 (PRA schematic and parts note) |
| Guide | Yamaha CS Series Servicing Guide | printed 18–19 / PDF 27–28 (KAS timing chart, ring trigger); printed 49 / PDF 58 (PWM VCO law); printed 50–53 / PDF 59–62 (ring modulator, oscillator and envelope outlines) |
| Overall | CS-50 overall circuit diagram | PRA section, grid L–M 3–5 |
| CS-60 SM | Yamaha CS-60 Service Manual (sibling) | PDF 15 (PRA schematic); printed 38–39 / PDF 40–41 (section VI alignment: M-board EG/VCA, PRA). Page numbers below are printed. |
| µA796 | Fairchild 1973 Linear IC Data Catalog | pp.7-74 to 7-77 (µA796 sheet) |
| MC1496 | Motorola/ON MC1496 data sheet (second source) | bias formula I5 = (V − φ)/(R5 + 500 Ω), φ = 0.75 V |

**Sibling status of the CS-60 manual.** SM2's PRA sheet carries its own
CS-60 notes ("IC9 is used for CS-60 only", "Value of R: CS-50 270K, CS-60
470K"), so it is one board drawn for both instruments. The CS-60 manual's PRA
schematic (PDF 15) matches it part for part in the ring section. Its alignment
values are used below as the procedure for this board, labelled as CS-60.

## Findings

### 1. The carrier divider (correction)

SM2 p.22, the guide's p.52 outline and the overall diagram all draw the 33k
**in series** between the IG00150's 100 µF coupling capacitor and pin 7; no
end of it is grounded (checked at 600 dpi). Pin 7 meets pin 8 through 100 Ω,
and pin 8 sits on the 100 µF-bypassed bias divider. The carrier at pins 7–8
is therefore a 33k/100 divider of the oscillator's sine:

- IG00150 pin 6 (SIO) is 3.20 Vpp (SM2 p.9). Guide p.49's 3 Vpp is the PWM
  level-control output on the SUB board, a different node.
- 100 Ω in parallel with the carrier port's small-signal r_π (3.92 kΩ, below)
  is 97.5 Ω, so pins 7–8 see 1.6 × 97.5/33097.5 = **4.71 mV peak**.
- The µA796 sheet (p.7-77, Fig. 3) says the carrier must stay below 25 mV
  peak for linear operation. At 4.7 mV the upper pairs never switch:
  tanh(4.71 mV / 2V_T) = 0.0915, within 0.3 % of linear.
- The 100 µF into 33k puts the coupling pole at 0.048 Hz, so the divider is
  within 0.46 dB from 0.2 Hz to 220 Hz.

The earlier model grounded the 33k and fed pin 7 straight from the
capacitor, so its carrier was about 1.5 V at pins 7–8: hard switching (a
square-wave product with odd sidebands) and a network that was 38 dB down
at 0.2 Hz and 13 dB down at 6.7 Hz. Both were artefacts of that reading.
The hardware multiplies by a sine: two sidebands, flat across the SPEED
range. This matches the guide's p.51 spectrum, which shows f ± Δf only.

### 2. Operating point and the carrier-port offset

With V− (pin 10) grounded and pin 5 fed from the 390 Ω-fed local rail S
through 10k:

| Quantity | Value | Basis |
| --- | --- | --- |
| Rail S | 12.152 V | KCL: 390 Ω from +15 V; loads 2 × I5 (pins 6, 9 via 2.7k), I5 (pin 5), 1.2k into 820 + (1k ‖ 2 × 125.47k) |
| I5 | 1.086 mA | (S − 0.75)/(10k + 500) (MC1496 formula; µA796 equivalent circuit shows the 500 Ω) |
| β | 82.8 | µA796 p.7-75: (I7 + I8)/2 = 12 µA typ in Fig. 1, where I5 = (8 − 0.75)/7.3k = 0.993 mA |
| r_e | 23.7 Ω | V_T/I5 at 25 °C |
| V7 − V8 (DC) | −1.31 mV | pin 7's base current I5/β can only flow through the 100 Ω |
| Carrier port r_π | 3.92 kΩ | β·V_T/(I5/2), two pairs in parallel |

The offset steers tanh(−1.31 mV/2V_T) = −0.0255 of the signal current
straight through, so the dry signal leaks into the wet output about 5 dB
below each sideband. That is a typical-part figure (the sheet's maximum bias
current is 30 µA). VR5 trims the *carrier* leakage on the signal side to
below 3 mVp-p (guide p.52; CS-60 SM p.39), not this offset, so it is kept.
A full nodal solve that also draws every base current moves the offset by
under 2 % (`Ceres.ring_switching_circuit_contracts`).

### 3. Signal side and wet gain

- **Input resistor.** SM2 p.22 and the CS-60 PRA schematic (PDF 15) print 330K; the
  guide's p.52 outline and the overall diagram print 330. With 330K the
  whole wet path would be 5.5 × 10⁻³ per unit carrier steering. Even at the
  wet buffer's full gain of 11 it would sit about 45 dB under the dry signal
  it is crossfaded against. So 330 Ω is used: pin 1 then gets 0.584 of O1.
- Pin 1 node: 470 Ω to the AC-grounded bias node ‖ 125k (100k + VR5 centre)
  ‖ β(470 + 2r_e) = 42.8k → 463.2 Ω.
- Emitter-degenerated signal pair: differential current = (V1 − V4)/(470 +
  2r_e) = /517.3 Ω. The µA796 sheet's own gain figure (3.5 V/V typ for
  RL 3.9k, RE 1k) agrees with RL/(RE + 2r_e) to 6 %.
- Pin 6 load: 2.7k ‖ 40k (the sheet's single-ended output resistance) ‖ 10k
  (the wet buffer's input) = 2.02 kΩ. The single-ended modulator gain is
  0.584 × 2019/517.3 = **2.279** per unit steering.
- **Couplings.** Pin 1: 10 µF into 330 + 463 Ω, 20.1 Hz. Pin 6 to the buffer:
  10 µF into 2.53k + 10k, 1.27 Hz. Both are now in the wet path. The old
  τ = 0.1 s omitted the collector source resistance.
- **Wet buffer.** IC1-2 is non-inverting, gain 1 + VR4/10k, with VR4 a
  B-100K trimmer (SM2 p.22, overall, CS-60 PDF 15). **No document gives its
  setting**: the CS-60 alignment trims VR3, VR7 and VR5 on this board, not
  VR4. Two trims are defensible for a steady midband carrier:
  - wet RMS equal to dry RMS: VR4 = 57.8k, gain 6.78 (shipped in 0.5.0);
  - the two sidebands together peaking at the dry peak, each half the dry
    tone: VR4 = 38.0k, gain 4.80, 3.0 dB less wet.

  The owner chose the peak-matched trim **by ear** (listening test
  ring-wet-trim, 2026-10-20), so the engine uses 38.0k. That is a choice
  between two defensible trims, not a measurement of VR4. Both trims land
  inside the trimmer's range, while the 330K reading would need a buffer
  gain of about 2000 against the trimmer's maximum of 11. That is what
  independently supports the 330 Ω reading.
  `Ceres.native_dsp_contracts` reads VR4 back off the engine's wet level
  through an independently derived chain and checks it is 38.0k and
  reachable.
- **Signal-pair nonlinearity.** The degenerated pair limits at 0.56 V peak on
  pin 1. CS-60 SM p.39 (VI-4) aligns the CS-50 voice output to 0.6 V
  ("CS-50 is 0.6V"). At pin 1 that is at most 0.35 V: 1.5 % peak
  compression, third harmonic −48 dB, and −62 dB at half that level.
  The engine's bus is not a calibrated voltage, so this is a bound, not a
  modelled effect.

### 4. Carrier oscillator law

- SPEED is TVR4 (B-10k, +10 V to ground) into VC1 through 100k. DEPTH is
  TVR3 (B-10k, EO to ground) into VC2 through 180k (overall diagram).
- CS-60 SM p.39 (PRA II) trims the oscillator to a 150 ± 10 ms period at
  VC1 = 5 V and 5 ± 1 ms at VC1 = 10 V (DEPTH 0). The IG00150 pin sheet
  (SM2 p.9) also states 200 Hz at VC1 = 10 V, VC2 = 0. Exponential through
  those points: 6.667 Hz × 30^((VC1 − 5)/5), 0.9814 octave/V, 0.222 Hz at
  0 V. The guide's p.53 range "0.2–220 Hz" agrees to its rounding.
- VC2 enters the same node through 180k against VC1's 100k. The trimmed node
  impedance cancels, so VC2 moves the carrier 100/180 as far per volt:
  0.5452 octave/V.
- EO peaks at 10 ± 2 V (CS-60 SM p.39, PRA I). Full DEPTH at the envelope
  peak lifts the carrier **5.45 octaves**. `ring_depth` stays in octaves,
  so existing patches keep their sound. The lever law is
  `ring_depth = travel × ringDepthOctavesPerLever`; the lever's own sag into
  180k is at most 1.3 % at mid-travel. `ringSpeedHz(VC1)` gives the SPEED
  law. Both are in `ceres.hpp`.
- The 0.2–220 Hz clamp on the total carrier is kept. It is the guide's
  printed range, though nothing in the circuit enforces it once VC2 is added.

### 5. Envelope

- IC7 is an IG00159 driving IC6, an IG00151 used as the integrator into
  0.22 µF. This is the same chip pair and capacitor as the M-board VCA EG
  (SM2 p.16: IC13 IG00159, IC2 IG00151, 0.22 µF). So the ring EG follows the
  VCA-EG law, not a separate one.
- SL (pin 15) is grounded on the PRA, so sustain is zero, as modelled.
- 2DT (pin 11) is held at 15 × 3.3/10.1 = 4.901 V by 6.8k/3.3k, so the
  release is fixed, as modelled.
- The PRA's TC path is a fixed 100k with VR2 as an offset trim; the M board
  uses 82k + VR20 (B-50K) and VR16. CS-60 SM p.39 (PRA I) sets AT = DT =
  10 V ("on SHORT side") and marks the time from key-on to the 10 ± 2 V EO
  peak "Alignment by 31mS". The M-board pin-sheet law gives 1 ms there, and
  CS-60 SM p.38 confirms that law on the M board: 4 ms at 8 V, 125 ms at
  3 V, 0.3 decade/V.
- The PRA envelope is therefore aligned 31× slower than the M-board law at
  the short end. Every segment drives the same TC → EI converter, so the
  factor applies to all of them if the slope is the same. That is an
  assumption: a fixed 100k against the M board's trimmed 82k–132k span
  could put the slope anywhere from 0.246 to 0.396 decade/V, which at the
  fixed 2DT means 7.7–45 s T60.
- **Release, chosen by ear.** The 0.5.0 engine used the untrimmed pin-sheet
  law at 2DT: 0.468 s T60. The PRA-aligned law gives 31 × 0.468 =
  **14.50 s T60**. The owner chose the aligned law by ear (listening test
  ring-release, 2026-10-20), so after the last key-up the DEPTH excursion
  now returns to SPEED over 14.5 s. That is a choice between two laws; the
  0.3 decade/V slope is still an assumption, not a measurement. The
  listening render took 2DT as the rounded 4.901 V (14.50202 s against the
  engine's 14.50212 s, 7 ppm apart).
- **Lever laws.** TVR1 (ATTACK TIME) and TVR2 (DECAY TIME) are B-10K from
  +10 V to ground, wipers to AT and DT (SM2 p.26). 10 V is the SHORT end
  (CS-60 SM p.39). On the aligned law the attack runs 31 ms–31 s and the
  decay 0.428–428 s T60 over 10–0 V. `ringAttackSeconds` and
  `ringDecaySeconds` in `ceres.hpp` give them, and the release is
  `ringDecaySeconds(ringReleaseVolts)`. `ring_attack` / `ring_decay` stay
  in seconds and stop at 10 s, so the slow end of each lever (AT below
  1.64 V, DT below 5.44 V) is outside their range. They also allow times
  below the hardware's 31 ms and 0.428 s.
  `Ceres.ring_carrier_circuit_contracts` checks the laws against the
  printed alignment points, and `Ceres.native_dsp_contracts` checks the
  engine's release trajectory against an independent derivation.

### 6. Trigger

The guide's p.19 timing chart labels the ring trigger "Output if any KEY-ON
signal is available": the OR of the channel KEY-ON lines, held while any
assigned key is down. TR reaches G2 directly and G1 through a 1000 pF
coupling capacitor (the attack-start edge). TR also has a 47k pull-up and
0.01 µF, 0.47 ms. The engine's physical-key ledger, which retriggers only on
an all-keys-up → any-key-down transition, matches this and is unchanged.

## Listening tests rendered (not committed)

`/mnt/project-files/ceres/listening/2026-10-20-ring-wet-trim/` and
`/mnt/project-files/ceres/listening/2026-10-20-ring-release/`, each with a
`key.md` the listener reads after deciding. A was the 0.5.0 engine in both.
The owner's verdicts go in `Docs/decisions.md`. The wet trim and the
release now follow the owner's choices (§3, §5).

## What still needs data

1. **VR4's factory setting**, or a recording of a CS-50 at full MODULATION
   next to the same patch at zero. That one number fixes the absolute wet
   level.
2. **One ring EG time on a real unit away from the 10 V alignment point**:
   the EO trace of an attack at any AT below 10 V, or of the release. One
   trace settles the slope assumption in §5 and with it the release.
3. **IG00150 output impedance and sine purity.** The 0.047 µF on its pin 6
   and any shaper distortion are not modelled.
4. **Bus voltage calibration** (with the VCA/output component), so the
   bounded signal-pair compression in §3 can be applied rather than bounded.
5. **Part-to-part spread.** Carrier-port offset (12 µA typ, 30 µA max bias
   current) and V_BE mismatch move the dry leakage by several dB between
   units.

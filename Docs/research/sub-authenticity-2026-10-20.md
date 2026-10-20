# SUB oscillator authenticity, 2026-10-20

Scope: the shared SUB source (sine, both saws, square, noise), its rate law,
and its depth into VCO vibrato, VCF and VCA. This note covers what the
schematics establish, what the engine now encodes, and what still needs data.

Sources: **Guide** is the Yamaha CS Series Servicing Guide, cited as printed
page / PDF page. **SM1** and **SM2** are the CS-50 Service Manual 1/2 and 2/2,
cited by printed page.

## The circuit

| Item | Finding | Source |
| --- | --- | --- |
| Chip | The SUB is "VCO II", an **IG00150**: the same chip as the PWM oscillator. | Guide p.55 / PDF 64; p.49 / PDF 58 |
| Pinout | 1 control input, 2 GND, 3 timing C, 4 +15 V, 5 −15 V, 6 sine, 7 saw (the p.49 pin legend says "square wave output" for pin 7, but the SUB schematic and its text both put the sawtooth there) | Guide p.49, p.55 |
| Rate input | SPEED (PN3 SVR1, B-10k, 0 to +10 V) gives VC2, which reaches pin 1 through 180k. Pin 1 also has 330k to pin 4 (+15 V). | Guide p.55; SM2 p.27 |
| Pin 1 is a current input | The PWM IG00150 takes VC1 through 110k and spans log2(222/.2) = 10.12 octaves over 10 V. The SUB spans log2(50/.8) = 5.97. A current input predicts a ratio of 180k/110k = 1.636; the tables give 1.696, 3.5% apart. If pin 1 were a voltage input behind those dividers, the ratio would be 1.16. | Guide p.49, p.55 |
| Rate calibration | 0.8 ± 0.16 Hz, 6.7 ± 1.3 Hz and 50 ± 10 Hz at VC2 = 0, 5 and 10 V, trimmed with VR7 (B-500k on pin 8). | Guide p.55 / PDF 64 |
| Sine | Made inside the chip from its sawtooth and shown as a sine, 3 Vpp. | Guide p.55 |
| Saws | Pin 7 is the quick-rise saw (it jumps up, then falls), 3 Vpp around 0 V, on SO. OP1 (NJM4558, 100k/100k, gain −1) inverts it into the slow-rise saw. | Guide p.55; panel legend Guide p.6 |
| Square | OP2 is an open-loop comparator with the sine on its − input. Its ±15 V-rail output goes through 15k/1.8k, giving 14 V × 1.8/16.8 = 1.50 V, which matches the sine's 1.5 V peak. The square is therefore −sign(sine). | Guide p.55 |
| Noise | N goes through 22k/0.1 µF into SUI's 100k. This is already modelled. | Guide p.32; SM2 p.8 |
| FUNCTION | M1 sine (SIO), M2/M3 the two saws (SO, inverted SO), M4 square (PO), M5 N, M6 EXT. | SM2 p.27 |
| Depth levers | VCO SVR2, VCF SVR3 and VCA SVR4 are each B-10k between SUO and ground. Their wipers go to R7 V1, R7 SUB and R7 L1. | SM2 p.27 |
| VCO path | V1 goes through 100k into each board's VB node. VB also takes the touch-vibrato V2 through 100k and drives IG00153 pin 15 directly, rated at "50 cent/1.5V". | SM1 p.14; SM2 p.16; Guide p.27 |
| VCF path | R7 SUB goes through 47k to each fL and 150k to each fH. | SM1 p.14 |
| VCA path | R7 L1 goes through 120k to each LC. This is already modelled as lever travel. | SM1 p.14 |
| Phase | There is one free-running clock and no key reset path. | Guide p.55 schematic |

## What changed

Four functions now encode the panel laws for adapters (`ceres.hpp`):
`subSpeedHz`, `subSpeedLeverHz`, `subVibratoLeverCents` and
`subFilterLeverOctaves`. **The Patch parameters keep their meaning, so saved
patches render bit-for-bit unchanged.**

- **SPEED** runs exponentially through the 0 V and 10 V points:
  `f = 0.8 · 62.5^(V/10)`. This gives 6.325 Hz at 5 V, 5.6% under the printed
  6.7 Hz nominal and well inside its ±1.3 Hz tolerance.
  - It is the same end-point convention the factory presets already use for
    the PWM IG00150.
  - The other defensible pinning, through the 5 V point, moves every rate by
    6%. That difference is far below the trim tolerance and below what anyone
    could set by ear, so it did not warrant a listening test.
  - The lever drives 180k into a current-summing node, so travel `x` gives
    `10·x/(1+x(1−x)·10k/180k)` volts. Mid-lever lands 1.4% low in volts and
    2.8% low in rate (6.15 Hz).
- **VCO depth**: with pin 15 drawing no current, VB is the mean of V1 and V2.
  The full-lever SUB (1.5 V peak) is then 0.75 V at the pin, or **25 cents
  peak**.
  - The wiper sees four (100k + 100k) paths to the low-impedance V2 outputs,
    which is 50k in total. Mid-lever therefore gives 11.9 cents instead of 12.5.
  - The same 100k into VB makes full touch at a full touch-VCO lever also
    25 cents for `pressure_vibrato`, with the existing +1/3 offset.
- **VCF depth**: full lever is 1.5 V through 47k, measured against FL's
  provisional 0.65 oct/V through 39k. That is **0.809 LP octaves per SUB unit**
  (HP 0.273 through the existing 0.3374 ratio).
  - The wiper drives four 47k plus four 150k, which is 8.95k. Mid-lever
    therefore delivers only 78% of the linear value.
  - The f-node resistors to ground (R6/R7, 1.8–3.0k by chip rank, SM2 p.16)
    are neglected. The tremolo LC path is treated the same way. Including them
    would ease the sag by about 4%.
  - The same 47k means full `pressure_wah` at full touch is also 0.809 LP
    octaves.

The `vibrato_cents` (0–100) and `filter_lfo` (0–5) ranges still include values
the hardware lever cannot reach. They are left as they are so that patches keep
working.

## Kept as shipped, and why

- **Square sign.** The hardware square is −sign(sine); the engine's is
  +sign(sine).
  - The single clock free-runs, its power-on phase is arbitrary, and only one
    waveform is selected at a time. The relative sign shows only at the
    instant of switching FUNCTION.
  - Changing it would shift every square-SUB render by half a cycle for no
    audible gain.
- **Square edges.** At the NJM4558's typical 1 V/µs, the comparator crosses its
  28 V swing in about 28 µs. At 48 kHz the BLEP spans one 21 µs sample, so the
  two are equivalent for a modulation source.
- **Sine shape.** The IG00150's internal saw-to-sine shaper is not published.
  Choosing any residual distortion, such as a rounded triangle, would mean
  inventing a constant, so the sine stays ideal.
- **Saw reset time.** The IG00150 works like the IG00153: C charges during a
  one-shot, then ramps. The one-shot width is internal and unpublished. The
  VCO III's is about 60 µs, which is 0.3% of a 50 Hz period, so it is
  inaudible as modulation either way.
- **EXT.** FUNCTION M6 takes the rear EXT IN through its preamplifier
  (Guide p.56). `Synth` has no audio input, so this stays an adapter matter.

## What would close the remaining gap

1. **IG00153 pin 15 input impedance.** It decides whether full VCO depth is
   exactly 25 cents or less. This needs a measurement or the chip's internal
   schematic.
2. **IG00150 sine distortion and saw reset width.** These need a scope capture
   of SIO and SO from a calibrated unit.
3. **FL slope.** The VCF depth inherits the 0.65 oct/V FL slope, which is still
   provisional (VCF component).
4. **The trim point of VR7.** Is it 5 V = 6.7 Hz or the end points? This is
   within tolerance either way and only matters to within 6%.

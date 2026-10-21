# SUB oscillator authenticity, 2026-10-20

Scope: the shared SUB source (sine, both saws, square, noise, EXT), its rate
law, and its depth into VCO vibrato, VCF and VCA. This note covers what the
schematics establish, what the engine now encodes, and what still needs data.

Sources: **Guide** is the Yamaha CS Series Servicing Guide, cited as printed
page / PDF page. **SM1** and **SM2** are the CS-50 Service Manual 1/2 and 2/2,
cited by printed page.

## The circuit

| Item | Finding | Source |
| --- | --- | --- |
| Chip | The SUB is "VCO II", an **IG00150**: the same chip as the PWM oscillator. | Guide p.55 / PDF 64; p.49 / PDF 58 |
| Pinout | 1 control input (VI), 2 GND, 3 timing C, 4 Vee (−15 V), 5 Vcc (+15 V), 6 sine (SIO), 7 saw (SO), 8 Iadj. The guide's p.49 legend calls pin 7 a square output and swaps the supply pins; the SM2 pin sheet, the SUB schematic and its text put the sawtooth on 7. | SM2 p.9; Guide p.49, p.55 |
| Rate input | SPEED (PN3 SVR1, B-10k, 0 to +10 V) gives VC2. The guide's sketch takes it to pin 1 through 180k. The CS-50 SUB board has 120k plus VR13 (B-100K trim) in series, 120k to 220k, and 330k from pin 1 to −15 V. | Guide p.55; SM2 pp.8, 27 |
| Pin 1 is a current input | The PWM IG00150 takes VC1 through 110k and spans log2(222/.2) = 10.12 octaves over 10 V. The SUB spans log2(50/.8) = 5.97. A current input predicts a ratio of 180k/110k = 1.636; the tables give 1.696, 3.5% apart. If pin 1 were a voltage input behind those dividers, the ratio would be 1.16. | Guide p.49, p.55 |
| Rate calibration | 0.8 ± 0.16 Hz, 6.7 ± 1.3 Hz and 50 ± 10 Hz at VC2 = 0, 5 and 10 V. The guide trims it with VR7 (B-500k on Iadj); the CS-50 board has VR9 (B-500K) there and VR13 for the scale. | Guide p.55 / PDF 64; SM2 p.8 |
| Sine | Made inside the chip from its sawtooth and shown as a sine, 3 Vpp. The SM2 pin sheet draws SIO at 3.20 Vpp and SO at 3.5 Vpp. | Guide p.55; SM2 p.9 |
| Saws | Pin 7 is the quick-rise saw (it jumps up, then falls), 3 Vpp around 0 V, on SO. OP1 (NJM4558, 100k/100k, gain −1) inverts it into the slow-rise saw. | Guide p.55; panel legend Guide p.6 |
| Square | OP2 is an open-loop comparator with the sine on its − input. Its ±15 V-rail output goes through 15k/1.8k, giving 14 V × 1.8/16.8 = 1.50 V, which matches the sine's 1.5 V peak. The square is therefore −sign(sine). | Guide p.55 |
| Noise | N goes through 22k/0.1 µF into SUI's 100k. This is already modelled. | Guide p.32; SM2 p.8 |
| FUNCTION | Six positions (Guide p.6): M1 sine (SIO), M2/M3 the two saws (SO, inverted SO), M4 square (PO), M5 N, M6 EXT. The common T feeds the SUB board's SUI. | SM2 pp.9, 27 |
| EXT IN | Rear jack: tip to the top of the A-50K LEVEL pot (PN5 VOLUME), sleeve to its foot, wiper to PRA EX (53). | Guide p.7; SM2 pp.2, 27 |
| EXT preamp | PRA IC3-1/2 (NJM4558D), non-inverting: 100k feedback over 1k, gain 101. Its output reaches EXO (56) through a 10 µF non-polar capacitor, with 100k from EXO to ground. | SM2 p.22; Guide p.56 / PDF 65 |
| SUI buffer | SUI has 100k to ground and the IC1-1 follower (NJM4558) drives SUO. SUO feeds the three B-10k depth levers and the touch level amplifiers. | SM2 pp.8, 9 |
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
- **EXT.** Implemented after the owner approved it; see the next section.

## EXT IN

The owner approved adding the CS-50's EXT modulation input (2026-10-20).
FUNCTION EXT is `lfo_waveform` 5, and the rear LEVEL pot is `ext_level`.

- **Units.** A front end calls `Synth::externalInput(x)` once per host frame,
  before `process()`. One unit is `subExternalUnitVolts` = 1.5 V / 101, about
  14.85 mV at the jack. The ×101 preamp lifts that to 1.5 V at SUI, the
  internal waveforms' peak. So a full-scale ±1 host signal at `ext_level` 1
  modulates every route (VCO, VCF, VCA, touch vibrato and touch wah) as deeply
  as the internal SUB, above the coupling's corner.
- **Line sources.** The jack is sensitive. A 0 dBu source (1.10 V peak) is 74
  units, so it needs `ext_level` near 0.014 for the internal depth, and
  −10 dBV (0.45 V peak) needs about 0.033. The guide lists voice, rhythm boxes
  and records as sources (pp.6, 56). The LEVEL pot does that attenuation on
  the hardware too. A front end that wants the absolute sensitivity feeds
  `volts / subExternalUnitVolts`.
- **LEVEL.** `ext_level` is the wiper ratio, 0..1, default 1. The A-50K's
  taper is not published, so a front end maps its own knob onto it. The
  preamp input does not load the wiper.
- **Coupling.** The 10 µF capacitor sees 100k ∥ 100k (τ 0.5 s, 0.318 Hz) while
  EXT is selected and EXO's own 100k (τ 1 s, 0.159 Hz) otherwise. The preamp
  and the capacitor run whatever FUNCTION selects, so the charge carries
  across a switch.
- **Swings.** NJM4558 typicals at ±15 V, so an estimate: ±14 V into the
  preamp's ≥10k load (101k ∥ 50k) and ±13 V from IC1-1 into the levers'
  ≥2k load (about 3k). That is 9.33 and 8.67 SUB units, 19.4 dB and 18.8 dB
  above full depth. The square's 15k/1.8k divider assumes the same 14 V.
- **Host rate.** Each host sample holds until the next call and is
  interpolated linearly across the internal oversampled steps, exact at 1×.
  This is a digital adaptation, not a hardware stage. Nonfinite samples read
  as 0, and finite ones are bounded at ±1e9 units as a numerical guard.
- **State.** `reset()` clears the jack and the capacitor. `allSoundOff()` keeps
  them, as it keeps the SUB clock and the SUB noise filter.
- **Not modelled.** The preamp's 3 MHz typical gain-bandwidth (29.7 kHz closed
  loop) and 1 V/µs slew rate act only at the top of the audio band or in hard
  overload. Op-amp offsets are blocked by the capacitor or are below the
  model's other uncertainties. The switch's break-before-make instant is not
  modelled.

Evidence:

- `Ceres.sub_oscillator_contracts` solves the chain as a continuous circuit
  and checks it at 1×, 2× and 4×: passband gain and phase (1e-6), both time
  constants and the switch (1e-9 and 1e-5), both swings, that the internal
  sources ignore the jack bit for bit, and that the same SUI signal from EXT
  and from an internal source moves all five routes identically. Fifteen code
  mutations of the chain and its routes each fail it.
- `Ceres.external_input_audio_parity` renders 270,000 samples over three
  rates, three qualities and five scenes in both cores, byte-identical.
- `Ceres.embedded_allocation_contract` feeds the jack, invalid samples
  included, at all three qualities without allocating.
- With nothing fed and `lfo_waveform` 0..4, every existing native and embedded
  parity stream (all `ParityDriver` modes and the configuration driver, 18
  streams and 6,512,640 float32 samples) is byte-identical to Ceres 0.5.0
  (0f1d00f). `Tests/check_external_parent_parity.py` recompiles both cores
  with the unchanged drivers; its receipt is
  [sub-ext-parent-parity-2026-10-20.json](../validation/sub-ext-parent-parity-2026-10-20.json).

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
5. **SUB output levels.** The guide gives 3 Vpp for every SUB waveform and the
   square's divider gives 3 Vpp too, but the SM2 IG00150 pin sheet draws SIO at
   3.20 Vpp and SO at 3.5 Vpp. If those are in-circuit levels, the sine is
   7% and both saws 17% deeper than the square. The engine keeps one 1.5 V
   peak. Settling it needs a capture from a calibrated unit; until then the
   two readings could be put to a listening test.
6. **EXT LEVEL taper.** The A-50K's travel law is not printed; `ext_level` is
   its ratio. A measured pot, or the part's catalogue curve, would let a
   front end map the knob.

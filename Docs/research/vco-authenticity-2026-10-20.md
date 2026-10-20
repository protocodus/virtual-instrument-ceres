# VCO authenticity review, 2026-10-20

Scope: `struct Oscillator` in `Source/DSP/ceres.cpp` (VCO III one-shot and
conditioned ramp, IG00158 WSC saw/pulse/sine), the PWM source, the pitch path
(FT/TRANSPOSE/PITCH) and the waveform levels and polarity into the mixer.
Board-to-board tolerance is out of scope (voice-variation work).

Sources: Yamaha *CS Series Servicing Guide* (cited "guide", printed page / PDF
page), CS-50 *Service Manual 1/2* ("SM1") and *2/2* ("SM2"), cited by PDF page.

## What the documents establish

### VCO III (IG00153) timing law

- Guide printed p.28 / PDF 37: Iref = 800 uA charges C2 for the one-shot time
  tau while the key current I = Kv/RFT discharges it for t. Charge balance
  `C2*Vpp = (Iref-I)*tau = I*t` gives `f = I/(Iref*tau)`, linear in Kv. The
  one-shot therefore fills `r = f*tau` of every period exactly, at every pitch,
  until I reaches Iref (r = 1, f = 1/tau, 16.7 kHz at the nominal 59.73 us),
  where the discharge interval vanishes.
- Guide printed p.29 / PDF 38 and SM1 PDF 9 (printed 19): the Normal table is
  0.25/0.5/1/2/4 V -> 130.8/261.6/523.2/1046/2093 Hz, i.e. linear 523.2 Hz/V.
  The guide's sentence on p.29 says pin 2 is exponential and pin 15 linear;
  its own table and the SM1 pin sheet (pin 15 VIB "50 cent/1.5V") show the
  reverse. The engine follows the table: key CV linear in Hz (the KAS ladder
  supplies the exponential), vibrato exponential in cents.
- Trim procedure (guide p.29): VR2 (input offset, pins 3/4) sets C2 at
  K = 250 mV; VR3 (the one-shot) sets the top note (the guide misprints "C2
  at 6V"). A two-point offset/scale trim of an exactly linear law leaves no
  tracking error at the extremes in the ideal circuit; any residual
  nonlinearity (comparator delay, FET switch time) is not documented. VR3
  trimming tau to the scale is why the default reset is
  `1/(523.2*40k*800uA)` (already shipped).
- **Factory pitch procedure** (sibling-document evidence: Yamaha CS-60
  Service Manual, printed p.17 / PDF 19, "Pitch Adjustment", which lists the
  CS-50 values row by row): after 10 minutes' warm-up, TU is set to
  4.000 V +/-0.1 % (CS-50: SUB VR11); with OCT-UP (4') selected, M-board VR3
  (B-5K, the one-shot) sets the C6 key to 4186 Hz +/-1 cent and VR2 (B-500,
  offset) the C2 key to 261.6 Hz +/-1 cent; VR4/VR5/VR6 (B-1K/2K/5K) then set
  the C6 key to 2093/1046/523.2 Hz +/-1 cent at Normal/1-down/2-down. At 4'
  RFT is R1, a 20k 0.1 % metal-film resistor with no trim (SM2 PDF 12 table),
  so the trim fixes `Iref*tau = 1/(1046.4 Hz/V * 20k)`: the shipped
  59.73 us at the guide's nominal 800 uA, now justified by the actual
  procedure rather than by the Normal 40k branch. The octave trims hold every
  transposition to +/-1 cent at C6; the engine's exact octaves are inside that.
  The +/-1 cent trim window is the hardware's own tuning tolerance (data for
  the voice-variation work).
- **A3 = 443 Hz.** The same procedure's last step re-trims the tuning
  reference "when A3 key is depressed, so that the output is 443 Hz", and the
  guide's PITCH description (printed p.5 / PDF 14) gives the centre click as
  A3 = 443 Hz. A factory CS-50 at PITCH centre therefore plays 11.7 cents
  above A440. The engine's `tune_cents = 0` is A440. Not changed here: it is
  a whole-instrument reference choice (every preset, every host) that the
  owner should make; a default of +11.73 cents would reproduce it exactly.
- Transposition (guide p.30 / PDF 39; SM2 PDF 12 table): RFT 20k/40k/80k/160k
  with individual trims for 4'/8'/16'/32', so octaves are exact after trim;
  the engine's `octave` -2..+1 matches. PITCH: the KAS ladder takes 3..6 V on
  TU, 4 V at centre (guide printed p.23 / PDF 32, "D-A converter"), i.e.
  -4.98..+7.02 semitones; `tune_cents` -500..700 matches. The fine knob adds
  +14/-11 cents (guide printed p.5 / PDF 14).
- VCO OUT7 is 3 Vpp (guide p.29) or 3.5 Vpp (SM1 PDF 9, pin 7); the WSC
  outputs are 3 Vpp (guide p.31, SM2 PDF 15 pins 2/5/7), TO 3.2 Vpp. Saw,
  pulse and sine are therefore equal-amplitude sources, as modelled.

### IG00158 pin sheet (SM2 PDF 15, printed 21)

This page was not used by earlier work. It adds:

- Pin 3 TO: triangle from the sawtooth folded at the slice level SL (pin 4,
  "inverting level which makes triangular wave from sawtooth"). On the M
  board pin 4 is open (SM2 PDF 12), so the slice is the chip's internal
  default; the engine folds at the ramp midpoint.
- Pin 15 STI: sine-shaper input, "the triangular wave (TO) is provided".
  On the M board TO reaches STI through 4.7 uF NP with 22k to ground
  (SM2 PDF 12), a 1.54 Hz high-pass.
- Pin 5 SO (overbar): "Output of inverted sawtooth wave".
- Pins 9-11: PI is fed the sawtooth; the drawing shows PO high from the
  ramp's reset jump until the descending ramp crosses the PW/PWM level.
- M board wiring (SM2 PDF 12): PI, SI and TL are all tied to T1 (VCO OUT7),
  22k to ground. SO and PO each pass an identical FET gate (FET6/FET7) and
  150k into one summing node; no inverter on either branch.

## Changes made

1. **Pulse comparator reads the conditioned ramp.** The comparator level sits
   at `p = 1-2d` on the conditioned ramp p (fixed bottom, fixed slope per
   period), so it is crossed at phase d. While the one-shot holds p on its top
   plateau there is no crossing, and a level above the ramp's start `1-2r`
   (d < r) is crossed at the plateau step. The logical pulse therefore falls
   at `max(d, r)`. The previous code kept the edge at d even inside the
   plateau, labelled as an independent approximation. Audible only for deep
   PWM at upper notes: d reaches 0.10 with PW at 50 % and full PWM, and
   r > 0.10 above about 1.67 kHz (r = 0.125 at C6 on 8').
2. **One-shot fraction follows `r = f*tau` up to the hardware top.** The
   former `r <= .35` clamp cut the law at 5.86 kHz; the CS-50 reaches 6.33 kHz
   (C6 on 4' = 4186 Hz, PITCH at TU 6 V x1.5, +14 cents), r = 0.378. The
   guard is now 0.9, which only bounds MIDI notes and engineering tuning
   beyond anything the instrument plays.

`Tests/OscillatorCircuitTests.cpp`: the old deep-PWM assertion ("fixed-edge
reduction must not depend on the reset plateau") encoded the approximation
in change 1 and contradicts the wiring above; it is replaced by an independent
per-sample check of `max(d, r)` against the same PWM sine, a check that the
probe really puts d below r, and a plateau/ramp/pulse check at 6.33 kHz and
10 kHz (both above the old clamp).

## Saw/pulse relative polarity: evidence conflicts

The engine negates the pulse so equal-level saw and pulse reinforce. That rests
on one public recording (Marc Doty, AutomaticGainsay, 2013; see
`Docs/accuracy/waveform-mixing-20261019/`), where both-on/saw RMS is 2.35-2.47
against 0.89 for the cancelling sign.

Yamaha's documents, read literally, say the opposite:

- Guide printed p.31 / PDF 40 labels SI -> SO an inverter and says PWM moves
  only the pulse's fall, with the rising edge fixed (the "Fixed" sketch).
- The SM2 pin sheet draws SO as the inverted saw and PO high from the reset
  jump until the level crossing.
- The fixed edge must be the reset jump (the only threshold-independent event
  on the ramp), and SI jumps upward there (guide p.28/29 OUT7 sketches). So at
  reset SO drops while PO rises: opposite jumps, partly cancelling fundamentals.
- The M board gives both branches the same FET gate and 150k (SM2 PDF 12):
  nothing on the board reconciles the two.

Both readings share the same edge geometry (fixed edge at reset, moving edge
at d, 9:1 at PW 10 V), so the only open bit is the sign of PO relative to SO
inside the IG00158. The sketches are block-level illustrations of a hidden
chip, while the recording is a quantitative measurement of the whole
instrument; the shipped sign is kept, and the conflict is now recorded beside
the code. **What would close it:** a scope capture of IG00158 pins 5 and 7 (or
T2/T3) on one board, or a second dry recording that holds one note while
switching saw, square and both at more than one pulse width (that also tells a
sign flip from a half-cycle offset, which are identical only at 50 %).
An attempt to re-analyse further recordings was not possible from this
environment.

## WSC sine

The engine's sine is `-cos(pi*p)` on the conditioned ramp, which is exactly the
fold `tri = 2|p|-1` followed by an ideal `sin(pi/2*tri)` shaper, so the
plateau and fold match the pin sheet. Two things remain:

- **Shaper transfer.** No transfer, transistor diagram or harmonic measurement
  of the IG00158 (or the CS-60/CS-80, which use the same chip) is published.
  Searches found only Ando Pilve's 2026 clone announcement ("authentic
  waveform phases, wave folding methods"), with no data. The textbook bipolar
  topology, an undegenerated differential pair, has its own minimum-THD drive
  (k = 1.402 for tanh(k*tri): THD 1.36 %, H3 -54 dB, H5 -39 dB, H7 -45 dB).
  That is a defensible candidate but not evidence, so it was an A/B listening
  test (`/mnt/project-files/ceres/listening/2026-10-20-vco-sine-shaper/`,
  key in `key.md`). **Owner verdict, 2026-10-20: B.** The differential-pair
  shaper `tanh(k*tri)/tanh(k)`, k = 1.4020, now ships; it is chosen by ear,
  not measured. Its mean square at unit peak, `(1-tanh(k)/k)/tanh(k)^2` =
  0.46928 against an ideal sine's 0.5, lowers the four-sine 0 dBm output
  anchor by 0.275 dB, and the output rails with it.
- **STI coupling.** The 4.7 uF/22k coupling removes the triangle's mean, which
  the plateau makes `2r^2` of peak (0.2 % at C4, 3 % at C6 on 8', 29 % at the
  6.33 kHz top). For the ideal shaper that needed a transfer beyond its
  nominal input range, which is unpublished. The shipping tanh shaper is
  defined there, but the auditioned candidate omitted the offset, so it is
  still not applied and is noted in the code.

## PWM source

Already as documented: VCO II exponential at 0.2/6.7/222 Hz for 0/5/10 V
(guide printed p.49), 3 Vpp sine; PWM level control VR8 trimmed to unity at
10 V; full PWM is +/-1.5 V = +/-40 % duty (guide p.31); PW 0..10 V = 50..90 %;
duty above 99 % saturates (the guide's "990 %" is a misprint), matching the
0.99 clamp. VCO II's own sine shaper is likewise undocumented.
Sibling note: the CS-60 manual (printed p.37 / PDF 39) trims its PWM VCO II
to 150 +/-10 ms at 5 V (6.67 Hz) and 5 +/-0.5 ms at full speed (200 Hz,
181.8-222.2 Hz), and its IG00150 pin sheet (printed p.17) says 200 Hz at 10 V;
the CS-50 guide's figure gives 222 Hz, the edge of that window. The CS-50 SUB
board differs from the CS-60's, so the CS-50 figure stays.

## Score

7 -> 7.5. The one-shot and comparator now follow the documented circuit across
the instrument's whole range instead of an approximation and a clamp, and the
pitch path has been checked against the factory trim procedure: the one-shot,
octave and scale laws match it; the only discrepancy is the A3 = 443 Hz
reference, left for the owner.
9 needs: the IG00158 sine transfer (scope capture or spectrum of SIO/T-point,
any CS-50/60/80), and either a pin-level capture or a second qualifying
recording for the saw/pulse sign. Neither is published.

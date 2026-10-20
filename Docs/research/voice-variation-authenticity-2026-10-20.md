# Board-to-board variation: authenticity pass, 2026-10-20

The CS-50 has four separate M voice boards (SM2 printed pp.17-18, PDF 13-14,
"M1~M4"). Before this pass Ceres ran four bit-identical boards; the only
differences were the engineering controls `voice_spread_cents` (a fixed
symmetric spread) and `drift_cents` (a random walk), both VCO-only and
default 0. Neither is a tolerance model.

This pass adds the per-board variation the service documents bound, as two
amounts that default to 0 (identical boards, the shipping sound):

| Key | Range | 1 means |
| --- | --- | --- |
| `board_pitch_tolerance` | 0..1 | the VCO trim residual the tuning method allows |
| `board_filter_tolerance` | 0..1 | the IG00156 rank-bin residual of the HP and LP cutoffs |

Each seed is one instrument: the offsets are drawn once per physical board
slot, from a dedicated seeded stream, in a fixed order. The shared audio and
drift stream, the oscillator start phases and the noise sequence are
unchanged, and board *i* is the same unit at every polyphony and quality.
`reset()` keeps the same boards.

## What the hardware trims, and what it does not

The four M boards carry their own trimmers (VR1-VR21, Guide printed p.65 /
PDF 74 board drawing). The service guide prints a procedure and a tolerance
for the VCO only.

### VCO: trimmed, residual +/-1 cent at each trim point

- IG00153 is a **linear** converter: the one-shot/integrator gives
  `f = I/(Iref*tau)` with `I = Kv/Rn` (Guide printed pp.27-29 / PDF 36-38).
  The NORMAL table maps Kv 250 mV, 500 mV, 1 V, 2 V, 4 V to 130.8, 261.6,
  523.2, 1046, 2093 Hz (p.29), so Kv is proportional to the NORMAL frequency
  and the C2 key's Kv is 1/16 of the C6 key's.
- Tuning method (Guide printed p.65 / PDF 74), per board: at OCT-UP, C6 key
  to C7 (4186 Hz) and C2 key to C3 (261.6 Hz), each **+/-1 cent**, repeating
  (VR2 offset, VR3 scale; the p.29 figure names the two trims). Then the C6
  key at NORMAL to 2093 Hz (VR4), 1 OCT DOWN to 1046 Hz (VR5) and 2 OCT DOWN
  to 523.2 Hz (VR6), each **+/-1 cent**.
- The transposition resistors are B-mark 0.1% metal film and the timing
  resistors G-mark 2% (SM2 printed p.17, note 7; SM1 parts list p.3 / PDF 15
  confirms 0.1% 20k/40k/80k/160k). They sit inside these trims, so their own
  tolerance leaves no residual beyond the trim band.

Model. A board is `f = G_t*(Kv + Voff)`: a gain per transposition resistor
and one input offset. Five residuals per board, each uniform in +/-1 cent:
`eLow` (OCT-UP, C2 key) and the C6-key error at each of the four
transpositions. With `r = 2^((eLow-eUp)/1200)` the offset is
`d = Voff/Kv(C6) = (r-1)/(16-r)`, and the error at any key is
`e_t + 1200*log2((1 + d/x)/(1 + d))`, `x = Kv/Kv(C6)`. A board is therefore
within +/-1 cent at every trim point and within +/-3 cents from the C2 to the
C6 key, its error largest at the bottom of the keyboard. Kv carries the key
(including glide), master PITCH and the MIDI bend; vibrato enters the FT pin
separately and is not scaled. TRANSPOSE values between switch positions
interpolate the gain residual linearly (the hardware has only the four
positions).

Assumption: uniform within the printed band. The guide prints the band, not
how technicians land inside it. Post-trim ageing and temperature drift are
not printed (the method only says to warm up 10 minutes) and are not
modelled; `drift_cents` stays the separate engineering control it was.

### VCF cutoff: not trimmed, binned by chip rank

- The IG00156 has a pin-3 "fc" input that "sets the control current of the
  cut off frequency" (SM1 printed p.19 / PDF 9), and the tuning method has no
  filter step.
- The M-board notes give the IG00156 three ranks and a resistor per rank
  (SM2 printed pp.17-18 / PDF 13-14, note 9): R6 = 2.7k / 2.2k / 1.8k
  (IC10, HP) and R7 = 3.3k / 2.7k / 2.2k (IC11, LP) on board NA03574;
  NA03655 uses 3.0k / 2.4k / 2.0k for R7. Every column spans a factor 1.5 over
  two steps, i.e. a step of sqrt(1.5) = 1.2247.
- Selecting a resistor by rank corrects a chip to within half a rank step:
  +/-log2(1.2247)/2 = **+/-0.14624 octave**, HP and LP independent.

Model. The variable-gm part of each cutoff is scaled by `2^(amount*u*0.14624)`,
`u` uniform in [-1, 1], per board and per filter. The 4.7 M feed path is not
part of the chip and keeps its 22.58 Hz floor. Inference, labelled as such:
fc proportional to the pin-3 current (1/R); bins contiguous and one step wide;
a chip uniform within its bin. If pin 3 instead offset the exponential
control voltage, the bound in octaves would be similar but not identical.

### Not modelled: no printed tolerance

| Mechanism | Hardware | Why left out |
| --- | --- | --- |
| Q | Qo trimmer per filter, Q = 5 at Vq = 0 V (SM1 p.19) | trim tolerance not printed |
| EG times | .22 uF timing caps, TR trimmers (VR19 etc.), IG00152/IG00159 in ranks A-D | no rank table, no cap or trim tolerance printed |
| VCA gain | IG00151 pin-1 trim: 0 at 0 V, maximum at 10 V (Guide p.41 / PDF 50) | no tolerance printed |
| WSC levels | IG00158 trimmer | no procedure in the CS-50 guide |
| 1.5 nF, 4.7 M, 180 pF | unmarked capacitors; 4.7 M is a "solid" (carbon composition) resistor (SM1 parts list p.3) | no tolerance printed |

Inventing a typical 5 % or 10 % for these would be a fitted number; the
brief's limit applies.

## Listening test

`/mnt/project-files/ceres/listening/2026-10-20-voice-variation-board-tolerance/`,
letters A-D, `key.md` alongside. A is the shipping engine (identical boards).
The candidates are the full documented model, the untrimmed (filter) part
only and the trimmed (VCO) part only. Factory `brass1`, seed 50, four-board
unisons on MIDI 48 and 36 and a four-note chord; whole-file RMS matched. The
default stays at identical boards until the owner chooses.

## Verification

- `Ceres.board_tolerance_contracts` (Tests/BoardToleranceTests.cpp): defaults
  keep identical boards; every trim point within +/-1 cent across 40 seeds x 4
  boards and the C2-C6 range within 3 cents; untrimmed keys follow one linear
  VCO rebuilt independently from the OCT-UP trim points; cutoff residual
  within half a rank step recomputed from the printed resistors, scaling
  linearly in octaves with the amount; the shared noise stream and start
  phases unchanged; boards fixed across polyphony, reset and seed.
- `Ceres.board_tolerance_audio_parity`: native and embedded produce identical
  audio with both tolerances at 1, and the tolerances change it.
- With both amounts 0 the native parity driver (all seven modes) and the
  configuration parity driver are byte-identical to `0845146`.

## What would close the gap

- The M-board alignment procedure for VCF Q, EG timing, VCA gain and WSC
  level, with tolerances (the CS-60 service manual's section VI is cited in
  ceres.cpp but is not among the sources here).
- The IG00156 pin-3 transfer (current vs. control-voltage offset) and the
  rank bin edges, or measured cutoffs of a few boards at a fixed Vfc/Kv.
- Measured per-board pitch error of a calibrated unit across the keyboard,
  to replace the uniform-in-band assumption, and a documented thermal drift.
- Tolerances of the 1.5 nF integrator, .22 uF EG and 4.7 M feed parts.

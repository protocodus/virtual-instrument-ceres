# Board-to-board variation: authenticity pass, 2026-10-20

The CS-50 has four separate M voice boards (SM2 printed pp.17-18, PDF 13-14,
"M1~M4"). Before this pass Ceres ran four bit-identical boards; the only
differences were the engineering controls `voice_spread_cents` (a fixed
symmetric spread) and `drift_cents` (a random walk), both VCO-only and
default 0. Neither is a tolerance model.

This pass adds the per-board variation the service documents bound, as three
amounts that default to 0 (identical boards, the shipping sound):

| Key | Range | 1 means |
| --- | --- | --- |
| `board_pitch_tolerance` | 0..1 | the VCO trim residual the tuning method allows |
| `board_filter_tolerance` | 0..1 | the IG00156 rank-bin residual of each filter's control sensitivity |
| `board_envelope_tolerance` | 0..1 | the VCF EG (+/-10%) and VCA EG (+/-2%) output-level check bands |

Each seed is one instrument: the offsets are drawn once per physical board
slot, from a dedicated seeded stream, in a fixed order. The shared audio and
drift stream, the oscillator start phases and the noise sequence are
unchanged, and board *i* is the same unit at every polyphony and quality.
`reset()` keeps the same boards.

## What the hardware trims, and what it does not

The four M boards carry their own trimmers (VR1-VR21, Guide printed p.65 /
PDF 74 board drawing). The CS-50 guide prints a procedure and a tolerance
for the VCO only; the VCF, EG and VCA come from the sibling CS-60 manual
below.

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

### Sibling evidence: the CS-60 alignment of the same M board

The CS-50 guide prints only the VCO tuning. The CS-60 service manual's
section VI (printed pp.37-39 / PDF 39-41) aligns the VCF, EG and VCA of the
M board, and that board is the CS-50's:

- SM1 parts list p.2 (PDF 14): M circuit board NA04485, "Common Models:
  CS-60".
- CS-60 SM printed p.15 (PDF 17), "M (Master) Circuit": its resistor tables
  are headed "CS-60,50" (R1-R4 0.1% 20k/40k/80k/160k, VR4-VR6 B-1k/2k/5k,
  the values on the CS-50's own M sheet, SM2 p.17) and "Constant value of R6,
  R7 in IC10,11 according to rank" with the same CS-50,60 column.
- The trimmer layout drawn on CS-60 p.37 is the CS-50 guide's p.65 board
  drawing (VR7-VR10, VR19, VR15 / VR18, VR20, VR16, VR17 / VR11-VR14, VR21).
- The procedure states the CS-50's own difference where there is one
  (p.39: VR14 output alignment ".24V (CS-50 is 0.6V)").

Every value below that comes from the CS-60 manual is labelled sibling
evidence in the code.

### VCF: sensitivity binned by chip rank; cutoff and Q trimmed by ear

The "CS-60,50" schematic corrects the first pass of this work: the rank
resistors are not on the IG00156 fadj pin (a fixed 220k) but load each
filter's **Vfc node** (R6 on IC10's f1 node, R7 on IC11's f2 node, each with
0.01 uF). The panel lever, the VCF EG buffer (100k HP, 47k LP), SUB/touch via
R7, and the VR7 (HP) / VR9 (LP) B-100k offset trims through 150k all sum
there. R6/R7 (~2.2k) are small beside those sources (60k or more in
parallel), so the node voltage per input scales with R6/R7: the rank sets the
chip's control **sensitivity** (octaves per volt).

- Ranks (SM2 printed pp.17-18 note 9; CS-60 p.15): R6 2.7k/2.2k/1.8k, R7
  3.3k/2.7k/2.2k (CS-60/50 column 3.0k/2.4k/2.0k). Every column spans x1.5
  over two steps, a step of sqrt(1.5) = 1.2247.
- A chip lands within half a step: Vfc octaves scaled by 2^(u*0.14624),
  |u| <= 1, HP and LP independent.
- Alignment (CS-60 p.38, III-IV): HPF lever centre, LPF and RES high; "adjust
  VR7 for cut off frequency and VR8 for Q so that the tone color of the M
  circuit board may be the same as that of other M circuit boards"; for the
  LPF, VR9 likewise. Matched by ear, with no numeric tolerance, so the static
  cutoff and Q stay identical across boards here.

Model. Each board scales the octaves of its Vfc-driven terms (VCF EG, SUB,
touch, BRILLIANCE) by its own HP or LP factor. The static panel cutoff and Q
are unchanged. Inference, labelled: node gain proportional to R6/R7 (exact
to the ~3% the parallel sources add), and chips uniform within contiguous
one-step bins. Strictly, the static cutoff away from the trim setting would
also move by (sensitivity - 1) times its distance from the trim point in
volts. Ceres's static cutoff is defined in Hz at C4, not in panel volts, so
that term is left out. It is listed as a gap.

### EG output levels: check bands

- VCF EG (CS-60 p.37, II): with AL, IL at 10 V, TP1 must show +5 V +/-0.5 V
  and -4 V +/-0.5 V; only the offset is trimmed (VR18, 0 +/-0.01 V). One
  buffer gain must meet both bands, so the VCF EG reaches both filters within
  **+/-10%** (the tighter band). It is one gain for HP and LP (one buffer,
  TP1).
- VCA EG (CS-60 p.38, V): peak 10 V **+/-0.2 V** at TP2 (+/-2%). The VCA is
  linear in the EG (Guide p.41), so this is a per-board level of +/-0.17 dB.
- Uniform within each band, as for pitch.

### Not modelled: no printed tolerance

| Mechanism | What the documents give | Why left out |
| --- | --- | --- |
| Static cutoff and Q | VR7, VR9 (cutoff), VR8, VR10 (Q) matched by ear (CS-60 p.38) | no numeric residual |
| EG attack time | VR19, VR20: lengthen if longer than 125 ms at 3 V (CS-60 pp.37-38) | trim direction only, no tolerance |
| EG decay/release times | .22 uF caps, IG00152/159 ranks A-D | no rank table, no tolerance |
| VCA levels | VR11 1 Vpp, VR12 800 mVpp, VR3, VR14 0.6 V on the CS-50 (CS-60 pp.38-39) | alignment targets with no tolerance |
| VCA-EG floor | TP2 -200 +/-20 mV at SL 0 (CS-60 p.38) | below the VCA's zero point; no audible spread at sustain 0 |
| WSC level | no per-board procedure in either manual | no data |
| 1.5 nF, 4.7 M, 180 pF | unmarked capacitors; "solid" 4.7 M resistors (SM1 parts list p.3) | no tolerance printed |

## Listening test

`/mnt/project-files/ceres/listening/2026-10-20-voice-variation-board-tolerance/`,
letters A-D, `key.md` alongside (revision 2: the whole set was re-rendered
after the sibling-evidence revision). A is the shipping engine (identical
boards). The candidates are:

- the full documented model;
- the untrimmed or check-band parts only (filter sensitivity and EG levels);
- the trimmed part only (VCO).

Material: factory `brass1`, seed 50, four-board unisons on MIDI 48 and 36
and a four-note chord; whole-file RMS matched. The default stays at
identical boards until the owner chooses.

## Verification

- `Ceres.board_tolerance_contracts` (Tests/BoardToleranceTests.cpp):
  - defaults keep identical boards;
  - every VCO trim point within +/-1 cent across 40 seeds x 4 boards, and the
    C2-C6 range within 3 cents;
  - untrimmed keys follow one linear VCO, rebuilt independently from the
    OCT-UP trim points;
  - static cutoffs identical, and each filter's control sensitivity (the
    cutoff octaves a BRILLIANCE step moves) within half a rank step,
    recomputed from both printed R7 columns and scaling with the amount;
  - one VCF EG level factor for both filters, within +/-10%;
  - a lone board's output scaled by one VCA EG factor within +/-2%;
  - the shared noise stream and start phases unchanged;
  - boards fixed across polyphony, reset and seed.
- `Ceres.board_tolerance_audio_parity`: native and embedded produce identical
  audio with all three tolerances at 1, and the tolerances change it.
- With all amounts at 0, the native parity driver (all seven modes) and the
  configuration parity driver are byte-identical to `0845146`.

## What would close the gap

- A numeric residual for the ear-matched static cutoff and Q trims, e.g.
  measured cutoff and Q of the four boards of one aligned CS-50.
- The panel-lever-to-Vfc law, so the sensitivity residual can also move the
  static cutoff away from the trim setting.
- EG time tolerances (attack trim band; decay/release spread of the .22 uF /
  IG00152/159 ranks), and VCA/WSC level tolerances.
- Measured per-board pitch error of a calibrated unit across the keyboard,
  to replace the uniform-in-band assumption, and a documented thermal drift.

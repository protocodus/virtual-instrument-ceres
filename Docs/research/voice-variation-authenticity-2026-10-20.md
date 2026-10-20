# Board-to-board variation: authenticity pass, 2026-10-20

The CS-50 has four separate M voice boards (SM2 printed pp.17-18, PDF 13-14,
"M1~M4"). Before this pass Ceres ran four bit-identical boards; the only
differences were the engineering controls `voice_spread_cents` (a fixed
symmetric spread) and `drift_cents` (a random walk), both VCO-only and
default 0. Neither is a tolerance model. (`drift_cents` now ships at 1: see
[Free-running drift](#free-running-drift-drift_cents).)

This pass adds the per-board variation the service documents bound, as
amounts from 0 (four identical boards) to 1 (the documented spread). The
owner chose the pitch, filter and envelope amounts by ear (verdict B on the
listening set below), so those three ship at 1; the others ship at 0 until
the owner has heard them:

| Key | Range | Default | 1 means |
| --- | --- | --- | --- |
| `board_pitch_tolerance` | 0..1 | 1 | the VCO trim residual the tuning method allows |
| `board_filter_tolerance` | 0..1 | 1 | the IG00156 rank-bin residual of each filter's control sensitivity |
| `board_envelope_tolerance` | 0..1 | 1 | the EG level checks: VCF EG +/-10%, VCA EG peak +/-2% and floor -200 +/-20 mV, with VR14's full-sustain trim after them |
| `board_envelope_time_tolerance` | 0..1 | 0 | both EGs' attack-time trims, +/-5% at 4 ms and 125 ms |
| `board_vca_tolerance` | 0..1 | 0 | the VR11/VR12 CP4 level trims, with VR13/VR14 and VR6 after them |
| `board_temperature_rise` | -25..25 K | 0 | the ambient change since tuning; each board's VCO drifts within the IG00153 temperature ranks |

Each seed is one instrument: the offsets are drawn once per physical board
slot, from two dedicated seeded streams (the first keeps the order of the
0.5.0 draws, so a seed keeps the boards it had), in a fixed order. The shared
audio and drift stream, the oscillator start phases and the noise sequence
are unchanged, and board *i* is the same unit at every polyphony and quality.
`reset()` keeps the same boards. With every amount at 0 each factor is
exactly the identical-board value, and the audio is byte-identical to 0.5.0;
the shipping defaults therefore change the sound of every patch, by the
amounts the boards' checks allow.

## Sources

| Document | Where | Used for |
| --- | --- | --- |
| Yamaha CS Series Servicing Guide | project sources (`servicing-guide.pdf`) | VCO law (printed pp.27-29), tuning method (p.65 / PDF 74) |
| CS-50 Service Manual 1/2, 2/2 | project sources (`sm1.pdf`, `sm2.pdf`) | M board parts and rank tables |
| Yamaha CS-60 Service Manual | project sources (`cs60-service-manual.pdf`, SHA-256 `4b03579a...106636`) | section VI alignment of the shared M board |
| Yamaha CS-80 "Adjustment Procedures and updates" | `https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF` (36-page scan, SHA-256 `eea101eb...276ba`) | the same trims with printed tolerances (printed pp.3-15 / PDF 5-17); Service News E-46 (PDF 33-36) |
| CS-80 parts list | `https://www.therogoffs.com/cs80/cs80_parts_list.pdf` (SHA-256 `eaea3c1b...aabe28`) | checked for capacitor tolerances: none printed |
| Soundgas CS-50 listing, "Engineer's report" | `https://soundgas.com/products/yamaha-cs-50` (page created 2024-02-13, published 2024-06-05; fetched 2026-10-20, SHA-256 `cb818990...badfb9`) | a restorer's service of one CS-50: qualitative only, no numbers |

None of these files is in the repository. CS-80 procedure pages are cited as
"printed p.N" (PDF page N+2).

## The boards are one family

The CS-50 guide prints a procedure and a tolerance for the VCO only. Two
sibling documents align the same M board:

- SM1 parts list p.2 (PDF 14): M circuit board NA04485, "Common Models:
  CS-60". The CS-60 SM's M schematic (printed p.15 / PDF 17) is headed
  "CS-60,50" with the CS-50's own transposition values and rank columns; its
  trimmer layout (printed p.37) is the CS-50 guide's p.65 board drawing.
- The CS-60 section VI text names its siblings where they differ: "2
  OCT-DOWN (16' for CS-80)", "NORMAL (8' for CS-80)" (VI.1-2, printed p.38 /
  PDF 40), "Alignment by 0.24V (CS-50 is 0.6V)" (VI.4, printed p.39 / PDF 41).
- SM2 printed p.16 lists the CS-80's M-board differences: the transposition
  circuit (R1-R5, VR4-VR6) and the rank resistor R7, nothing in the EGs or
  VCAs. Where the CS-80 procedure trims the same trimmer to the CS-60's
  target, it prints the tolerance the CS-60 leaves out.

Every value below from the CS-60 or CS-80 documents is labelled sibling
evidence in the code.

## VCO: trimmed, residual +/-1 cent at each trim point

- IG00153 is a **linear** converter: the one-shot/integrator gives
  `f = I/(Iref*tau)` with `I = Kv/Rn` (Guide printed pp.27-29 / PDF 36-38).
  The NORMAL table maps Kv 250 mV, 500 mV, 1 V, 2 V, 4 V to 130.8, 261.6,
  523.2, 1046, 2093 Hz (p.29), so Kv is proportional to the NORMAL frequency
  and the C2 key's Kv is 1/16 of the C6 key's.
- Tuning method (Guide printed p.65 / PDF 74), per board, after "10 minutes
  or longer" of warm-up: at OCT-UP, C6 key to C7 (4186 Hz) and C2 key to C3
  (261.6 Hz), each **+/-1 cent**, repeating (VR2 offset, VR3 scale). Then the
  C6 key at NORMAL to 2093 Hz (VR4), 1 OCT DOWN to 1046 Hz (VR5) and 2 OCT
  DOWN to 523.2 Hz (VR6), each **+/-1 cent**. The CS-80 procedure trims its
  VCOs to the same "Within +/-1 cent" (printed p.5).
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
positions). Assumption: uniform within the printed band (the guide prints
the band, not how technicians land inside it).

## VCO thermal drift: Service News E-46

Yamaha Service News bulletin E-46 (January 1978, "CS-50 / CS-60 / CS-80",
"Countermeasures for stabilizing the pitch in combo synthesizer series",
PDF pp.33-36 of the CS-80 procedure scan):

- "The IC IG00153 (VCOIII) on each M circuit board (VCO circuit) is ranked
  by temperature characteristics", by "the degree of change in the musical
  interval on C1 and C6 affected by the ambient temperature change from 25
  to 50 C", one figure per 5 cents: C1 0-45 cents (figures 1-9), C6 0-30
  cents (1-6). Example: 12 and 6 cents rank "32" (the bulletin prints the
  units as degrees, a typo).
- "Only the same figure representing the interval change on C6 can be used.
  Only the neighboring figure representing the interval change on C1 can be
  used in pairs at the same time for an instrument" (e.g. "54" with "64").
  So over that 25 K step one instrument's boards differ by less than 5 cents
  at C6 and less than 10 at C1.
- The 1S1555 temperature-compensating diode is moved onto IC8 with silicone
  grease. It applies to CS-50 #1546 on (November 1977) and to the earlier
  serials it lists.
- C1 and C6 are Kv 125 mV and 4 V: the CS-80 procedure sets the C1 key's Kv
  to 125.0 mV against a 4.000 V reference (TUNING, printed p.3), and the
  CS-50's C6 key is 4 V (Guide p.29). C1 is x = 1/32.

Model (`board_temperature_rise`, kelvin). Each board sits uniformly in its
window about the window's centre, C6 +/-2.5 and C1 +/-5 cents per 25 K, and
between them it drifts as one linear VCO, in the trims' gain-and-offset form
with the ratio 32 for 16. The drift the boards share (the windows' centres,
up to tens of cents) moves the instrument, not a board, and is left out.
Conventions, labelled in the code: uniform in the window; linear in the
temperature change (E-46 gives only the 25 -> 50 C step); the same at every
transposition (the IC is ranked, not the Rn resistors). 0 K is the
temperature the boards were tuned at.

## Free-running drift (`drift_cents`)

`drift_cents` predates this pass. Each board's pitch moves toward a new
random target, uniform within +/-`drift_cents`, every 0.2-0.7 s (0.1-0.6 s
for the first), followed with a 0.3 s time constant. Measured at 1 cent
(seed 50, four boards holding middle C for 600 s): 0.40-0.41 cents RMS per
board, peaks 0.91-0.99 cents, 0.59 cents RMS between two boards, the
autocorrelation down to 1/e in 0.51-0.56 s, about 1 cent per second of
average slew.

**Default 1 cent: an owner-requested engineering value, not a
measurement.** The owner asked for a non-zero default. The bound comes from
the tuning method: each warmed-up board is set and checked within +/-1 cent
at every trim point (Guide printed p.65, after "10 minutes or longer";
CS-80 procedure printed p.3, "Unit should be allowed to warm up at least 30
minutes before any final tuning adjustments are performed", and +/-1 cent
on printed p.5). A board whose wander left that band could not be set and
verified to it, so 1 cent peak is the most the tolerance leaves room for,
and the model never exceeds it. Where inside the band a real board's wander
lies is unknown: no measurement of IG00153 or CS-50/60/80 pitch against
time was found. E-46 gives temperature coefficients only, the service
documents warm-up times and tolerances; owner and dealer accounts online
describe temperature-sensitive tuning and long warm-ups without numbers.
A restorer's service of one CS-50 (Soundgas engineer's report, 2024) did
"Remake the thermal bonding on all 4 x VCO III devices" and "Replace
temperature compensation diode on card M3": E-46's thermal coupling again,
with no pitch figures.

**Time behaviour: it does not match the documented mechanism, and is not
changed.** The documented instability is thermal. E-46 is titled
"Countermeasures for stabilizing the pitch", and both its remedies are
thermal: rank each IC by its temperature coefficient, and grease the
compensating diode onto the IC so the two share one temperature. The
instrument settles over the 10-30 minute warm-ups the procedures ask for,
so its drift moves on a scale of minutes, set by the interior temperature
and that IC/diode coupling. The engine's wander decorrelates in half a
second, about three orders of magnitude faster, and no source describes
pitch movement on that scale. A slower model would need a temperature
course (amplitude and time constants) for a warmed-up instrument, which no
source gives, so the wander keeps its engineering time constants, labelled
as such in the code.

What would match: each board's pitch following E-46's coefficients times
its own temperature excursion, `board_temperature_rise` driven by a slowly
varying temperature, the shared part included (E-46's absolute figures put
up to 30 cents at C6 and 45 at C1 per 25 K on every board). The missing
data is the temperature, or directly the pitch, of a warmed-up CS-50 over
an hour: a frequency-counter log of one or more boards with the room
temperature alongside would give the amplitude and the time constants, and
would show whether anything moves on the sub-second scale at all.

## VCF: sensitivity binned by chip rank; cutoff and Q trimmed by ear

The "CS-60,50" schematic puts the rank resistors not on the IG00156 fadj pin
(a fixed 220k) but on each filter's **Vfc node** (R6 on IC10's f1 node, R7
on IC11's f2 node, each with 0.01 uF). The panel lever, the VCF EG buffer
(100k HP, 47k LP), SUB/touch via R7, and the VR7 (HP) / VR9 (LP) B-100k
offset trims through 150k all sum there. R6/R7 (~2.2k) are small beside
those sources (60k or more in parallel), so the node voltage per input
scales with R6/R7: the rank sets the chip's control **sensitivity**.

- Ranks (SM2 printed pp.17-18 note 9; CS-60 p.15): R6 2.7k/2.2k/1.8k, R7
  3.3k/2.7k/2.2k (CS-60/50 column 3.0k/2.4k/2.0k). Every column spans x1.5
  over two steps, a step of sqrt(1.5) = 1.2247.
- A chip lands within half a step: Vfc octaves scaled by 2^(u*0.14624),
  |u| <= 1, HP and LP independent.
- Alignment (CS-60 p.38, III-IV): "adjust VR7 for cut off frequency and VR8
  for Q so that the tone color of the M circuit board may be the same as
  that of other M circuit boards"; for the LPF, VR9 likewise. Matched by ear.

Static cutoff and Q stay identical. No printed number bounds the ear match:
the trimmers' range bounds what can be corrected, not what is left. The CS-80
does print numbers (its VCF alignment, printed p.11 / PDF 13, holds FH and FL
at 4.0 and 3.3 V, turns VR7/VR9 for maximum amplitude at the C1 key and sets
VR8/VR10 to 4.3 and 2.2 +/-1 Vpp at CP3), but that is a different procedure
on a CS-80 R7 and bus, so its bands do not carry over to a CS-50 aligned by
ear. A restorer servicing one CS-50 found its "Filters well matched"
(Soundgas engineer's report): consistent with a small ear-matched residual,
but not a number.

Strictly, the static cutoff away from the trim setting would also move by
(sensitivity - 1) times its distance from the trim point in volts. Ceres's
static cutoff is defined in Hz at C4, not in panel volts, so that term is
left out and listed as a gap.

## EG output levels

- VCF EG (CS-60 p.37, II): with AL, IL at 10 V, TP1 must show +5 V +/-0.5 V
  and -4 V +/-0.5 V; only the offset is trimmed (VR18, 0 +/-0.01 V, also
  CS-80 printed p.7). One buffer gain must meet both bands, so the VCF EG
  reaches both filters within **+/-10%**, one gain for HP and LP.
- VCA EG (CS-60 p.38, V; CS-80 printed p.13): the attack ends where the
  sequencer's comparator trips, TP2 **10 V +/-0.2 V**: a peak of each
  board's own. The held and released levels sit VR17's offset below the
  panel's, trimmed to **-200 +/-20 mV** at SL 0.
- VR14 is trimmed after both, with A, D, R short and S at 10 (CS-60 VI.4,
  "CS-50 is 0.6V"): every board gives the nominal output at full sustain.

Correction to 0.5.0. The 0.5.0 model scaled each board's whole VCA EG by its
+/-2% peak residual. That ignored the trim order: VR14 comes last and is set
at full sustain, so a full-sustain note is back at the nominal level. What
remains is the attack peak relative to the held level, and the floor's
residual, which shows below full sustain: at SUSTAIN 3 a board sits within
+/-0.5% of nominal, at full sustain on it.

## EG times

Each EG is one exponential converter: TC reaches the IG00151's EI through
82k + VR19 (VCF) or VR20 (VCA), with the VR15/VR16 offsets through 100k (SM2
p.16). The CS-60 trims them to an attack of 4 ms at 8 V and 125 ms at 3 V
(pp.37-38). The CS-80 trims the same VR15/VR19 (VCF EG, printed p.7 / PDF 9)
and VR16/VR20 (VCA EG, printed p.13 / PDF 15) to the same two times and
prints **"(+/-5%)"** for each.

Model (`board_envelope_time_tolerance`). Each trim point is uniform in
+/-5%, per EG and board. An exponential converter's log-time error is linear
in its control voltage, so between and beyond the trim points it is linear
in log time. Decay and release drive the same converter
(Docs/research/envelopes-authenticity-2026-10-20.md); preset()'s factory
conversion makes an attack 5 RC and a D/R time a T60 of ln(1000) RC, so a
T60 shares the RC of an attack of 5*T60/ln(1000), and takes that attack's
error.

## VCA levels and the 0 dBm trim

- VR11 trims the sine to 1 Vpp and VR12 the filtered pulse to 800 mVpp at
  CP4 (CS-60 VI.1-2); the CS-80 trims the same VR11 and VR12 to **1.00
  +/-.02 Vpp** and **800 +/-20 mVpp** (printed p.15 / PDF 17).
- VR13 (CP5) and VR14 (O) act after CP4. The CS-60/50 trims them on the
  filtered pulse ("With the above condition", VI.3-4), restoring its level,
  so a board's sine keeps `s = (1+e11)/(1+e12)` of its level. (The CS-80
  trims its VR13/VR14 on the sine instead, to its own 400 and 80 mVpp: the
  CS-50 follows the CS-60 here.)
- PRA III (CS-60 printed p.39): VR6 is trimmed last, in the finished
  instrument, with the sine, SUSTAIN and LEVEL at maximum and all keys down,
  for 0 dBm at OUT. It takes the four boards' sine error out of the common
  gain: with the engine reference's assumptions (RMS meter, notes adding
  incoherently), `V = sqrt(4 / sum s^2)` over the four physical boards.
  VR6's own band (0 +/-1 dBm) is an instrument's level, not a board
  difference, and is not modelled.
- The same restorer's report: "VCA levels trimmed so that the amplitude of
  all M cards the same". Board-level VCA differences are what a service
  trims out; the report gives no residual.

## Not modelled: no printed tolerance

| Mechanism | What the documents give | Why left out |
| --- | --- | --- |
| Static cutoff and Q | VR7, VR9 (cutoff), VR8, VR10 (Q) matched by ear (CS-60 p.38) | no numeric residual for the CS-50 procedure |
| VR14's own residual | 0.6 V on the CS-50 (CS-60 VI.4), no band | the CS-80's 80 +/-5 mVpp is a different target on the sine |
| VR6's own residual | 0 +/-1 dBm (CS-60 PRA III) | an instrument level, not a board difference |
| WSC level | no per-board procedure in either manual | no data |
| 1.5 nF, 4.7 M, 180 pF, .22 uF | no tolerance in the CS-50 or CS-80 parts lists; the CS-80 M-board photo is too coarse to read markings | no data |
| KAS sample-holds | no per-channel procedure | no data |

## Listening tests

`/mnt/project-files/ceres/listening/2026-10-20-voice-variation-board-tolerance/`,
letters A-D with `key.md` (revision 2): A the then-shipping engine
(identical boards) against the 0.5.0 tolerance model and its parts.
Material: factory `brass1`, seed 50, four-board unisons on MIDI 48 and 36
and a four-note chord; whole-file RMS matched.

Verdict, by ear (2026-10-20): B, the pitch, filter and envelope amounts
together at 1. They ship at 1. The set predates the VCA EG correction
above: its B scaled the whole VCA EG by the +/-2% check band, where the
shipping envelope amount follows the documented VR14 trim at full sustain.
That correction is the procedure's, not a listening choice; the next set
plays it as its A.

`/mnt/project-files/ceres/listening/2026-10-20-voice-variation-eg-time-vca/`,
letters A-D with `key.md`: A the shipping engine (verdict B, `drift_cents`
1) against the envelope-time and VCA amounts, alone and together. Material:
factory `electric_piano` (pulse and sine, long decays), seed 50, the same
unisons and chord; whole-file RMS matched. It decides whether those two
amounts ship. `board_temperature_rise` is not a candidate: its default would
be an invented temperature.

## Verification

- `Ceres.board_tolerance_contracts` (Tests/BoardToleranceTests.cpp), every
  expected value rebuilt from the printed numbers:
  - the defaults ship pitch, filter and envelope at 1 and the rest at 0,
    the factory presets keep them, the shipping boards differ, and zero
    amounts give identical boards;
  - every VCO trim point within +/-1 cent across 40 seeds x 4 boards, the
    C2-C6 range within 3 cents, and untrimmed keys on one linear VCO;
  - thermal drift at 25 K within +/-2.5 cents at C6 and +/-5 at C1, one
    instrument's spread under 5 and 10 cents, one linear VCO's gain and
    offset drift between and above (rebuilt from C1 and C6), linear in the
    temperature change, independent of TRANSPOSE and additive to the trim;
  - static cutoffs identical, and each filter's control sensitivity within
    half a rank step;
  - one VCF EG level factor for both filters, within +/-10%;
  - VCA EG peak within 10 +/-0.2 V and floor within -200 +/-20 mV; a lone
    board's held output, once the attack has left the output couplings, at
    the VR14-predicted gain at SUSTAIN 10 (1) and 3 (from its own floor);
  - attack times within +/-5% at 4 ms and 125 ms for both EGs, a 1 s attack
    and the decay and release on the log-linear converter line through
    them, measured from the EG steps;
  - each board's sine balance within the VR11/VR12 bands, the filtered path
    changed only by the common VR6 gain, and `V^2 sum s^2 = 4`;
  - the 0 dBm reference within the output-stage contract's 0.1 dB with every
    tolerance at full spread (measured -0.005..+0.003 dB for seeds 3, 8, 21,
    against -0.011..+0.003 dB with identical boards);
  - the shared noise stream and start phases unchanged; boards fixed across
    polyphony, reset and seed;
  - `drift_cents` ships at 1, factory presets included; over 30 s each of
    four boards on one key stays within +/-1 cent and moves (peak over 0.5,
    RMS over 0.2 cents), two boards wander independently, and 2 cents
    doubles every excursion exactly. The time course is not checked: it has
    no source.
- `Ceres.board_tolerance_audio_parity`: native and embedded produce identical
  audio with every tolerance at full spread, and the tolerances change it
  (against the driver's `identical-boards` mode, the same panel controls
  with the three shipping amounts at 0). Every other parity driver mode now
  plays the shipping boards, and native and embedded still match exactly.
- `Ceres.output_stage_contracts` runs on the shipping boards unchanged. VR6
  is trimmed last, on the instrument's own four boards (CS-60 PRA III), but
  by then VR14 has put each board's full-sustain output back on nominal and
  the VCA amount is 0, so VR6 sees the nominal four-sine sum and keeps its
  nominal setting; the pitch residual only detunes the incoherent sum.
  Measured reference error, seeds 3, 8, 21, 50, 77, 101: -0.010..+0.014 dB
  (identical boards -0.018..+0.006 dB), inside the contract's 0.1 dB.
- Contracts that read a nominal design law back from the engine (the VCO
  key law and clock, the printed filter node ratios and SUB depth, EG
  octaves per volt, touch and vibrato depths) set the three shipping
  amounts to 0, and those that read a pitch `drift_cents` too, each with a
  comment; the board contracts above own the tolerance and the drift
  bound. The idle VCA-EG floor contract checks each shipping
  board's own floor (-200 +/-20 mV) and the exact -200 mV of identical
  boards.
- With every board amount and `drift_cents` at 0, the native parity driver
  (all seven modes; it sets its own `drift_cents`), the configuration parity
  driver and `ceres_render` of all 14 presets are byte-identical to 0.5.0
  (`0f1d00f`), checked again after the default changes.

## What would close the gap

- A numeric residual for the ear-matched static cutoff and Q trims, e.g.
  measured cutoff and Q of the four boards of one aligned CS-50.
- The panel-lever-to-Vfc law, so the sensitivity residual can also move the
  static cutoff away from the trim setting.
- Decay/release spread of the .22 uF capacitors and IG00152/159 ranks beyond
  the converter's trim line, and a WSC level tolerance.
- Measured per-board pitch error of a calibrated unit across the keyboard,
  to replace the uniform-in-band assumption, and the temperature course of
  a board between the 25 and 50 C points E-46 gives.
- A pitch-against-time log of a warmed-up CS-50 (frequency counter on one
  or more boards for an hour, room temperature alongside), to give
  `drift_cents` a measured depth and time course.

# Circuit realism, 2026-10-28

The owner asked for five grounded improvements that make Ceres more analog and
authentic through circuit modelling. The engine already models almost every
documented CS-50 stage, so this round looked for places where the engine
itself computed an effect and then left it out, where its behaviour
contradicted a printed diagram, or where two printed sources disagree. Four
changes ship in 1.1.0; the fifth is a disagreement between two printed sources
that the owner decides by ear.

Each change is the commit named beside it on `claude/project-thread-f75gcl`,
and each has a before/after pair rendered from that commit and its parent in
[the previews](../audio/previews/2026-10-28-circuit-realism/README.md).

## Sources

| Short name | Document | Pages used |
| --- | --- | --- |
| Guide | Yamaha CS Series Servicing Guide | printed 16, 19 (KAS timing chart), 25 (gate/integrator regimes), 56 (EXT preamp) |
| SM1, SM2 | Yamaha CS-50 Service Manual, parts 1 and 2 | SM1 printed 5 (YM26700), 14 (R7); SM2 printed 3, 7 (KAS), 8 (SUB), 11 (T71), 15–16 (R7, M board), 22 (PRA), 26 (PN2 wiring), 27 (PN5) |
| IC Guide | Yamaha IC guide, YM26600 block diagram | PDF 16 ("Old Key OFF CH Detector") |
| CS-60 SM | Yamaha CS-60 Service Manual (S/# 1001~) | printed 6 (power-on key), 35 (KAS adjustments I–V) |
| NJM4558 | New JRC NJM4558 datasheet | typical slew rate, gain-bandwidth and output swing |

## 1. The key assigner frees the board released longest ago (`3319311`)

**Circuit.** The KAS sees keys only: no wire brings an envelope back to it
(SM2 printed pp.3, 7). The YM26600's block diagram names an "Old Key OFF CH
Detector" (IC Guide PDF 16), and the Guide's p.19 timing chart gives the next
key to the channel released first.

**Before.** A new key went to an idle board first, then to the board whose
VCA envelope was quietest. That needs an envelope reading the KAS does not
have.

**Now.** The board whose key went up longest ago takes the key; a board still
sounding its release is as free as a silent one. Notes of even length and
release pick the same board either way. The difference shows when a long note
is released before short ones: its loud tail used to keep ringing while a
quieter, younger tail was cut; now the older one is cut, as the hardware's
detector does.

**Evidence.** Printed (block diagram and timing chart). A fixed channel
preference would fit the chart too, but it would need no such detector.

## 2. The ring MIX lever's sag follows the expression pedal (`3a7cdb6`)

**Circuit.** The MODULATION lever (TVR5, B-10k) is the dry/wet crossfader, and
its wiper MI is the expression network's input (SM2 printed p.26 wiring, p.22
for what MI drives). With the pedal open, the photo-coupler cell ties MI to the
VOLUME track, loading the wiper with 8.04k: the mix sits 2.35 dB low at
mid-lever.

**Before.** The engine computed that load and noted that a closing pedal
raises it, then kept the open-pedal load at every pedal position.

**Now.** MI's load follows the pedal's own cell (the network that already sets
the pedal's printed level curve). As the pedal closes the load rises toward
67k and the taper straightens: at a −16.5 dB pedal the mid-lever sag is
0.64 dB. Both lever ends are unchanged, and an open pedal (`expression` 1) is
exactly as before. At `expression` 0.7 and mid-lever the mix is 1.1 dB louder.

**Evidence.** Printed wiring and component values; the cell's resistance law is
the one the engine already fits to the printed pedal curve.

## 3. The EXT preamp slews at its op-amp's 1 V/µs (`481cab8`)

**Circuit.** EXT IN reaches a ×101 non-inverting NJM4558 stage (SM2 printed
p.22; Guide printed p.56). At that gain almost any line signal drives it into
its ±14 V swing, and the NJM4558's typical slew rate of 1 V/µs takes 28 µs to
cross it.

**Before.** The clipped preamp jumped rail to rail within one internal step.

**Now.** The preamp output ramps at 1 V/µs, so an overdriven square reaches the
SUB input as a trapezoid. A signal the slew rate does not limit passes bit for
bit as before. With a 0.3 V, 110 Hz square at EXT IN modulating the filter,
100 ms windows change by up to 0.8 dB and the difference sits 22.6 dB below
the signal; at 0.075 V the output is unchanged.

**Evidence.** Datasheet typical (an estimate, like the noise limiter's slew).
The 3 MHz gain-bandwidth, a 29.7 kHz closed-loop corner, is left out: under
0.5 dB at 10 kHz.

## 4. The boards share their cutoff rows (`32c7116`)

**Circuit.** R7 takes each cutoff row, FH and FL, through four 39k, one per
board, to a pin-4 node the M board holds near ground (SM1 printed p.14; SM2
printed pp.11, 15–16). A board's VCF EG, touch, SUB and BRILLIANCE lift that
node, so its 39k draws less from the row, the row rises, and every board's
cutoff follows.

**Before.** The engine computed this coupling in a comment ("at most .03 octave
per board at +5V LP EG on a weak column, .1 with all four") and left it out,
so each board's filter ignored the others.

**Now.** Each sample, the rows solve V + Vd(V/rl − J) = S behind the PANEL
lever's follower and diode (`CutoffBus`), with J the current the four physical
boards' lifts return to the row. Idle boards rest at their EG's initial level,
at any voice count; further voices follow the rows without loading them. Near
the lever's floor four boards at +5 V of LP EG lift each other 0.10 octave;
with the lever at 1 V the diode holds the row to 0.009 octave. With nothing
lifting the rows, the cutoffs are exactly the patch's. On the factory
presets the change sits at least 62 dB below the signal (a four-note chord).

**Evidence.** Printed network. Two estimates remain, as before: the 1S1555's
forward curve (modelled as a 1N4148-class diode) and pin 4 as a high-impedance
input. A patch does not say whether a tablet set its cutoff, and a tablet's
column couples more (0.07–0.09 octave with four boards on the weaker columns);
the rows are solved for the PANEL lever (README, Known gaps).

`Ceres.sound_path_circuit_contracts` solves the printed network independently
(bisection over the diode law) and checks the reported rise and every board's
cutoff against it. Three older contracts compared a modulation (key CV, an EG
offset, master PITCH) with a static cutoff. Above the lever's 0 V point a static
cutoff also biases the row's diode, which those modulations do not, so the
identities moved below it, where the rows are linear and they still hold
exactly.

## 5. The portamento clock: two printed sources disagree (listening test)

The Guide prints the KAS gate/integrator's three regimes against the
PORTAMENTO lever voltage (printed p.25: 108.9 ms, 13.6 ms, and a direct gate
that passes the counter's semitone steps). Which regime a `glide_rate` falls in
depends on the counter clock at each lever voltage, and the two sources that
give that clock disagree: the CS-50's SM1 printed p.5 gives YM26700 pin 4 as
11 s at −6.5 V to 1.4 ms at +8.5 V, and the CS-60's KAS adjustment IV (printed
p.35) trims IC24 pin 6 to 6 Hz at LONG and 6 kHz at SHORT. Their LONG/SHORT
ratios differ (about 7860 against 1000), so they cannot both describe the same
node. The CS-50's document is not silent here, so under the sibling-evidence
rule the CS-60 figure cannot decide alone. A lettered set, level-matched, is
with the owner; its key stays with the set until the verdict, which goes to
[the decision log](../decisions.md). Nothing ships for it until then.

## Candidates considered and set aside

| Candidate | Why not |
| --- | --- |
| LFO board tolerance | It only offsets one shared oscillator's rate, and would need a new control |
| Release diode-OR into the EGs | The CS-50's SUS source cannot be verified from its documents |
| SUB loading the VCF's R6/R7 | Under 1 % |
| Q-bus coupling between boards | 0.12 % |
| KAS sample-hold droop and charge injection | The sign is unknown |
| KAS scan window (the CS-60 YM26600's 94 kHz clock) | Defines a "simultaneous keys" window; inaudible |
| MI's frequency-dependent load | Its corner is at 13 Hz; inaudible |
| EXT preamp gain-bandwidth | Under 0.5 dB at 10 kHz |
| WSC SIO and IG00150 0.047 µF couplings | The driving output impedances are not printed |
| Expression photo-cell lag | No values are printed |

## Findings that change nothing

- **LI is a current input.** On the CS-60 M board the VCA's LI pins are fed only
  through 560k from +15 V, and the SUB IG00151s through 100k from trimmers. That
  supports leaving out the RC a voltage input would form with the LEVEL lever's
  100k/0.01 µF.
- **The power-on key.** The CS-60 SM (printed p.6) powers up on C#4, the Guide
  (printed p.17) on C4. The engine keeps the Guide's C4.

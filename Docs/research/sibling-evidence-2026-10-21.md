# Sibling evidence for the open gaps, 2026-10-21

The 2026-10-20 scorecard listed, for each component, the data that would take
it to 9. No CS-50 document or measured unit supplies any of it, so this round
searched the CS-80, CS-60 and CS-30 service documents, which share the CS-50's
custom chips and, in places, its boards. The owner allowed that ("you can also
explore sister synths (cs60 etc) if original unit is unavailable"). Everything
taken from them is labelled sibling evidence in the code and documents.

## Sources

| Short name | Document | Pages used |
| --- | --- | --- |
| CS-80 Adj | Yamaha CS-80 Adjustment Procedures and updates, [therogoffs.com](https://www.therogoffs.com/cs80/CS80%20Adjustment%20Procedures%20and%20updates.PDF) (36-page scan) | printed 7 (VCF EG), 9 and 11 (VCF), 13 (VCA EG), 15 (VCA), 19 (RING MODULATOR), 29 (OUTPUT AMPLIFIER) |
| CS-80 SM | Yamaha CS-80 Service Manual (S/#1001~), [page scans](https://therogoffs.com/cs80/manuals/CS80_Service_Manual/) | 41 (M circuit diagram), 42 and 52 (M circuit ICs), 53 (PRA circuit diagram), 54 (PRA board) |
| CS-60 SM | Yamaha CS-60 Service Manual | printed 38–39 (sections VI.1–VI.5 and PRA I–III), as in the earlier notes |
| CS-30 SM | Yamaha CS-30 Service Manual, [emusic-diy.org](https://emusic-diy.org/_media/yamahamanuals:cs30_service_manual.pdf) | IG00152, IG00156, IG00158 and IG00159 pin sheets |

Secondary sources, used only to corroborate: a Sound On Sound review of a
CS-80 beside Deckard's Dream
([soundonsound.com](https://www.soundonsound.com/reviews/black-corporation-deckards-dream)),
Soundgas's service notes on a CS-50
([soundgas.com](https://soundgas.com/product/yamaha-cs-50)), and a VCV Rack
forum post quoting the ModWiggler "Yamaha CS Filter Analysis" thread
([community.vcvrack.com](https://community.vcvrack.com/t/creating-the-cs-80-sound/17682/66);
the [thread](https://modwiggler.com/forum/viewtopic.php?t=216820) and its PDF
could not be read).

## Applied in Ceres 0.6.2

1. **Ring envelope alignment voltage.** CS-80 Adj p.19 trims VR10 for 31 ms
   ±5 % from key-on to the EO peak with ATTACK TIME at +5.0 V on the A
   terminal. CS-80 SM p.53 draws the same ring envelope as the CS-50 PRA
   (IG00159, IG00151 into 0.22 µF, TC and the VR10 trim each through 100k
   into EI, 2DT on 6.8k/3.3k). The CS-60's "Alignment by 31mS" names no
   voltage; 0.6.0 read it at 10 V. At 5 V the trim leaves the PRA envelope
   on the M-board pin-sheet law within 2 %, and the fixed release becomes
   0.459 s T60 instead of 14.50 s. This reverses a by-ear choice; see
   [the decision log](../decisions.md) and the
   [ring addendum](ring-authenticity-2026-10-20.md#addendum-2026-10-21-the-alignment-voltage).
2. **Output meter load.** CS-80 Adj p.29 reads the output jack on an AC
   voltmeter into 10 kΩ, 0.78 V RMS (0 ± 3 dB). HO now drives 10 kΩ through
   its 560 Ω: the rails sit 0.47 dB lower and the final coupling corner moves
   from 0.92 Hz to 1.37 Hz. The 600 Ω reading is excluded; see the
   [VCA addendum](vca-authenticity-2026-10-20.md#addendum-2026-10-21-the-meters-load).

## Corroborated, no change

- **Ring carrier law.** CS-80 Adj p.19 step 3: 150 ms ±5 % at VC1 = +5.0 V,
  5 ms ±5 % at SPEED max, the CS-60's PRA II values behind `ringSpeedHz`.
- **EG time trims.** CS-80 Adj pp.7 and 13 trim both voice EGs to 4 ms ±5 %
  at 8 V and 125 ms ±5 % at 3 V on the attack terminal, as the CS-60 M-board
  steps do, and set the VCA EG's TP2 floor to −200 ± 20 mV. These are the
  bands behind `board_envelope_time_tolerance` and the amplitude envelope's
  floor.
- **EG chip ranges.** CS-80 SM pp.42 and 52 and the CS-30 pin sheets give
  the IG00152 and IG00159 0–10 V inputs as attack 1 ms–1 s and decay and
  release 10 ms–10 s, the pin-sheet law the engine uses.
- **VCF anchor.** CS-80 SM p.42 and the CS-30 IG00156 sheet print 1 kHz at
  KV = 0.25 V and Vf = 5 V, which is the guide's 125·KV·2^Vfc exactly, and
  CS-80 SM p.42 gives Q = 5 at VQ = 0 V and 0.5 at 10 V, as SM1 does. The CS-80's complete VCF alignment (Adj
  p.11) ties cutoff to one lever voltage per filter, but its KC input and
  FEET setting do not map onto the CS-50 without an assumption, so it is not
  used. Its HP point agrees with the printed law within 0.065 octave if KC
  is taken as 0 V.
- **VCF slope.** The CS-30's VCF alignment (p.28, steps 1a–1c) trims the
  low-pass peak onto C4 at 4' (1053 Hz) with CUT OFF FREQ at +5 V and
  500 mV on pin 2, then expects it on B5 (3978 Hz) at +7 ± 0.1 V: 0.96
  octave per volt, and the printed one octave per volt lands inside that
  tolerance (6.92 V). Its lever reaches pin 4 through 39k into a rank
  resistor of the values the CS-50/60 M board uses (3.0k, 2.4k or 2.0k), as
  the CS-50's cutoff buses do through R7's 39k branches; the other branches
  on that node differ, so the scale per volt is close, not identical. This
  supports the printed law Ceres chose by ear in 0.6.0. The absolute point
  is set by the CS-30's own offset trim (VR2), so it does not calibrate the
  CS-50's offset, and the step's B2 check cannot be reached within KBD
  FOLLOW's range, a misprint.
- **VCO sine and LFO sine.** CS-80 SM p.42 and the CS-30 sheet draw the
  IG00158 sine (SIO 3 Vpp) shaped from its own 3.2 Vpp triangle, and CS-80
  SM p.54 gives the IG00150 SIO as 3.20 Vpp. Neither prints a transfer curve
  or harmonic content, so the VCO shaper stays unmeasured. Soundgas traced a
  CS-50's missing PWM to a dead SIO pin, consistent with PWM taking the
  IG00150 sine as Ceres does.
- **Filter floor.** The ModWiggler analysis, as quoted, has a fixed 23 Hz
  term in the low-pass cutoff (its w "should be w − w0, where w0 is 23 Hz");
  Ceres's fixed 4.7 MΩ feed gives a 22.58 Hz floor. Its frequency-dependent
  Q, gain 1.7 and 7.6 kHz pole are not used: they are secondhand, the
  derivation could not be read, and with no resonance control its Q is 14.4
  at low frequency, against the printed maximum of 5.
- **Release longer than decay.** The Sound On Sound reviewer's CS-80 reaches
  about 7 s decay and 12 s release at the slow end, a ratio of 1.71. The
  CS-50's release follower drop gives about 1.7× (envelope research §2). The
  CS-80's release path was not checked, so this is consistency, not evidence.
- **Board matching.** Soundgas found a serviced CS-50's four filters matched
  and had to trim its VCA levels, consistent with the default board
  tolerances, not a measurement of their size.

## Still open

| Gap | What the siblings give |
| --- | --- |
| VR4, the ring wet trim | Nothing: the CS-80's VR4 is the carrier speed trim, a different function. |
| IG00153 pin-15 impedance, IG00150 and IG00158 sine shape | Levels only (3.2 and 3 Vpp); no impedance or harmonic data. |
| IG00151 above 10 V LI | The 10–30 kΩ output check load and the current-output reading; no transfer above 10 V. |
| IG00156 input scale, saturation and pin-7 current | Gotharman's module calls the IG00156 input "very sensitive", which says it clips early but gives no level. No pin-7 current is printed, but the CS-30's board implies a low-impedance pin 7 ([addendum](#addendum-the-research-cross-check)). |
| Envelope scope captures, warm-up pitch log, measured voice spreads | None: the procedures trim to bands, and no capture of a unit was found. |

## Scores after this round

| Component | 2026-10-20 | Now | Why |
| --- | --- | --- | --- |
| SUB | 9 | 9 | unchanged |
| Ring | 8 | 9 | the alignment voltage is printed for the same circuit; the slope assumption now moves the release by 2.2 % at most |
| VCO | 8 | 8 | unchanged |
| VCA and output | 8 | 8.5 | the meter's load is printed; IG00151 above 10 V stays open |
| VCF | 8 | 8.5 | the CS-30 alignment supports the printed slope, and the CS-80's HPF point agrees within 0.065 octave if its KC input is taken as 0 V; input scale and saturation stay open |
| Envelopes | 8 | 8 | corroborated, no new data |
| Voice variation | 7 | 7 | unchanged |
| BRILLIANCE and RESONANCE | 7 | 7 | unchanged |
| Average | 7.9 | 8.1 | |

All three raised scores rest on sibling evidence, CS-80 and CS-30 documents
for circuits the CS-50 shares, not on a CS-50 measurement.

## Addendum: the research cross-check

A wider search (19 sources; 25 claims each put to three independent checks)
found no measured value for any row of **Still open**. It confirmed the
readings above, the CS-30 slope check included: the step on B5 at
+7 ± 0.1 V reads as quoted, and only an inference that the law is not
log-linear, which rested on the unreachable B2 step, was rejected. Three new
leads come from board wiring. None is implemented, because each needs a value
no document prints.

1. **IG00156 pin 7 may be a current input.** The CS-30 feeds the same chip's
   VQ pin through 22k into a rank shunt of 470, 430 or 390 Ω, with 10 µF
   (CS-30 SM pp.8–9), and prints Q = 10 at 0 V and 0.5 at 10 V. A
   high-impedance pin there would see at most 0.2 V and stay near maximum Q
   over the whole knob, while a pin that responds over 0.2 V would sit at
   minimum Q almost everywhere on the CS-50's 220k/220k network. Both boards
   work only if pin 7 takes current into a low impedance. Ceres takes pin 7 as
   drawing no current, which gives the RESONANCE controls their 1.1 s lag
   (10 µF × 110k) and loads each preset Q row with 110k to RES. With a current
   input the lag would be tens of milliseconds, and each Q row would see 55k
   to a node near 0 V instead. If both boards' 10 V endpoints mean the same
   pin current, the input is near 1.7 kΩ and the lag about 17 ms; that is an
   inference, not a datum. Measuring pin 7's voltage against the RES lever on
   a real M board would settle it.
2. **A capacitor on the WSC sine output.** One reading of the CS-80 M-board
   drawing (SM p.41) puts 0.047 µF from the IG00158 SIO to ground. Its corner
   depends on SIO's output impedance, which is not printed (about 34 kHz at
   100 Ω, 3.4 kHz at 1 kΩ). The CS-50's own drawing (SM2 p.16) should be
   checked first.
3. **The IG00150 sine output may be high-impedance.** The CS-80 PRA drives the
   µA796's 100 Ω-terminated carrier port straight from SIO (CS-80 SM p.53),
   which a stiff 3.2 Vpp source could not do. Ceres treats SIO as an ideal
   source into the CS-50's 100 µF and 33k; that holds only if SIO's impedance
   is well below 33k.

E-46's temperature ranks and the factory trim bands came up again; the drift
and board-tolerance models already use them.

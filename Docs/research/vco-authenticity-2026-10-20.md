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
  above A440. The engine's `tune_cents = 0` is A440. It is a whole-instrument
  reference choice (every preset, every host), and the owner kept A440
  (round 2, 2026-10-20); a default of +11.73 cents would reproduce the
  factory reference exactly.
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

Later the same day:

3. **The differential-pair sine shaper ships** (owner listening verdict B,
   round 3; see WSC sine below). The sine is `tanh(k*tri)/tanh(k)`,
   k = 1.4020, on the same fold and plateau, band-limited analytically. Its
   lower RMS at the same peak moves the four-sine 0 dBm output anchor and
   the output rails down 0.275 dB.
4. **Round 2 evidence beside the code, no behaviour change.** A CS-80 sample
   set measured by `Tools/measure_cs80_wsc_recordings.py` now stands beside
   the pulse polarity and the shaper constants, with a contract
   (`recordedEdgeRelationship` in `Tests/OscillatorCircuitTests.cpp`) that the
   rendered audio pulse falls with the saw's reset and that PWM moves only its
   rising edge, checked on the edges themselves.

## Saw/pulse relative polarity

The engine negates the pulse so equal-level saw and pulse reinforce. In
round 1 that rested on one public recording (Marc Doty, AutomaticGainsay,
2013; see `Docs/research/accuracy/waveform-mixing-20261019/`), where both-on/saw RMS
is 2.35-2.47 against 0.89 for the cancelling sign.

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
the code. Round 1 asked for a scope capture of IG00158 pins 5 and 7 (or
T2/T3) on one board, or a second recording.

### Round 2: a CS-80 sample set

**Recording.** Freesound user "mogigrumbles" sampled a Yamaha CS-80's raw
waveforms note by note ("I sampled all 61 notes for each of the CS80 tone
selector presets, and also the raw waveforms", uploaded 2020-07-07; packs SAW
29608, SQUARE 29624, PULSE 75 % 29623, PULSE 90 % 29613, PWM SLOW 29612,
PWM MED 29607, SINE 29632; CC BY-NC 3.0). The CS-80 uses the CS-50's M board
(#21233; SM2 p.16 tabulates only pitch-section differences), so its saw,
pulse and sine come from the same IG00158. Each take is peak-normalised and
sounds an octave below its MIDI note. The capture chain, panel settings and
voice allocation are not documented, and every pack is a separate take,
assumed to share one signal path. `Tools/measure_cs80_wsc_recordings.py`
holds the method and pins the 77 public previews it reads (MP3 and Ogg
Vorbis) by SHA-256; no audio is committed.

**Result.** On all 12 PWM notes (MIDI 48-69, both rates) the falling audio
pulse edge keeps a fixed period, with a period-to-period spread of 0.04-0.23
samples at 44.1 kHz, while the rising edge moves: 0.24-12.1 samples, 3.7 to
98 times the fixed edge's. On all 11 SAW notes (36-84) the first difference
is strongly negatively skewed (-5.1 to -22.6): a rising ramp with a falling
reset. The comparator's fixed edge is the ramp reset, so on this instrument
the pulse's fixed edge and the saw's reset both fall, and equal levels add,
as the shipped `-square` makes them and as the CS-50 recording found. The
relationship is relative, so it holds whatever the capture's sign, provided
the takes share it, and it does not depend on levels.

Further data, not used for the sign (audio high fraction, median over
periods, notes 36-84):

| Pack | High fraction |
| --- | --- |
| SQUARE | 0.510-0.537, falling with pitch |
| PULSE 75 % | 0.379-0.395 |
| PULSE 90 % | 0.117-0.148 |

The 90 % pack is mostly low, as the engine's audio pulse is at PW 90 %
(0.10). The 75 % pack reads 0.38-0.40, not 0.25, so the labels are not
calibrated lever positions. The SQUARE pack's logical duty of about 0.48
gives |cos(pi*0.483)| = -25.4 dB of H2, in line with the CS-50 square's
measured H2 of about -25 dB; whether that is a trim offset or a tolerance is
unknown, and it is data for the voice-variation work, not modelled here.

**Patent.** Nippon Gakki's own discrete saw-to-pulse converter (US 4,012,981,
Y. Nagahama, priority 1974-10-09; FIG. 2) switches NPN transistor Q1 and
buffers its collector with PNP transistor Q3: two inversions, so the output
is high while the input saw is above the PW level, a non-inverting
comparator. Fed T1, as the IG00158's PI is, that is exactly the pin sheet's
PO, which the inverted SO would partly cancel. The patent therefore supports
the pin sheet's comparator sense; with the recordings it puts the extra
inversion inside the IG00158 (an inverting PO output stage, or an SO that
does not invert as labelled). No document shows the chip's output stages.

**Status.** Two instruments' recordings now agree on constructive mixing,
the second through an edge relationship that depends neither on levels nor
on the capture's sign. The shipped sign stays, and the CS-80 relationship is
now a test contract. Read absolutely, a non-inverting capture makes the
pulse the guide's PWM sketch upside down, and an inverting one makes the saw
jump up at the reset, against SO's label: either way one drawing is off.
Where the inversion sits inside the chip is still unknown. It does not change
saw against pulse, but it sets that pair's sign against the sine, which
separate takes cannot show; a capture of IG00158 pins 2, 5 and 7 together
would.

## WSC sine

Until round 3 the engine's sine was `-cos(pi*p)` on the conditioned ramp,
which is exactly the fold `tri = 2|p|-1` followed by an ideal `sin(pi/2*tri)`
shaper, so the plateau and fold match the pin sheet; only the shaper has
changed since. Two things remain:

- **Shaper transfer.** No transfer or transistor diagram of the IG00158 (or
  the CS-60/CS-80, which use the same chip) is published, and round 1 found
  no harmonic measurement. Searches found only Ando Pilve's 2026 clone
  announcement ("authentic waveform phases, wave folding methods"), with no
  data. The textbook bipolar
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
  still not applied and is noted in the code. **Applied 2026-10-22 (Ceres
  0.6.5):** the coupling is the M board's own (SM2 p.16), so it now sits in
  front of the chosen shaper as an RC high-pass, 103 ms, on the held
  triangle; the 22k biases STI, so the pair's base is taken as a high
  impedance. Its capacitor starts precharged to the mean at the board's
  first pitch and re-centres STI after every pitch change. Harmonics re H1
  of the continuous waveform with the coupling settled, before and after:

  | Note | Mean | H2 dB | H3 dB | H4 dB |
  | --- | --- | --- | --- | --- |
  | C4 on 8', 523 Hz | 0.2 % | −56.4 → −51.5 | −61.7 → −61.7 | −56.5 → −56.3 |
  | C5 on 8', 1047 Hz | 0.8 % | −43.6 → −39.2 | −45.4 → −45.2 | −44.0 → −43.8 |
  | C6 on 8', 2093 Hz | 3.1 % | −30.2 → −27.1 | −30.8 → −30.3 | −31.9 → −31.4 |
  | C6 on 4', 4186 Hz | 12.5 % | −16.8 → −17.1 | −19.1 → −18.0 | −22.2 → −20.9 |
  | top, 6331 Hz | 28.6 % | −9.7 → −12.7 | −14.3 → −13.7 | −17.2 → −17.5 |

  The fundamental moves by at most 0.27 dB, at the top. See the
  [decision log](../decisions.md).

### Round 2: patents and a CS-80 sine measurement

**Patents.** Google Patents, assignee Nippon Gakki, priority 1973-1979,
`(sine OR sinusoidal) (triangular OR triangle)`: 39 results, every title
screened. Of the 14 documents opened, 5 returned only a rate-limit page; the
rest are US 4,012,981 and US 4,002,095 (1974 pulse converters), US 4,061,987
and US 4,191,932 (1976 VCOs), US 4,108,040 (1975 harmonic synthesis), JP
S58-9440 B2 (1977, a block-level converter giving sine, square and saw), JP
S59-37518 B2 and JP S60-677 B2 (digital waveform generation) and DE 2755472 C2
(1976, an FM-tuner pilot canceller that turns a triangle into a sine with a
logarithmic amplifier). None shows the WSC's triangle-to-sine shaper.
J-PlatPat was not searched, and web searches found no CS-80 restorer scope
photo of the sine.

**CS-80 sine measurement.** The sample set above has a SINE pack: 11
sine-only notes (MIDI 36-84, sounding 33-525 Hz), the only isolated sine
recording with note-by-note coverage found. Least-squares harmonics relative
to H1 (0.25 s frames, median; MP3 and Vorbis agree within 0.6 dB on H2-H5,
1.0 dB on H6-H9):

| | H2 | H3 | H5 | H7 |
| --- | --- | --- | --- | --- |
| CS-80, range over notes | -26.6..-35.4 | -22.5..-30.3 | -40.4..-46.9 | -59.4..-71.2 |
| CS-80, median | -31.9 | -27.6 | -44.8 | -64.2 |
| Ideal shaper (round-1 engine) | none | none | none | none |
| Differential pair, k = 1.4020 (shipping) | none | -54.1 | -39.0 | -45.1 |

H4 is -42.8..-67.2 dB, H6 -48.5..-64.1 dB, and H8 and H9 lie between -59.5
and -76.8 dB. H3's phase against the fundamental is -5 to +3 degrees from
note 65 up (flattened, compressive peaks) and drifts negative, with about 10
degrees of scatter, to -49 degrees at note 36: the trend a high-pass of
roughly 10 Hz in the path would give. Neither shaper alone produces this
spectrum: the even harmonics and the strong compressive H3 come from
elsewhere in the path, which after the shaper is the sine-level OTA (IG00151,
IC3), two more IG00151 VCAs, the output stage and the unknown capture. The
recording therefore bounds the whole path, not the shaper.

An IG00151 input stage is a differential pair, so the path was modelled as
the shaper followed by one or two tanh stages, each at a free drive
(0 or 0.02-10, 131 values; two free parameters, the same for both shapers),
and the drives asked to reproduce each note's H3 within 0.5 dB:

- Shipping shaper: H5 is 3.5-7.9 dB above the measurement at every note.
  H7 is 10.8-19.5 dB above at 9 of the 11 notes, 6.1 dB above at one and
  reached at one (MIDI 53). Over all drives that put H3 anywhere in the
  measured range, H5 stays within -36.1..-39.2 dB and H7 within
  -48.2..-59.1 dB.
- Ideal shaper: H5 is 0.9-11.2 dB below the measurement at every note. H7
  is reached at 2 notes, within 2.2 dB at 2 more and 6.9-22.4 dB below at the
  other 7. Over the same drives H5 spans -42.2..-58.5 dB and H7
  -61.3..-86.8 dB.
- Fitted to the median spectrum over H3, H5 and H7 together, the ideal
  shaper plus compression misses by 2.3 dB rms and the shipping shaper plus
  compression by 6.2 dB; a differential pair at any drive k from 0.6 to 4,
  with or without one stage after it, misses by about 5 dB.

`measure_cs80_wsc_recordings.py chains` reproduces these figures from the
measurement report.

So the measured path has more H5 than a clean sine compressed by OTA stages,
and less H5 and much less H7 than the shipping shaper compressed by them: it
fits neither. Its H7 argues most against the undegenerated pair fed an ideal
triangle. A shaper whose own H5 and H7 lie between the two (a degenerated
pair, the same pair fed a triangle with softened corners, or another
topology) would be consistent, but no source names one, and its parameters
would be new numbers. The model also leaves out whatever makes the path's H2
and H4 (an asymmetric stage it cannot represent), so the comparison weighs
against B without closing the question. The listening verdict B stands on
ear alone; this sibling measurement is the first evidence on the question,
and it is against B without supporting the round-1 ideal shaper either.
Under the A-Z rule's limit, whether it rejects B is the owner's call; the
change is one commit. Two things would sharpen it: the IG00151 input stage's
drive from its printed input network (VCA work), which removes the free
drives above, and any sine-only take at documented, low VCA level.

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

CS-80 sibling data (the same IG00158 comparator; the CS-80's PWM source is
its own, not the CS-50's VCO II): PWM SLOW runs at 0.154-0.156 Hz and PWM
MED at 0.798-0.806 Hz on all 12 notes, and the audio low fraction sweeps from
0.107-0.162 to 0.794-0.828. A joint fit of the PWM rate's first three
harmonics to that trajectory leaves H2 at -27.9..-36.3 dB and H3 at
-36.4..-41.7 dB of the fundamental: the duty follows a nearly pure sine, as a
sine source compared against a linear ramp makes it. Nothing changes in the
engine.

## Score

Round 1: 7 -> 7.5. The one-shot and comparator now follow the documented
circuit across the instrument's whole range instead of an approximation and a
clamp, and the pitch path has been checked against the factory trim
procedure: the one-shot, octave and scale laws match it; the only discrepancy
is the A3 = 443 Hz reference, which the owner kept at A440.

Rounds 2-3: 7.5 -> 8. The saw/pulse sign now rests on two instruments, the
second through an edge relationship that depends neither on levels nor on
the capture's sign, and is a test contract. The sine shaper earns nothing:
the shipping shaper is chosen by ear, and the first measurement found fits
neither it nor the ideal shaper.

9 needs: the IG00158 sine transfer isolated from the rest of the path (a
scope capture or spectrum at pin 2/SIO, or the IG00151 input-stage drive from
its printed network together with a sine-only take at documented VCA level,
any CS-50/60/80), and where the PO/SO inversion sits (a capture of pins 2, 5
and 7 together), which sets the saw and pulse's sign against the sine.

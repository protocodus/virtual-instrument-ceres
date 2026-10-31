# CS-50 demonstration videos against Ceres, 2026-10-31

Source: Marc Doty (AutomaticGainsay), "The Yamaha CS-50" series on YouTube:
Presets (HupZtUPiJLU), Part 1 Oscillator (d2u1QrIYvnw), Part 2 Filter
(ivVAJ13Tzvc), Part 4 Amplifier (AtoWZaNemxk). The audio was fetched for
analysis on the owner's machine and is not committed; it is lossy (Opus about
128 kb/s, AAC for Part 2), the capture chain is undocumented, and the
performance controls (BRILLIANCE, RESONANCE, touch, LFO, sustain) move during
the demonstrations without being stated. Nothing below changed the engine.

## Preset envelope timing

Envelope times are the one preset property the capture, the codec and the
filter levers cannot move. Protocol, applied to the video and to Ceres
renders of the same notes and held durations (canonical `15e38f1`, 48 kHz,
four voices, 4x, seeds 50-52, median): RMS in 20 ms Hann windows, 5 ms hop,
dB re the event's maximum; onset at -30 dB; t_peak from onset to the maximum;
rise 10-90 % in linear RMS; decay to -6 dB; sustain as the median of the last
0.3 s before key-up; release T60 from the slope between 3 and 23 dB below the
key-up level. Only isolated events count (0.3 s more than 30 dB below the
peak beforehand, no captioned speech): 12 events in 8 presets. Key-up in the
video is a two-segment fit, every event ends at a re-attack, and several
release fits are marked unreliable.

| Preset | Notes | Held s | t_peak video / Ceres | rise video / Ceres | release T60 video / Ceres |
| --- | --- | --- | --- | --- | --- |
| String I | E3 G4 | 1.41 | .345 / .44 | .22 / .275 | 1.01 / .061 |
| String II | A3 D4 F4 | 1.19 | .825 / .04 | .025 / .025 | .14 / .22 |
| Brass II | F4 | .31 | .020 / .025 | .010 / .015 | .13 (unreliable) / .21 |
| Clavichord | G4 B4 | .27 | .025 / .015 | .010 / .010 | .125 / - |
| Guitar I | D4 F4 A4 | - | .145 / .020 | .130 / .010 | - |
| Funky I | A2 D3 F3 | .47 | .355 / .16 | .14 / .08 | .40 (unreliable) / .065 |
| Funky III | D3 F3 A3 | 2.57 | 1.67 / 1.82 | .49 / .545 | .21 / .20 |
| Funky III | C#4 A#4 E5 | .76 | .695 / .73 | .35 / .355 | .19 (unreliable) / .17 |

Funky III agrees within 10 % on every figure. The others scatter in both
directions (String I and Funky I release longer in the video, String II and
Brass II shorter), which a single error in the EG time law could not
produce; a SUSTAIN lever or touch setting off its preset position could. So
the set does not settle the provisional EG CV-to-time conversion beside
`preset()` in `ceres.cpp`, and nothing is re-pinned from it.

## Stated behaviour, checked

- "It does not self oscillate" with RESONANCE all the way up (Part 2,
  01:46): Ceres with `lp_resonance` 1 and `resonance_offset` -10 on noise at
  1 kHz shows a 14.7 dB peak against 4.9 dB at `lp_resonance` 0, and no
  oscillation. Agrees.
- Four oscillators assigned in rotation (Part 1, 01:15) and notes robbed when
  release is long (Part 4, 05:09): Ceres' four-board allocation. Agrees.
- "There are four of them", noise growing with notes (Part 1, 07:48): the
  SUB-board schematic (SM2 p.8) has one zener source feeding all boards, as
  Ceres does; a shared source also grows with the number of open boards.
  The schematic stands over the spoken reading.

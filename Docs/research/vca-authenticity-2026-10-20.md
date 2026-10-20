# VCA and output stage authenticity, 2026-10-20

Scope: the four IG00151 VCAs on each M board (sine level IC3, filtered level
IC4, common LEVEL/touch/tremolo IC5, envelope IC6), the sine/filtered balance,
the output coupling, the voice bus and the PRA output preamp with VOLUME.
Filters, envelopes, oscillators, SUB, ring and the expression network are out
of scope and unchanged.

Sources (page = printed page / PDF page): Yamaha CS Series Servicing Guide
(“guide”), CS-50 Service Manual 1/2 (“SM1”) and 2/2 (“SM2”), CS-50 block
diagram (one sheet), and, as labelled sibling evidence, the Yamaha CS-60
Service Manual (“CS-60 SM”), whose M and PRA boards the CS-50 shares. The PDFs
are copyrighted and not in the repository.

## What the schematics establish

| Item | Value | Source |
|---|---|---|
| Signal path | IC3 (sine) and IC4 (filtered) current outputs join at CP4 into 27 kΩ; 1 µF NP + 100 kΩ to IC5 +I; IC5 out at CP5 into 27 kΩ; 100 kΩ to IC6 +I; IC6 out into 2.7 kΩ, emitter follower Tr8 (4.7 kΩ to −15 V), 4.7 µF NP + 1 kΩ to the voice output O | SM2 p.16 / PDF 12 |
| Gain trims | IC3 VR11, IC4 VR12, IC5 VR13, IC6 VR14: B-100k across ±15 V into EI (pin 1) through 100 kΩ | SM2 p.16 / PDF 12 |
| Zero point | the pin-1 trim is set for zero output at 0 V on LI (pin 2), maximum at 10 V; output “proportional” to LI | guide p.40–41 / PDF 49–50 |
| Feedthrough trim | IC6 only: VR21 (B-100k across ±15 V) into +I through 10 MΩ | SM2 p.16 / PDF 12 |
| Level-lever control RC | LP1/LP2 reach IC4/IC3 LI through 100 kΩ with 0.01 µF to ground; IC5 LI (LC) has 0.01 µF to ground | SM2 p.16 / PDF 12 |
| Shared board | M board #21233, “Common Models CS-60”; the sheet's CS-80 vs CS-60/50 table differs only in R1–R5 and VR4–VR6, the transposition trims (guide p.64 names VR4 B-1k, VR5 B-2k, VR6 B-5k) | SM1 parts list p.2 / PDF 14; SM2 p.16 / PDF 12; guide p.64 / PDF 73 |
| Voice bus | four O outputs through 4.7 µF + 1 kΩ into 47 kΩ at I1; IC1-1/2 (HA1452) voltage follower to O1 | SM2 p.22 / PDF 16 |
| Output preamp | I2 (VOLUME wiper) → 100 kΩ → (Tr5 mute) → 47 kΩ → IC2-1/2 (−) input; feedback 150 kΩ + VR6 B-1M rheostat; (+) to ground via 68 kΩ; output → 33 µF NP → HO through 560 Ω, and 4.7 kΩ to LO with 560 Ω to ground. Gain 1.02 to 7.82, inverting | SM2 p.22 / PDF 16 |
| Preamp supply | +15 V through 390 Ω, −15 V through 220 Ω, each decoupled by 33 µF | SM2 p.22 / PDF 16 |
| Preamp IC | IC1, IC2: HA1452 | SM2 p.22 / PDF 16, note 1 |
| Output trim | “The maximum output level should be adjusted by VR6 to 0±1 dBm p-p at the HO terminal and −18±3 dBm p-p at the LO terminal” | guide p.47 / PDF 56 |
| Output definition | “0dBm for HIGH, and −18dBm for LOW, when all the volume controls are turned fully clockwise and keys for four notes are depressed” | guide p.7 / PDF 16 |
| Output labels | HIGH 0 dB/600 Ω, LOW −20 dB/600 Ω | block diagram |
| Shared PRA | “IC9 is used for CS-60 only”; only one resistor value differs (CS-50 270 kΩ, CS-60 470 kΩ, the headphone feedback) | SM2 p.22 / PDF 16, notes 7–8 |
| M-board VCA alignment (sibling) | VI.1 VR11: sine lever 10, 2 OCT-DOWN, PANEL, CP4 = 1 Vpp. VI.2 VR12: pulse, VCF LEVEL 10, RESH/RESL low, LPF high, NORMAL, CP4 = 800 mVpp. VI.3 “VR3” (IC5's trim is VR13) with LEVEL 10 sets CP5; its figure is not printed. VI.4 VR14 with A, D, S, R at short/10: O = 0.24 V, “CS-50 is 0.6 V”. VI.5 VR21 with LEVEL 0 and a key down: O at minimum | CS-60 SM p.38–39 / PDF 40–41 |
| Output alignment (sibling) | PRA III VR6: sine lever, S and LEVEL at max, PANEL, all keys down, OUT jack 0±1 dBm | CS-60 SM p.39 / PDF 41 |

The unloaded LO/HO divider is 560/(4.7k+560) = −19.5 dB, inside the guide's
−18±3 dB, so the HO/LO wiring is read correctly.

## Sine/filtered balance

The CS-60 M-board procedure (sine-only CP4 1 Vpp, filtered pulse-only CP4
0.8 Vpp) was carried as sibling evidence. It is the CS-50's own: the M board is
one part number for both models, the only tabulated differences are in the
pitch section, and the procedure itself names the CS-50's value where the two
differ (VI.4, O = 0.6 V). Neither the guide nor SM1/SM2 prints a separate CS-50 VCA level
procedure. The engine's sine (±1) and filtered pulse (±0.8 after
`filteredVcaCalibration`) already hold that 1 : 0.8 ratio, so nothing changes
in code beyond the comment. Still unprinted: the test key and the HPF lever
behind the 0.8 Vpp, which is why `filteredVcaCalibration` stays a broad-pass
estimate.

## Output preamp: from guessed tanh to parts

Before: `rail = .9*tanh(x*.18*volume/.9)`, labelled “deliberately mild,
uncalibrated”. At the reference condition itself (four sines, full LEVEL and
VOLUME) it took 1.5 dB off the peaks (0.587 against 0.700) and 0.55 dB off the
RMS. At full VOLUME the factory presets lost 1 to 6 dB of peak (Funky 1:
argument peak 1.63 against a 0.9 ceiling).

The hardware cannot do that. The preamp is an inverting op-amp stage, and its
only nonlinearity is its output swing:

- **Rails.** The HA1452 is a two-channel audio preamplifier rated 25 V, Icc
  12 mA (radiomuseum.org “HA1452” data; no Hitachi sheet was found). The 390 Ω
  and 220 Ω droppers exist to keep it inside that rating: at 12 mA they leave
  +10.32 V and −12.36 V on the chip (22.7 V total).
- **Swing margin.** The HA1452's output swing is unpublished. The documented
  sibling is the NJM4558 on the same PRA board (IC3, IC4, IC9): typical ±13 V
  on ±15 V into 2 kΩ or more, a 2 V margin. HO's own load is 4.7k+560 Ω plus
  the feedback network, above 2 kΩ. HO therefore clips at **+8.32 V and
  −10.36 V**, hard, as an op-amp does once the loop opens.
- **Anchor.** The CS-50 guide defines 0 dBm at HIGH with all volume controls
  full and four keys down; the shared-PRA CS-60 procedure says which tone:
  the sine lever, SUSTAIN and LEVEL at maximum on PANEL, all keys. Each sine
  is 1 Vpp at CP4 (VI.1), engine ±1, so four incoherent sines are √2 RMS and
  0.18·√2 = 0.2546 at full VOLUME reads 0.775 V RMS at HO. The engine's own
  render of that condition measures 0.2542 RMS (`Ceres.output_stage_contracts`
  holds it within 0.1 dB).
- **Cross-check.** VI.4 sets a CS-50 voice output O to 0.6 V for 0.8 Vpp at
  CP4, a CP4-to-O gain of 0.75. Four reference sines then put 0.132 V RMS on
  the 47 kΩ bus through their 1 kΩ branches, so VR6 must set the preamp to
  5.9, inside its printed 1.02–7.82 range. The parts and the two procedures
  agree.
- **Result.** In the engine node (HO inverted) the rails are **+3.40 and
  −2.73**: linear `.18·volume·x` below them, clamped above. That is about
  12 dB above the reference's own peak.

Assumptions, stated rather than estimated away: the service meter reads RMS
and HO is unloaded during the VR6 trim; the four notes add incoherently; the
HA1452 draws its typical 12 mA. The CS-60 procedure reads the OUT jack, the
CS-50 guide the HO terminal, and neither names a load. Reading 0 dBm as into
600 Ω (the block diagram's label) lowers both rails by 5.7 dB, to +1.76 and
−1.41. On the loudest panel state tried (Funky 1, full VOLUME, full sine,
touch LEVEL depth 1 with full pressure, full tremolo, four voices) that
reading clips the loudest stabs and the unloaded one does not. That is the
one open choice here; it is the listening set below. A p-p reading of
“0 dBm p-p” would raise the rails a further 9 dB.

Output can now exceed digital full scale at full VOLUME (Funky 1 with full
touch and tremolo peaks at 1.9). That is the hardware's headroom, not a
limiter; `output_trim_db` is the place to set a host level.

## What remains open, and what would close it

| Gap | Effect | Data needed |
|---|---|---|
| IG00151 transfer above 10 V LI | touch LEVEL (up to +1.08 of full) and tremolo (±0.3375) push LC past the trimmed maximum; kept linear | IG00151 data sheet or a measurement of output current against LI at 0–20 V |
| IG00151 input stage | linear range, input admittance (see `Docs/accuracy/sine-input-admittance.md`), internal current limit | Yamaha IC Guide pin-level values or a measurement |
| IC5 control feedthrough | IC5 has no offset trim, so SUB tremolo could reach CP5 as a DC-coupled signal into IC6 | a measurement of CP5 with a 0 V input and swinging LC |
| LP1/LP2/LC control RC | 100 kΩ/0.01 µF at LI is a 1 ms lever smoothing only if LI is high-impedance | LI input impedance |
| HA1452 swing and Icc | rails ±1 V either way | Hitachi HA1452 data sheet |
| HO load during the VR6 trim | rails move 5.7 dB between unloaded and 600 Ω | the load Yamaha's service bench used, or a measured CS-50 HO clip level |
| CP5 level (VI.3) | the CS-60 figure for IC5's trim is not printed; only the product CP4→O (0.75) is | the missing VI.3 figure |

## Listening

`/mnt/project-files/ceres/listening/2026-10-20-vca-output-rail/` holds A
(shipping tanh) and two candidates on loud Funky 1 material, RMS-matched, with
`key.md`. Removing the tanh is decided by the parts, not by ear; the two
candidates differ only in the load behind the 0 dBm anchor, which no source
prints. The branch ships the unloaded reading (the service procedure reads a
terminal); no by-ear verdict is claimed yet.

# Sound-path gaps and bounded circuit additions

18 October 2026. Source review against canonical commit
`10e17fd9ecdc0a5a0354bad4c5a53780885a5503`, before the separate sound-path
implementation. Keyboard mechanics, pressure/touch changes, pedals, expression
operation and external audio input are excluded. This document does not claim
that implementation tests demonstrate a match to an instrument.

## Primary evidence and present gaps

The [CS-50 service archive](https://bama.edebris.com/manuals/yamaha/cs-50/)
contains the Yamaha small-sheet manual, large-sheet manual and Overall diagram.
The [Yamaha CS Series Servicing Guide](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf)
supplies explanatory diagrams. Below, PDF page numbers are one-based; printed
page numbers follow the slash. Exact local scan and crop hashes are in
[the evidence manifest](soundpath/source-manifest.json); scans remain outside
this repository. The archive listing was verified online on 18 October. Direct
PDF opens were unavailable through the web tool; existing hash-bound scans,
not a newly fetched version, supplied the circuit evidence.

| Priority | Sound-impacting difference | Direct evidence |
|---|---|---|
|1|Global BRILLIANCE moves both cutoff-control nodes; it is absent from the baseline model.|Service2 PDF20/26 PN2 TVR7: C100k, +15/−15 V ends, separate grounded center tap, wiper to R7 BR29. Service1 PDF8/14: BR→HP680k; BR→LP220k.|
|1|Global RESONANCE shares one source between all eight Q-control nodes. Baseline exposes independent instantaneous effective HP/LP resonance only.|PN2 TVR6 C100k from +10 V to ground, wiper R7 RES51. Each node receives RES and its stage's preset bus through separate220k resistors. M service2 PDF12/16: each pin7 has10µF; pin6 trim is separate.|
|1|Ordinary SUB filter modulation has unequal HP/LP coupling. Baseline applies the same octave offset to both.|R7 SUB30→HP150k, LP47k. PN3 service2 PDF21/27: one selected SUB source through a B10k depth pot. PWM is independent.|
|2|Common VCA LEVEL is distinct from filtered-branch VCF LEVEL and downstream Master VOLUME. Baseline folds factory common LEVEL into final volume and multiplies AM gains.|Guide PDF48/39 shows filtered/sine mix→common-level/AM VCA→envelope VCA. R7 mixes SUB L1/120k, LEVEL L2/270k and touch L3/150k into the same LC. M PDF12/16 and PRA PDF16/22 establish placement before voice bus/ring.|
|Deferred|Actual preset source loading, chip control laws, waveform conditioning, analog multiplier distortion/leakage and output loads remain incomplete.|These are calibration/internal-circuit limitations, not missing oscillators or permission to invent a fitted transfer.|

The current core already contains the five non-external SUB waveforms, separate
PWM oscillator, sine bypass, post-bus ring multiplication with wet coupling,
amp/filter capacitor envelopes and sourced output coupling. They must not be
reported as absent. Ring's input/carrier coupling, wet trim and transistor
transfer remain reduced; exact oscillator/reset conditioning is unresolved.

## Cutoff-node equations and safe incremental coordinates

For stage j and voice i, let u be physical pin4 voltage, P its shared preset
bus, B actual shared BR voltage, S actual SUB depth-wiper signal, E own EG,
and b the local bias source. A small-signal circuit has

```
Cfc du/dt = (P-u)/39k + (B-u)/Rbr + (S-u)/Rsub
          + (E-u)/Reg + (b-u)/Rbias - u/Rrank - I4(u)
          + other still-connected source currents.
Cfc = 0.01µF; Rbr = 680k HP / 220k LP; Rsub = 150k HP / 47k LP.
```

Grounded rank values A/B/C are HP2.7k/2.2k/1.8k and LP3k/2.4k/2k, per
the direct M table. A zero-amplitude excluded control does not establish zero
source impedance; its static loading remains part of the unknown baseline.
M shows330k bias feed plus a B100k rail-to-rail pot (wiper resistance≤25k);
Overall shows150k. LP EG47k is corroborated, while HP220k versus100k remains
unresolved. Do not reuse user-editable EG scales to define other input paths.

With actual other source voltages stiff, local input loading and exponential
gain cancel in the within-stage derivative ratio:

```
(d log2(gm)/dB)/(d log2(gm)/dP) = 39k/Rbr
(d log2(gm)/dS)/(d log2(gm)/dP) = 39k/Rsub.
```

These are circuit ratios, not a recovered volts-to-octaves calibration. The
guide's `fc=125*KV*2^Vfc` and CS-50's stated KV=.25 V,Vf=5 V→1 kHz do not
resolve the relationship between that Vf coordinate and the strongly loaded
physical pin4. Rank/trim/input current and raw preset-to-bus loading are unknown.

For a deliberately minimal addition, keep the old effective reference cutoff
and choose the **existing provisional** manual slopes .7/.65 oct/V. Then a
stiff actual BR-node offset gives .0401470588 HP and .1152272727 LP oct/V.
Interpret existing `filter_lfo` as LP octave depth; the corresponding HP depth
is `(.7/.65)*(47/150)=.3374358974` times LP. This supplies the missing
resistor asymmetry while retaining explicit calibration uncertainty. It changes
old nonzero filter-LFO patches; zero-depth defaults remain unaffected.

Scale variable conductance only:

```
f = ffix + max(0,fbase-ffix)*2^(existing_control_octaves + delta_octaves)
ffix = 1/(2π*4.7M*1.5nF) = 22.575169232893 Hz.
```

For a finite identical preset source resistance Rth and four voices, the
common-signal ratio relative to nominal source voltage replaces39k by
`39k+4Rth`. This is **not** interchangeable with the actual-bus convention.
Named factory Rth can be computed locally as selected resistor||10k while its
diode conducts; hand patches do not supply such a resistor. No default Rth
should be fabricated from an effective cutoff.

BR is a passive center-tapped pot, not a buffered voltage source. Within one
half, unloaded `Bnom=±15β`, `Rb=Rhalf β(1−β)`, with unknown taper/half split.
One finite wiper drives four HP and four LP loads. The proposed `brilliance`
control must therefore mean an actual loaded/stiff-node offset, not a calibrated
lever position or complete passive-pot model.

## Eight Q-control states and the anchored reduction

Let qji be physical pin7, Pj the two preset buses and r the one actual RES
wiper. R=220k,C=10µF, with unknown chip input current I7:

```
(Pj-Ej)/Rthj + sum_i(Pj-qji)/R = 0
(r-Er)/Rr + sum_ji(r-qji)/R = 0        (or r=Er if Rr=0)
C dqji/dt = (Pj-qji)/R + (r-qji)/R - I7,ji(qji).
```

The real pot has Er=10a and Rr=100k*a*(1−a); electrical a is not mechanical
lever travel. Service1 PDF9/19 specifies **pin7**0 V→Q5 and10 V→Q.5;
pin6 current sets the Q5 endpoint. It supplies no intermediate Q/current law.
The guide's 1k dummy-load mixer test does not identify chip input resistance.

At fixed impedances, linearize as I7=Gq*q+offset. The six within-stage
difference modes have `tau=C/(2/R+Gq)`. With stiff actual RES, each stage's
common mode has

```
Rej=R+4Rthj
tau_j=C/(1/Rej+1/R+Gqj)
delta_q_inf=(delta_RES/R)/(1/Rej+1/R+Gqj).
```

With stiff preset buses and zero incremental chip current, this becomes
**delta_q_inf=delta_RES/2, tau=1.1 s**. Eight equal increments reduce exactly
to one shared increment, while different base effective HP/LP settings remain.
This is a limiting model, not proof that every physical preset voltage halves.
The minimal `resonance_offset` may use a signed engineering voltage delta
about the prior calibration; ±10 V is not a claim that every offset is reachable
from the unknown baseline by a real0–10 V pot.

Retain the old reference damping and apply the existing provisional slope:
`delta_d_ref=.18*delta_q`, equivalently `r_effective=r_base-delta_q/10`,
with documented control-range clipping. Apply audio-source loading afterward,
as in the existing circuit. Native r=1 means reference damping.2, not physical
Q1; the audio feedback/source capacitors alter actual pole Q.

These control states must continue through notes/allocation and when the offset
returns to zero; note-on must not retrigger them. A global-increment-only model
still leaves old preset/manual resonance changes instantaneous. It does not
reproduce full preset-transition memory. Reset initialization must be stated;
ordinary panic should not silently erase a still-applied performance control.

**Do not vary passive pot source resistance while solving only zero-mean
increments.** Changing Rr or Rb also changes baseline preset-to-wiper DC backfeed.
That requires actual baseline voltages/currents or declared virtual calibration
sources. The fixed-source incremental reduction intentionally avoids that
unidentified reconstruction.

## Common VCA and deferred mechanisms

An added `amp_level` belongs before the envelope VCA, bus and ring; master volume
stays downstream. Additive LEVEL/AM control is more faithful to the LC node than
independent multiplication. Preserving the old normalized AM at amp_level=1
can use `max(0,amp_level-depth*(.5+.5*SUB))`, but its negative DC offset,
normalization and zero threshold remain engineering choices. The3 Vpp SUB
specification alone does not calibrate audible depth. Touch remains unchanged
and excluded from this implementation scope.

The sourced nominal58–60µs VCO duration cannot be promoted alone: frozen
59µs raw/conditioned variants already worsened the fitted6µs baseline's dry
harmonic scores (late E4 .114→2.188/.199 dB; D4 .812→1.970/.823 dB).
This does not prove physical6µs, but rejects an unsupported default replacement.
Likewise, prior maximum-Q and half-octave-LP experiments retained adverse
early/tail outcomes; no new control path is automatically a fidelity improvement.

## Verification and next evidence

[check_networks.py](soundpath/check_networks.py) independently solves11-node
passive networks, verifies36 cutoff derivative ratios and18 Q networks/modes.
Run `python3 Docs/soundpath/check_networks.py --check`; the result is
[nodal-checks.json](soundpath/nodal-checks.json). No renderer, optimizer or
hardware audio is invoked. These checks verify equations, not component gain
calibration.

First evaluate old neutral-control parity and explicit BR, RES-step, unequal
SUB and amp-level/AM witnesses. Then replay the existing public Brass I and
Flute cases with fixed MIDI hypotheses, source masks, gains and adverse controls.
Do not optimize to the reused passage and call it a holdout. The most decisive
physical measurement remains known-input cutoff/Q response plus actual pin4/7
voltages/current on an identified board. Until then, label these additions
source-backed topology with conditional calibration.

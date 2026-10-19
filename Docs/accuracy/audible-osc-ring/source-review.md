# Audible oscillator and ring circuit omissions

19 October 2026. Primary Yamaha drawing review and passive circuit calculation only. No DSP edits, public-audio measurement, fitting or native render. File identities and source URLs are in `evidence.json`. This note adds to the retained audits; it does not rewrite them.

The two practical changes are **the source-timed, conditioned VCO output feeding the WSC**, and **the ring carrier's differential input/bias coupling network**. Both can affect ordinary sound. Their known topology is stronger evidence than the previous arbitrary reset time or a completely flat carrier path, but neither supplies a complete calibrated transistor model.

## 1. VCO current balance, conditioned output and WSC

Re-viewed Guide printed28–31/PDF37–40 and direct M components, service2 PDF12/printed16. IG00153 CT10 has1200pF and22k+B5k toward+15V;120pF is compensation at pin6. The integrating capacitor at To14 is22nF. Guide28 gives Iref=800µA, I=Kv/RFT and charge balance:

```text
C2*Vopp=(Iref−I)*tau=I*t
f=1/(tau+t)=Kv/(RFT*Iref*tau).
```

Guide29 gives Normal250mV→130.8Hz,1V→523.2Hz. Guide30 maps Normal to branchIV/R2,40k+B1k. Thus nominal tau=1/(523.2*RFT*.0008)=59.7286µs at40k,58.2718µs at41k. This is an inference from Yamaha's simplified current/frequency law, not bare RC, an instrument measurement or a hard manufacturing tolerance. Current source branches reviewed still use6µs, with a50µs parameter ceiling, and a fixed-amplitude raw triangular return. The latter is not the documented output pin.

Guide28 routes both TI12 and the one-shot pulse into an opaque wave shaper before OUT7. OUT7 is drawn with a fast rise, brief plateau, downward step and descending ramp; TI12 has a finite rising recharge. With a frequency-independent normalization A0=Iref*tau/C2 and phase zero at recharge start, current balance gives descending `p=1−2phi`. The explicit reduced conditioner is:

```text
r=f*tau
p(phi)=1, phi<r; 1−2phi, otherwise
saw=−p                            # WSC inversion
triangle=2*abs(p)−1
sine=sin(pi*triangle/2)            # still an ideal WSC nonlinear transfer
```

The flat plateau and its exact amplitude are assumptions consistent with the sketch, not a uniquely recovered clamp circuit. The inferred step2r, saw mean−r² and sine mean `r−sin(2*pi*r)/(2*pi)` are model predictions. At3520Hz, r≈.21025: the sine discontinuity `1−cos(2*pi*r)` is about.753, so this is not an inaudible schematic detail. Both value jumps and slope changes need antialiasing; shaping an already bandlimited saw is not automatically a correctly bandlimited nonlinear output. Keep existing physical coupling states rather than manually deleting every predicted DC component.

The pulse's fixed leading edge and moving falling edge remain separately documented. At fixed comparator duty d, the proposed unchanged falling edge requires r<d. The existing r≤.35 numerical guard makes that true for the static.5–.9 duty range, but PWM's engineering extension can put d below r; an explicit policy is needed there. Clamping r to.35 also abandons constant tau above approximately5859.84Hz. This is an out-of-range fallback, not a Yamaha calibration. Tests must cover both conditions and simultaneous BLEP/BLAMP events.

**Prior result must remain visible:** `experiments/vco-conditioning-01` already ran12 native comparisons on v0.7. Conditioned59µs changed saw only; its WSC sine still used an independent ideal ramp. Raw59µs worsened fixed-control harmonic MAE substantially. Conditioned59µs was much closer to baseline but did not beat it: lateE4 .11447→.19846dB, lateD4 .81179→.82292dB, dry saw .33340→.34632dB. All square variants were byte-identical. Those controls were fitted effective patches with incomplete recording-chain knowledge; the result neither establishes the6µs circuit value nor validates the proposed conditioner. Updating the same-ramp WSC is a new part of the present change, but the saw conditioner itself is not an unperformed experiment.

Recommendation: implement the source-derived timing and the clearly labeled conditioned-output reduction, retaining the old source and this adverse comparison. Validate analytic DC/Fourier predictions, pulse timing, bandlimiting and high-frequency guards before public comparisons. Describe any adoption as improved known topology with an approximate hidden shaper, not demonstrated recording accuracy.

## 2. Ring differential carrier network

Direct PRA service2 PDF16/printed22, newly cropped in `evidence/pra-ring-carrier-bias.png`, establishes more than the older audit's abbreviated pin7 network. Guide printed52/PDF61 names carrier pin7(+), pin8(−), signal pin1(+), pin4(−). Use the voltage **pin7−pin8**, not pin7 relative to ground.

Define x=pin7, b=pin8, a=audio-input bias/bypass node and s=local positive-supply/bypass node. Direct PRA has:

| Component | Connection |
|---|---|
|100µF|IG00150 sine output to x|
|33k|x to ground|
|100Ω|x to b|
|100µF|b to ground|
|820Ω|b to a|
|1.2k|b to s|
|100µF and1k|a to ground|
|100µF and390Ω|s to ground and ideal+15V rail respectively|

These four100µF capacitors include the source coupling capacitor. In incremental AC coordinates the ideal supply is ground. First take the declared limit of an ideal voltage-source oscillator, zero incremental IC input/current-source/collector loading, and omit the additional audio-input bias paths. With C=100µF:

```text
C*(x'−u') + x/33000 + (x−b)/100 = 0
C*b' + (b−x)/100 + (b−a)/820 + (b−s)/1200 = 0
C*a' + a/1000 + (a−b)/820 = 0
C*s' + s/390 + (s−b)/1200 = 0
carrier = x−b.
```

For an implementation without input differentiation use z=x−u and state q=(z,b,a,s). Then `C*q' + G*q=B*u`, where G is the symmetric conductance matrix stamped from the six resistors above, and `B=(-(1/33000+1/100),1/100,0,0)`. The output is `u+z−b`. The capacitor state can be integrated with a fixed4×4 trapezoidal solve, precomputed per sample rate. A consistent direct-feed trapezoidal update must use both current and previous u. All resistors are positive and G is positive definite; there is no arbitrary cutoff or fitted time constant.

The source-independent stdlib calculation `passive_carrier.py` independently agrees with a scalar Schur reduction. Its predicted steady sinusoidal differential gains, without amplitude restoration, are:

| CarrierHz | Linear amplitude | dB |
|---:|---:|---:|
|.2|.011975|−38.4345|
|1|.047659|−26.4370|
|6.6692178847|.216124|−13.3059|
|30|.670654|−3.4700|
|220|.988643|−.0992|

This is not the erroneous100µF×33k high-pass. When b is not otherwise clamped, the two100µF capacitors around100Ω suggest an approximate31.8Hz differential corner; the full resistive/bypass network gives the table. The0.2–220Hz range is directly documented in Guide53. Normal slow ring operation therefore exercises this network, and carrier-frequency envelopes also cause genuine coupling-state transients. Do not normalize each frequency back to unit gain: that would erase the component effect under test. Keep the existing global post-bus placement and wet10µF/10k output coupling. At ring_mix0 the new carrier states should run continuously while audio remains byte-exactly dry; selecting wet should not restart their charging history.

### Additional known paths and limits of the four-state reduction

The direct drawing also has470Ω from a to each signal input pin1/4; each signal pin reaches grounded balance-pot wiper through100k plus its portion of B50k. Pin1 receives audio through10µF and a disputed series resistor: direct PRA prints330K, Guide52 prints330Ω. The new calculator expands to seven nodes, with five capacitor states, including all those external paths. It checks330Ω and330k and balance fractions0,.5,1. For the eight declared carrier frequencies, the largest magnitude difference from the four-node reduction is under.026dB. This is a finite comparison under zero IC current and zero audio excitation, not a universal transient/large-signal bound. The expanded equations are exactly the resistor/capacitor stamps in the retained script; the unknown330 value must not silently become an asserted hardware revision.

General physical KCL includes additional current vector `I_IC(v,t)`, the IG00150 output impedance and carrier-output shunt47nF, plus modulator signal/current-source/output loading. The source does not establish that these are zero. The four-state model is a concrete passive limit that implements omitted components now; it does not establish the actual carrier voltage or wet loudness in a populated instrument. Constant DC bias currents also determine operating point and carrier leakage; the AC reduction does not predict them.

Guide52 differs from the direct sheet in carrier and wet-output coupling values (10µF versus100µF exchanged). This note deliberately uses the direct CS-50 sheet; no population-revision claim follows. The manufacturer's leakage adjustment below3mVpp does not set the multiplication coefficient.

## Fairchild evidence boundary and next nonlinear step

The original Fairchild1971 catalog is a relevant primary source, not a reason to substitute MC1496 parameters blindly. During this audit its public URL and trailing-edge mirror returned403; the decromancer mirror timed out. No PDF was successfully acquired or transistor diagram visually verified. The search index exposes printed192 electrical-table text, including signal-port resistance200k under stated test conditions and carrier-input bias currents12µA typical. **Signal-port resistance is not carrier differential input resistance.** These indexed snippets do not justify assigning200k to pin7/8 or claiming the unknown carrier-current term is absent. Failed accesses are retained in `evidence.json`; a manufacturer transistor-law claim remains pending.

A matched differential pair would lead to an ideal switching factor `tanh((v7−v8)/(2VT))`, but applying that here requires verified transistor connections, degeneration, signal/common-mode currents, physical sine amplitude and the wet-gain normalization. Those have not been jointly established. Therefore implement the known passive network independently first; do not invent a thermal-voltage saturation scale in normalized DSP units or use a public recording to set it. An ideal multiplier after this network remains an explicit approximation and should be described as such.

No new public recording, exact gate history, component tolerance or calibrated factory transfer was inferred. Useful native tests are DC rejection, complex small-signal transfer against the nodal oracle, sample-rate agreement, rate/depth transitions with preserved state, ring-bypass parity, and audible fixed-patch slow/fast ring demonstrations. These tests assess implementation and audible consequence, not original-hardware equivalence.

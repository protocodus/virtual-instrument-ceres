# Source-scaled carrier switching in the CS-50 ring modulator

19 October 2026. Primary-source acquisition, visual inspection and constant-only DC calculation. No DSP edit, native audio rendering, recording measurement or fit. The earlier passive-network report remains unchanged. The source diagram now supports a physical carrier switching law with explicit nominal assumptions; it does not identify the complete populated instrument's operating point or audio-port gain.

## Newly acquired manufacturer diagram

PDF page12 of the [Vasulka archive's Electronic Video Synthesizer scan](https://vasulka.org/archive/Artists6/Siegel%2CEric/ElectronicSynth.pdf) contains an original **Fairchild August1970 µA796** sheet. Its copyright and equivalent circuit are visible in `fairchild-12.png`; the retained PDF is `ElectronicSynth.pdf`, SHA-256 `16e8eead814ba8553f70b4fceb6c1080179abfad1962207a943957bf90cdb0fc`. This is direct µA796 evidence; no MC1496 substitution is needed.

The ten-pin diagram agrees with Yamaha: carrier inputs7(+)/8(−), signal inputs1(+)/4(−), gain-adjust emitters2/3, outputs6/9, bias5 and negative supply10. Four upper transistors form two common-emitter switching pairs with directly driven carrier bases and no drawn emitter-degeneration resistor. The two lower signal transistors feed these pairs. Three500Ω emitter resistors belong to the bias mirror, not the carrier differential pairs. The signal-port470Ω external gain resistor must not be inserted into the carrier law.

For matched, forward-active upper transistors at a common temperature, each pair's current ratio is `exp((V7-V8)/VT)`. Dividing its difference by its sum gives

```
F(V7-V8) = tanh((V7-V8)/(2*VT)),  VT = k*T/q.
```

That is a transistor-law reduction, not a guessed soft-clip scale in normalized DSP units. Finite beta, mismatch, Early effect, capacitance and saturation are omitted. The factor multiplying the lower signal-current difference is independent of its total current in this ideal limit. For Yamaha's single-ended pin6 voltage output, positive signal1−4 and positive carrier7−8 give the same product polarity as the current DSP convention.

## Yamaha's nominal carrier scale

The [CS Series Servicing Guide](https://rcarchives.com/m/YamahaCSSeriesSynthesizerServicingGuidePOSTED.pdf), PDF58/printed49, explicitly identifies the IG00150 pin6 sine output and a **3 Vpp** output amplitude. Its following PWM VCA is adjusted to unity at maximum control, so the3 Vpp statement is not an arbitrary downstream gain assumption. The rendered page is `yamaha-pwm-49.png`. Printed53/PDF62 describes the ring carrier as the same VCO family and refers its operating principle back to the PWM oscillator.

Thus **1.5 V peak** is a source-supported nominal IG00150 sine scale for a reduced carrier model. It is not a separate measured ring-board amplitude or a tolerance specification. The ring oscillator output impedance, loading by the carrier network, its47nF shunt, actual amplitude variation and sine distortion remain unresolved. Retain the previously declared ideal voltage-source approximation rather than pretending those terms were measured.

## Do not discard the external DC offset

The [direct CS-50 PRA sheet](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf), PDF16/printed22, puts33k from carrier7 to ground and100Ω between7 and8. The source-coupling capacitor blocks the oscillator's DC, so the carrier inputs are not automatically equal at DC.

`dc_nominal.py` solves six passive DC nodes, with all capacitors open and all IC input, bias and collector DC currents omitted. It includes the known+15V/390Ω local supply feed,1.2k carrier-bias feed,820Ω/1k signal-bias divider, the33k/100Ω carrier network, and both470Ω signal branches with100k plus the respective portion of the50k balance pot returning to its grounded wiper. Centered VR5 gives zero ideal signal-input differential. The source-only330Ω/330k signal-series conflict does not enter this DC solve because the series audio coupling capacitor is open.

The independent series/parallel reduction is

```
R1 = 100k + z*50k; R4 = 100k + (1-z)*50k
Ra = 1 / (1/1k + 1/(470+R1) + 1/(470+R4))
Rb = (33k+100) || (820+Ra)
V8 = 15*Rb/(390+1.2k+Rb)
V7 = V8*33k/(33k+100)
deltaDC = V7-V8.
```

At z=0.5 the nodal solution is:

| Quantity | Conditional voltage |
|---|---:|
|Carrier pin7|7.751514269432 V|
|Carrier pin8|7.775003706612 V|
|Carrier differential|**−0.0234894371800962 V**|
|Signal bias node|4.241518374144 V|
|Both signal inputs|4.225630005324 V|
|Local supply node|13.227831097848 V|

Nodal and independent reductions agree within1.34e−14 V, with maximum KCL residual1.57e−17 A. Moving the balance pot to either endpoint changes the carrier differential by only3.795µV in this limit, while creating±6.59mV signal differential. The balance adjustment cancels carrier leakage through the signal pair; it does not establish a centered carrier-pair DC voltage.

This is an **unloaded external bias limit**, not a self-consistent complete transistor operating point. In the populated circuit the10k bias-pin path and two2.7k collector paths draw current from the local supply, and finite base currents load the signal/carrier nodes. Their DC currents have not been recovered. In particular, at fixed V8, carrier input currentI7 would add `−(100||33k)*I7` to the differential. The already indexed manufacturer signal-port resistance must not be misused as a carrier resistance. No unverified datasheet current value is adopted here.

## Recommended explicit nominal reduction

At the declared temperature25°C (298.15 K), exact SI constants give `VT=0.02569257912108585 V` and `2VT=0.0513851582421717 V`. Let c(t) be the existing passive network's differential response to a unit-peak sine, in its AC deviation coordinate. Then use

```
physicalDifferential = -0.023489437180096218 + 1.5*c(t)
switchingFactor = tanh(physicalDifferential / 0.0513851582421717)
```

Retain the existing downstream signal/product gain convention and wet-output coupling. Do not normalize the factor per rate, remove its mean, divide by its peak, or silently force its DC center to zero. With zero AC carrier, this nominal reduction gives **F=−0.42773806049**. It therefore predicts dry-signal feedthrough in addition to ring sidebands. That is a consequential prediction of the declared bias approximation, not a measured leakage calibration. Its small-signal derivative with respect to normalized c at zero is23.850471018.

The remaining signal-port nonlinear transfer and absolute wet/dry gain should stay deferred: audio scaling, the330Ω/330k source discrepancy, balance/current trim and output loading are not jointly identified. The carrier switching stage can be improved independently without claiming that the complete device has become calibrated. A nominal engineering reference temperature is adequate for this declared model; it is not a measurement of a particular chip's junction temperature.

## Bounded validation, no fitted controls

Independent point checks are F(0)=0, F(±2VT)=±tanh(1), and F(±10VT)=±tanh(5), applied to the **total physical differential**, plus the nonzero-bias zero-AC check above. Verify the derivative by a small voltage perturbation and retain the passive state/transfer oracles. A fixed source-amplitude-zero diagnostic must preserve the predicted offset feedthrough rather than produce a hidden bypass. Symmetric voltages around physical zero should yield odd symmetry; AC values around the biased operating point need not.

For actual synthesis, use fixed rates already declared for the passive network, preserved phase/history, equal gains and no parameter fit. Ring bypass should remain sample-exact while carrier states continue to evolve. These tests and controlled listening can establish implementation and audible consequence; they cannot identify the omitted populated-circuit DC currents or prove improved hardware-recording agreement.

The constant report reproduces with `python3 Docs/accuracy/ring-carrier-nonlinearity-20261019/dc_nominal.py --check`. Retrievals were capped at six, including the initial search. The original1970 transistor page was acquired successfully; an attempted fuller five-page sheet ended in a local TLS certificate-chain failure. That failure is retained and is not described as proof that the public source is unavailable. No further retrieval was attempted. The original passive/source report and its previous failed accesses remain intact.

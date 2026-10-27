# Direct sine input: conditional admittance bound

2026-10-18. Source-only follow-up; no DSP changes, audio renders or recording fits.
**No production pole is selected.**

The direct CS-50 M schematic (large-sheet service manual, PDF12/printed16)
connects WSC sine output/CP1 through **100 kΩ to IC3 IG00151 pin3 (+I)**,
with **0.056 µF from that input node to ground**. IC3 pin4 (−I) is grounded;
its output pin8 joins the filtered branch at CP4. This is the sine-level
VCA **IC3**, not common-level IC5. IC3's separate gain trim is VR11.

The Yamaha IC Guide PDF31/printed2-2 draws grounded input shunts and a gm
converter, but supplies neither their values nor a numerical input-admittance
specification. No feedback connection in this application establishes pin3 as
an ideal virtual ground. The WSC description (PDF39/printed2-6) identifies sine
output pin2 but does not specify its output impedance.

## What the known components establish

Assume IC3's incremental input is a **fixed, nonnegative, frequency-independent
resistance** `Rin`, and use the **actual CP1 voltage** as the input reference.
With `R=100000 Ω`, `C=0.056e-6 F`, KCL gives

```text
Vpin3/VCP1 = 1 / (1 + R/Rin + s*R*C)
H(s)/H(0) = 1 / [1 + s*C*(R || Rin)]
fc = 1 / [2*pi*C*(R || Rin)] >= 28.4205255521 Hz.
```

The lower corner bound is attained as `Rin→∞`; there is **no finite upper
corner bound**, since `Rin→0` sends the normalized pole to infinity. The
engine's omitted shunt response corresponds to that low-input-resistance
limit after its separately retained gain normalization. This is not proof
that the actual chip has zero input resistance.

Illustrations only, not measurements or a proposed allowable resistance range:

| Assumed Rin | Corner | Normalized attenuation at 660 Hz |
|---:|---:|---:|
|100 Ω|28.44895 kHz|−0.002337 dB|
|1 kΩ|2.870473 kHz|−0.223733 dB|
|10 kΩ|312.6258 Hz|−7.369504 dB|
|Infinite|28.42053 Hz|−27.326282 dB|

These follow directly from the equations, with attenuation
`-10*log10(1+(frequency/fc)^2)`. The one-pole bound is conditional on the
resistive-input assumption; a frequency-dependent or active input admittance
requires its actual complex transfer. Modeling an **unloaded** WSC source
instead of measured CP1 additionally requires the WSC output impedance.

## Why calibration does not identify the missing resistance

Related CS-60 printed38 section VI specifies sine-only CP4 output of 1 Vpp
after adjusting VR11, with the sine-level terminal at 10 V and transpose at
2 OCT-DOWN. It does not specify the played key or a second frequency-response
point. Even with a known test frequency, one output amplitude cannot separate
the unknown input attenuation from the independently adjusted VCA gain.
The procedure therefore does not determine `Rin` or this pole.

The missing quantity is **IC3 pin3 small-signal input admittance versus
frequency and control voltage**, with pin4 grounded and the actual operating
bias. No cached primary source inspected here supplies it. A finite low-pass
correction would require an explicit hypothetical resistance or a measurement;
choosing one from a recording would be a fit, not a recovered component value.

## Inspected source bindings

Cached evidence resides under `/tmp/ceres-cs50-1x8LGd/research/`.
Hashes are SHA-256 of the inspected files, not substitutes for the page scope.

| File | Scope | SHA-256 |
|---|---|---|
|`service2.pdf`|CS-50 M, PDF12/printed16|`1102e55b949378e7776555bca1a4d73105ca2d76d6fcf3aa4c2b103566201cfc`|
|`yamaha-ic-guide.pdf`|IG00151 and IG00158, PDF31/39|`d728bfa1b5dc4575414f8d2bca4a13aef95008f7f2ae21b6e2e9cc28f8c2ee09`|
|`master-12.png`|Full M sheet|`ecbf6ff2a15bd3bf10d91d8a33db42c98bc422ce78e6799bf3ca616eba911913`|
|`control-review-vca-path.png`|IC3/IC4 and following VCAs|`6f9857765efb600373f7967d8a865e2fcf7b2c71dacf774e7c090cbb5f9fce5a`|
|`ic-guide-031.png`|IG00151 block and pins|`e10eea98b0a32dcf98ce82aeaf40a89ef30391ba2a47ae83310f047269bc4706`|
|`ic-guide-039.png`|IG00158 sine output|`2034a9a8b4c1c938d470c755a16c690e9af32c6841d8f96d6ba45908d01c65ce`|
|`cutoff-cs60-38.jpg`|Related M calibration, printed38|`3ed81ac23d92872060277cbd6f1534550f2d4f58590b2318f063e274d0506315`|

Primary locations: [CS-50 large-sheet manual](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf),
[Yamaha IC Guide](https://bama.edebris.com/download/yamaha/ic_guide/Yamaha%20IC%20Guide%20Book.pdf),
[CS-60 printed38](https://therogoffs.com/cs80/manuals/CS60_Service_Manual/38%20-%20Adjustments%2C%20M.jpg).

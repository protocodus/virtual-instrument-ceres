# Passive ring-carrier coupling

19 October 2026. Separate implementation based on SUB-corrected `d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43`. It does not include the independently reviewed Q/idle-EG changes or the separate VCO experiment. No parameter key, stored patch format, oscillator clock, envelope law, GUI or recording fit changes here.

The previous ideal ring multiplier consumed the raw unit sine directly. The direct CS50 PRA sheet includes a passive network between its sine oscillator and the µA796 differential carrier pins. Ignoring that network removed substantial low-carrier attenuation and phase shift. The new path retains the raw free-running sine and feeds the multiplier with the actual modeled **pin7−pin8 voltage**. No frequency-dependent compensation or normalization is applied.

## Declared circuit limit

Nodes are `x=pin7`, `b=pin8`, `a=audio-bias`, `s=local-supply`. Four capacitors are 100µF each: source-to-x, b-to-ground, a-to-ground, s-to-ground. Resistors are:

| Path | Resistance |
| --- | --- |
| x → ground | 33kΩ |
| x ↔ b | 100Ω |
| b ↔ a | 820Ω |
| b ↔ s | 1.2kΩ |
| a → ground | 1kΩ |
| s → ground | 390Ω |

This is an ideal voltage carrier source and **zero incremental IC input-current** reduction around the existing multiplier. It is not a transistor-level µA796 emulation, nor identification of carrier-source impedance, chip input conductance, absolute control amplitude, transistor distortion, leakage or wet gain trim. The already modeled 10µF/10k wet-output coupling remains in place.

Primary-page evidence, known sheet discrepancies and the independent passive solver are frozen in the historical repository's [source review](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-osc-ring/source-review.md) and [evidence manifest](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/audible-osc-ring/evidence.json). Manifest SHA-256: `afa33dd2d74d3352167a00d28d00d1134ab173cfcba52f738cf647a146f3286f`. The four-node reduction differs from the expanded seven-node external passive model by at most **0.025077 dB at the eight declared frequencies and tested trim endpoints/midpoint**. That numerical comparison includes the disputed 330Ω/330kΩ audio branch alternatives, still at zero chip input current. It is not a universal frequency/transient error bound or measured hardware accuracy.

Source-only steady responses include approximately −38.4345dB/+83.8094° at 0.2Hz, −13.3059dB/+67.9012° at 6.6692178847Hz, −3.4700dB/+44.9557° at 30Hz and −0.0992dB/+8.2351° at 220Hz. These are circuit calculations, not measured recordings or new wet-gain calibration. In particular, passive pin7−pin8 attenuation is **not calibrated wet-output attenuation**: unknown physical carrier drive and µA796 switching/compression could partly restore low-speed wet gain. Neither a single inferred corner nor the −38dB passive value is claimed as the hardware wet-output response. No guessed compression scale or audio-port resistance has been substituted for carrier input loading.

## Numerical and lifecycle contract

Let `v=(x,b,a,s)`, `u` be the raw sine, and capacitor coordinates `w=v−e0*u`. The conductance matrix `G` is stamped from the six resistors above. The physical equation is `C*w' = −G*v`, where `C=100µF*I`.

The trapezoidal companion implementation factors `M=(I+G/(2*internalFs*C))^-1` once when constructing the synth. Every internal sample computes

```
v = M * (memory + e0*u)
memory = 2*(v - e0*u) - memory
carrier = v[0] - v[1]
```

The companion stores the previous source history implicitly; no separate input-history sample is missing. Tests independently solve the equivalent simultaneous physical-state trapezoid with explicit previous `w` and `u`. The sample path uses fixed arrays and no allocation, factorization or exception.

The network advances even at `ring_mix=0`, preserving the free-running electrical history when wet mix is enabled. Ordinary note events, note-off, presets and live carrier controls do not reset it. Explicit `reset()` clears carrier phase and coupling. `allSoundOff()` keeps the old free phase but clears the new coupling alongside the existing wet capacitor: this is a **declared software panic convention**, not a physical key-off action. The raw diagnostic `ModulationState.ringCarrier` keeps its previous meaning; appended `ringCoupledCarrier` reports the network output. The allocating offline `ringCarrierResponse()` diagnostic runs the exact production kernel; it is excluded from the embedded API.

The explicit mix-zero branch still returns the dry bus with the previous arithmetic. Wet and partial-mix sounds intentionally change. The untouched ideal multiplier/wet gain may remain inaccurate even though the documented passive carrier dependence is now represented.

## Validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
python3 Tests/check_ring_bypass_parent.py --build-dir build --report Docs/ring-carrier/parent-parity.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON
cmake --build build-sanitize --target ceres_ring_carrier_tests ceres_tests ceres_embedded_contract -j 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure -R 'ring_carrier_circuit_contracts|native_dsp_contracts|embedded_allocation_contract'
python3 Tests/record_ring_carrier_validation.py
```

New tests compare native impulse Fourier coefficients with an independently solved complex four-node KCL system at 0.2, 1, 6.6692178847, 30 and 220Hz, at 8/192kHz processing rates. The expected complex frequency is the trapezoid's bilinear mapping, not an accidentally prewarped analog component. Complex error must be below `2e-10`, phase error below `2e-8` rad; the retained impulse tail must be below `1e-14`. Independent direct physical-state trapezoids also check abrupt/continuous source changes and KCL residuals. Production tests verify the kernel is connected, continues at mix zero and survives ordinary note/control events; panic/reset behavior is explicit.

Existing sideband tests, native/embedded active-ring parity and the 57-key allocation contract remain in the suite. The parent checker recompiles the immutable parent and requires **768,000 byte-identical dry samples** across four rates and active carrier/depth/envelope settings, with notes, release, pressure, sustain, panic and reset. Temporary comparison binaries/float streams are deleted; retained source, commands and hashes reproduce the check. No public-recording-linked render or parameter fitting is part of this validation.

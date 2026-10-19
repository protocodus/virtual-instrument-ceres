# Physical ring-carrier steering

19 October2026. Isolated change based on passive-ring commit `8c5a96e75f426dd7854cfee8e76cccd9478bbbfe`. It includes the retained SUB-polarity correction and prior passive carrier coupling, without the separately implemented VCO or Q/idle-EG changes. No parameter key, GUI, preset mapping, carrier clock, envelope law, recording fit or output gain correction is added.

## Circuit and conditional operating point

The original Fairchild µA796 equivalent circuit shows four upper carrier-switching transistors with bases at pins7/8 and no emitter degeneration. Matched forward-active collector currents obey `I7/I8=exp((V7−V8)/VT)`, hence the normalized difference is

```
F = (I7-I8)/(I7+I8) = tanh((V7-V8)/(2*VT))
VT = kB*298.15/qe = 0.02569257912108585 volts
```

The three500Ω internal resistors are outside those upper differential pairs; importing emitter-degeneration values from another modulator IC would change this law without source support. The Yamaha guide printed49 gives a nominal3Vpp sine at IG00150 pin6. The existing passive network therefore supplies `1.5*c` volts, where `c` is the existing differential response to a unit sine. Raw oscillator and passive-response diagnostic meanings remain unchanged.

The direct PRA external resistor circuit is also asymmetric at DC. With the balance trim centered and **all IC currents omitted**, the independent six-node calculation gives `V7−V8=−0.023489437180096218 V`. This includes both470Ω+(100kΩ+25kΩ) signal-input returns. The implementation independently reduces that DC circuit:

```
Ra = 1k || (470+125k) || (470+125k)
Rb = 33100 || (820+Ra)
V8 = 15 * Rb / (390+1200+Rb)
DC = -V8*100/33100
steering = tanh((DC+1.5*c)/(2*VT))
wetInput = sharedBus * steering
```

No mean removal, peak normalization or wet gain compensation follows. At zero AC, the nominal steering is approximately−0.427738. At the0.2Hz endpoint the nominal AC swing is smaller than the DC imbalance, so this model does not reverse steering. That is a useful implementation test of this declared limit, **not measured hardware feedthrough**. At higher carrier frequencies the steering compresses toward switching and creates odd and even carrier harmonics. The old tests that assumed ideal-sine-only sidebands have consequently been replaced with independent physical-current Fourier expectations.

Primary evidence and the independent DC solve are retained in the historical [source audit](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/ring-carrier-nonlinearity-20261019/source-audit.md) and [source manifest](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/ring-carrier-nonlinearity-20261019/evidence.json). The manifest SHA-256 is `d023d975ce4edaa5ee6adc32f04914b66424e486f6e0431520d0dfdf2dbba030`. It binds the original Fairchild August1970 sheet (PDF12), the Yamaha source-amplitude page, the direct PRA diagram and the source-only DC calculator. No component or voltage was selected from a recording fit.

The limits are material: nominal3Vpp is treated as an ideal source; temperature is fixed25°C; the upper pairs are matched, forward-active and infinite-beta; base currents, Early effect and saturation are omitted. The DC solve omits **all** IC loads, including bias and collector currents, while the passive AC reduction omits incremental IC input current. Thus its DC point is not a complete populated-circuit solution. The existing four-node AC reduction differs slightly from the expanded external seven-node model; the preceding source audit bounds only its declared sampled cases. Signal-input nonlinearity and absolute wet gain remain unresolved, including the330Ω/330kΩ external audio-path disagreement. The linear signal-port gain convention is retained, not newly calibrated. This is a physically scaled reduced model, not a transistor-exact simulation or a demonstrated recording-match improvement.

## State and real-time behavior

The raw free-running sine and four capacitor histories advance even at mix0. Notes, key-off and live patch changes do not reset them. Reset clears phase/coupling; the explicit software panic preserves raw phase but clears coupling and wet-output history, as in the passive parent. The new instantaneous steering then returns to its nominal DC value rather than an artificial zero. The explicit dry bypass still uses the parent's exact arithmetic. No heap allocation, new state solver or parameter was added to the sample path; one bounded `tanh` follows the existing passive solve.

Native `ModulationState.ringCarrier` remains the raw sine and `ringCoupledCarrier` the passive differential response to unit source amplitude. Appended `ringSwitchGain` is the physical steering including DC. The native-only `ringSwitchingResponse(volts)` diagnostic evaluates the same kernel on a supplied **total physical differential voltage**; it does not add source amplitude or DC again. Embedded processing uses the identical function, without the diagnostic API.

## Reproduction and evidence

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
python3 Tests/check_ring_switching_parent.py --build-dir build --report Docs/ring-switching/parent-parity.json
python3 Tests/check_ring_switching_alias.py --build-dir build --report Docs/ring-switching/alias.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON
cmake --build build-sanitize --target ceres_ring_switching_tests ceres_ring_carrier_tests ceres_tests ceres_embedded_contract -j 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure -R 'ring_switching_circuit_contracts|ring_carrier_circuit_contracts|native_dsp_contracts|embedded_allocation_contract'
python3 Tests/record_ring_switching_validation.py
```

Tests use independent exponential BJT currents, a full six-node DC solve, small-signal derivative/voltage units, finite limits and actual production state/lifecycle checks. The actual fully wet audio is checked against a separate passive Schur reduction plus numerical Fourier integration of the exponential-current ratio; DC feedthrough and second/third sidebands have explicit expectations, alongside dry/partial/wet connection checks.

The bounded alias check compiles a small probe against the current native library. At the maximum220Hz carrier it compares the actual192kHz kernel with an independent3.072MHz exponential-current reference for a common steady physical input, alone and multiplied by440/15000Hz sine signals. It reports the0–24kHz periodic error and the reference energy above24kHz before decimation. This is not a full-synth/transient alias bound or a claim that above-band energy is absent; the existing FIR/output path is excluded from this local check. No audio file or hardware recording is fitted.

The parent checker recompiles the immutable passive parent and compares768,000 dry samples at four rates, including active carrier/envelope/control histories with the mix bypassed. Native/embedded parity additionally exercises the active wet path, and the existing allocation contract checks all57 parameters. Temporary probe binaries/raw float streams are removed; their source, exact commands and hashes remain reproducible. The prior passive validation files are historical and unchanged.

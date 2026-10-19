# Independent review of physical carrier switching

19 October 2026. Bounded source, code, test-method and stored-evidence review of the isolated ring-switching branch, whose immutable parent is `8c5a96e75f426dd7854cfee8e76cccd9478bbbfe`. Exact reviewed files and final implementation evidence are bound by the adjacent `implementation-review.json`. No reviewer DSP edits, audio rendering, recording measurements, parameter fitting or additional public retrievals were performed.

**Result: no implementation blocker found within the declared nominal reduction.** This is not a measurement of the populated hardware's operating point, an absolute wet-gain calibration, or a claim of improved public-recording accuracy.

## Physical coordinates and connection

The new production expression is `tanh((DC + 1.5 * passiveCarrier) / (2 * VT))`. The coefficient `1.5` converts the existing unit-peak source response to the Yamaha guide's nominal 3 Vpp source; it is applied once. The thermal denominator follows matched BJT collector-current ratios at the declared 298.15 K. The original Fairchild upper switching pairs have no emitter degeneration, so lower signal-pair and bias-mirror resistors do not belong in that denominator.

The implementation's independent series/parallel DC reduction includes the centered balance trim's two `470 + 125000` ohm returns. It agrees with the source audit's six-node unloaded external network. The negative differential remains in the product: zero AC gives approximately −0.427738 steering, rather than a silently centered zero. The bus is multiplied by this factor before the existing wet-output coupling; its signal-port and downstream gain conventions are retained. No rate-dependent normalization, peak division, mean subtraction or recording-derived coefficient was added.

The external DC approximation omits **all IC currents**, including mirror and collector loads. It is not merely a zero-base-current assumption or a self-consistent complete transistor bias solution. The source is ideal, the upper pairs are matched and forward-active, and temperature is fixed. Actual loaded source amplitude, signal-port transfer, finite beta, mismatch, saturation and wet trim remain unresolved. The code and implementation documentation now distinguish those limits explicitly.

## State and bypass

The previous raw sine and passive capacitor paths are unchanged. The new steering is computed after their update, including while ring mix is zero. Existing note and patch operations do not reset this instantaneous law or its input histories. Reset and explicit software panic use the passive parent's existing lifecycle; a cleared AC network returns steering to the nominal DC value, not artificial zero.

The explicit mix-zero branch retains the parent's dry arithmetic. The appended diagnostic reports steering separately from the unchanged raw sine and passive unit-source response. The native-only voltage probe accepts the total differential in volts and deliberately does not add source amplitude or DC again. The diagnostic interface remains outside the embedded ABI.

## Independent tests and retained results

The new unit tests use full six-node Gaussian elimination with KCL checks, rather than copying the production resistor reduction. Their matched-pair oracle uses exponential collector currents rather than the production `tanh` expression. Voltage points, a small-signal derivative, finite limits, biased zero-AC behavior and production wiring test the physical units and factor of two. Lifecycle checks verify continuous carrier evolution. A slow-rate check retains a specific consequence of this nominal bias: at 0.2 Hz the AC swing is too small to reverse the negative steering. That is a conditional model prediction, not measured hardware leakage.

The previously ideal-product audio assertion was replaced with explicit expectations from a separate passive Schur reduction and Fourier integration of the exponential-current law. It checks the retained dry fundamental feedthrough and second/third carrier sidebands as well as the primary sum/difference pair. It was not replaced with an arbitrary relaxed suppression threshold.

The stored release suite passes 9/9 and the targeted ASan/UBSan suite 4/4. The immutable-parent comparison reports 768,000 exactly matching float32 dry samples across the default and active-carrier-bypassed scenes, at four host rates. Existing native/embedded parity covers active wet processing. These are inspected producer results with verified file bindings; this review does not claim a second independent execution of those tests.

## Bounded sampling evidence

The new native probe compares the actual production steering kernel at 192 kHz with independent exponential-current evaluation at 3.072 MHz, using the same steady 220 Hz analog carrier input. It examines carrier-only output and products with 440 Hz and 15 kHz sine signals. The largest reported 0–24 kHz coefficient-band RMS difference is `4.751823758821833e-14`; the native-versus-current-ratio pointwise difference is at most `2.220446049250313e-16`.

Above-24 kHz energy is explicitly reported, including approximately 0.011742 RMS for the 15 kHz product before decimation. Thus this test does not establish that the new stage is band-limited, validate the existing FIR's complete rejection, or bound arbitrary carrier transients and full-instrument nonlinearities. It isolates periodic sampling of this new nonlinear kernel for fixed inputs.

The separately retained root-authored `switch_alias_check.py` and `switch-alias-result.json` provide a constant-only 192 kHz versus 1.536 MHz check of a steady 220 Hz carrier times 440 Hz sine. Its maximum coefficient difference is `4.709966188050835e-16`. It is supplemental analytical sampling evidence, not a native implementation or hardware-recording test. It was added alongside, and did not modify, the previously frozen source audit and acquisition manifest.

## Remaining validation boundary

Fixed-setting audio comparisons can establish the change's audible consequence without gain normalization or parameter fitting. Neither this review nor the synthetic test suite identifies the unknown populated-circuit DC loads or proves that the nominal model matches a particular CS-50 recording more closely. Those limits remain attached to the implementation instead of being hidden by an empirically adjusted drive scale or removal of the predicted feedthrough.

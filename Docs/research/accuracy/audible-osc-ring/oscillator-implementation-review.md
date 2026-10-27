# Independent oscillator implementation review

19 October 2026. Bounded, read-only review of `/tmp/ceres-vco-conditioning-20261019` against parent `d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43`. **Pass for the declared conditioned-output reduction.** No remaining mathematical or implementation blocker was found. This reviewer did not build, run the oscillator, decode audio, measure a recording or fit parameters. Exact reviewed file identities are in `oscillator-implementation-review.json`.

Reviewed CPP SHA-256: `adb699e99a3cf70e07f37f63cf2ea2eba28652d46a7a24d51db5d53d8f8ec070`. HPP: `8554b963f92c02930ef433c27a2239e3fa07e0425141a12171506b9a74d2d5f3`. New test: `0be7127f3dbe1acb0c8039ba16b9bc0c4d5b07e9abbe8c043c8b5a42354198a3`.

## Source and physical scope

The nominal duration `1/(523.2*40k*800uA)` follows the documented Normal frequency/current relation and selected resistor, rather than an unsupported interpretation of the CT RC product. Extending the engineering setter ceiling to60 µs permits that value. The CT threshold, reference-current tolerance and actual trim remain unknown. The source supports distinguishing the conditioned OUT7 signal from the raw integrating capacitor, but does not uniquely specify a perfectly flat clamp, a2r step or an exact WSC sine transfer. Those assumptions are explicitly retained in the implementation documentation.

The older `vco-conditioning-followup.md` and current source review were checked. The earlier recorded candidate's modest magnitude regressions are preserved in the new README, including the fact that its sine still bypassed the conditioned ramp. This implementation adds the same-ramp sine fold; it does not demonstrate improved recording agreement merely by changing topology. No prior adverse result is displaced by the source inference.

The phase model `r=f*tau` is a constant-frequency reduction. It does not retain a separate clocked one-shot state during rapid frequency/reset-control changes. Clamping r to0.35 abandons fixed tau above approximately5.86 kHz at the nominal duration. Both are now explicit limitations. At deep PWM, duty can be less than r; the retained independent fixed-edge pulse is then an engineering WSC approximation rather than a literal comparator of the flat plateau. Its source-supported fixed leading edge is preserved without pretending that the hidden compensation circuit is known.

## Independent algebra check

For audio saw `s(phi)=-1` before r and `2phi-1` afterward, the value jumps are−2 at wrap and+2r at r. The slope jumps are−2 at wrap and+2 at r. With the existing BLEP/BLAMP definitions, half-jump and half-slope-jump coefficients give exactly

```
s_corrected = s - BLEP(phi) + r*BLEP(phi-r)
                - BLAMP(phi) + BLAMP(phi-r).
```

For the chosen same-ramp sine fold, the plateau is1 and the descending portion is `cos(2*pi*phi)`. There is no value or first-derivative event at wrap. At r, value jump is `cos(2*pi*r)-1`, and derivative jump is `-2*pi*sin(2*pi*r)`. Thus the implemented correction coefficients `.5*(cos-1)` and `-pi*sin` have the correct sign and scale. The ideal fold identity follows directly from `sin(pi*(2*abs(p)-1)/2)`; this is not an independent free-running sine phase.

The new Fourier oracle integrates the piecewise functions independently of the sample generator. Its saw coefficient is the full ramp integral minus the integral of2phi over[0,r]. Its sine coefficient is the plateau integral plus the two shifted exponential integrals of the cosine branch. For the edge terms, multiplying the singular Fourier components by the triangular-kernel transfer `sinc(pi*n*step)^2` yields the same BLEP/BLAMP signs. Summing coefficients at n+kN correctly accounts for the sampled DFT's aliases, so residual high-frequency curvature is not mistaken for an edge-sign defect. The oracle does not claim that the sine becomes perfectly bandlimited; its curvature discontinuity is still present.

The continuous means are−r² for saw and `r-sin(2*pi*r)/(2*pi)` for the uncorrected sine. The finite BLAMP area adds `slope*step²/12` to the sine mean, consistent with the test. Physical downstream coupling remains responsible for DC response; no manual DC cancellation or normalization was added.

## Tests and bounded conclusions

The test checks16 harmonics at two exact-period probes, DC, plateau/interior values, the current-derived ramp slope, nominal timing acceptance, pulse invariance and extreme frequency/rate finiteness. The new deep-PWM case starts with duty0.1 below r≈0.210 at3520 Hz and compares its pulse exactly against a0.1 µs reset case. The surrounding bounds loop also uses this deep PWM configuration. This explicitly exercises the declared pulse policy where a literal plateau comparator would not apply.

The prior generic PWM assertion was changed from inspecting the old end-of-period recharge to inspecting the new reset interval. That adapts the test to the intentional topology change; it does not weaken fixed-edge or duty invariants. The sum-of-squares message now says the waveform is affected, rather than claiming the small numerical threshold proves audibility.

The stored final Release log reports **8/8 tests passed**, and the selected ASan/UBSan log reports **3/3 passed**. These logs were read and hash-bound; they were not independently rerun by this reviewer. Exact unit-test agreement certifies the chosen equations at their tested rates and cases, not the hidden Yamaha transfer, perfect antialiasing under all modulation, or perceptual similarity.

Actual audible and regression evidence must come from fixed, unnormalized old/new scenes and the retained recording comparisons. A pure-pulse negative control is particularly useful because its clock/duty equation is unchanged. Do not compensate the new saw/sine with a fitted gain or phase while describing that comparison as an isolated circuit change.

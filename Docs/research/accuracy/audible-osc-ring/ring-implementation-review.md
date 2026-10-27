# Independent ring carrier implementation review

19 October 2026. Bounded read-only review of `/tmp/ceres-ring-carrier-20261019`, based on the corrected-SUB parent `d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43`. No material implementation defect found. No reviewer DSP edits, audio decoding, native rendering, model evaluation or repeated test execution.

The frozen [JSON review](ring-implementation-review.json) records the inspected source/test/report identities and independently verified file bindings. Production cpp is `3472f2667c232409a9fe824580c273f9a1e3ddc46d49f26b05ef1d351996605a`; header is `bd785967fea79ed0df68bc0fb70f6c063c44439eac975c96357eb71bcdcaa10f`; new test is `913e2b8e37e0cf2e08afe22069db462529d4b88aa2a095d4bf60dc60f8aa4bf9`. Final author validation is `93109a7916ee46d47797266f685699e1a0434503330f63e21204c83e092ab7fd`; README is `2a4df42f9f4fc83fdf6b7a9e65cf79289268787d2cf7fef1af26d94215c74fce`.

## Circuit and numerical implementation

All six resistor stamps match the independently traced four-node reduction: pin7-ground33k, pin7-pin8 100Ω, pin8-audio-bias820Ω, pin8-local-supply1.2k, audio-bias-ground1k, local-supply-ideal-rail390Ω. All four capacitors are100µF; the pin7 capacitor is connected to the carrier source, rather than ground. Output correctly uses pin7 minus pin8.

For physical nodes `v=(x,b,a,s)`, raw source u and capacitor coordinates `w=v−e0*u`, the source review gives `C*w' = −G*v`. Production's precomputed matrix `M=(I+G/(2fsC))^-1` and updates `v=M*(memory+e0*u)`, `memory=2*(v−e0*u)−memory` are the equivalent trapezoidal companion equations. The stored companion incorporates previous input history; the absence of a separate previous-u variable is not an error. Factorization occurs at construction. Sample processing uses fixed arrays without allocation, matrix inversion or exceptions. The output is not renormalized by carrier frequency.

The author tests independently construct the complex-frequency physical nodal equations, using the trapezoid's bilinear frequency map, and compare native impulse Fourier coefficients in amplitude and phase. A separate transient oracle retains physical w and previous u, solving simultaneous trapezoids and checking KCL. These are meaningful independent formulations, rather than tests copying the companion update. The production lifecycle test then verifies that the exact kernel is actually connected in the synth.

## State and compatibility

The carrier network advances every internal sample, before the wet/dry mix. Thus mix-zero keeps capacitor history warm while the explicit zero-mix branch preserves dry arithmetic. Ordinary note-on/off, pedal, allNotesOff, preset and live rate edits do not clear carrier charge. The pulse/key ledger and ring-frequency envelope remain unchanged. The diagnostic keeps raw `ringCarrier` semantics and appends `ringCoupledCarrier`; both it and the allocating offline probe remain native-only. No Patch parameter is added or reinterpreted.

Explicit reset clears oscillator phase and charge. Software panic clears carrier charge together with the existing wet coupling while preserving the previously free-running phase. That can create a new charging transient if sound resumes immediately; it is the parent's deliberate panic policy, not a claim about the powered hardware or physical key release. The README now says so. The tests verify the next processing segment after panic, not merely the cleared diagnostic value.

Retained logs show8/8 Release and3/3 targeted ASan/UBSan tests passing, with leak detection disabled as documented. The parent checker reports768,000 byte-identical dry samples in two scenes, including active carrier/depth controls at mix zero. This reviewer verified the report's source identities against the actual parent Git objects, output-hash equalities and sample-count sum. The checker deletes temporary comparison binaries/streams, so their original hashes and reproducible commands are retained rather than the transient files. Native/embedded active-ring parity and allocation checks are in the existing suite. These results are implementation evidence, not hardware fidelity measurements.

## Required interpretation

This is an **ideal-source, zero-incremental-IC-current passive model** around the existing normalized ideal multiplier. Unknown source resistance, carrier-input current and transistor/common-mode behavior remain. The finite seven-node comparison in the source audit supports the chosen external-path simplification at its declared frequencies and balance trims; it is not a universal bound.

Most importantly, **passive differential carrier attenuation is not calibrated wet-output attenuation**. Unknown physical drive and µA796 BJT switching/compression may partly restore low-speed wet gain. Neither approximately31.8Hz nor the calculated−38dB at0.2Hz is established as a hardware wet-output response. No signal-port200k value is substituted for carrier impedance, and no guessed normalized tanh scale is introduced. The new path implements an audible omitted component network while leaving that nonlinear calibration problem explicit.

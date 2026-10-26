# Audible circuit corrections - 19 October 2026

Intermediate checkpoint before physical carrier switching and final recording evaluation. Superseded by [the completed result](RESULT.md); details below preserve that earlier state.

The standalone implementation is in `/tmp/ceres-audible-integrated-20261019`. It combines source-timed conditioned VCO/WSC input, independent HP/LP resonance-capacitor anchors, idle filter-envelope charge continuity and the ring carrier's four-capacitor differential network. These are circuit reductions with the specific limits below; no sample assets, GUI, Rack package, installation or parameter fitting was added.

The first integrated snapshot passes11 Release contracts and5 targeted ASan/UBSan contracts, including all three new circuit groups, embedded allocation and live native/embedded parity. Component reports are historical evidence bound to their isolated commits. Their parent-comparison recorder scripts should be run in those isolated source branches, not treated as parent-parity assertions for the combined oscillator-changing model.

## Source-backed changes

- Q controls: runtime HP/LP resonance changes now charge their1.1s capacitor coordinates instead of applying instantaneously. This is the existing stiff-source/zero-chip-current limit, with the previous settled Q mapping retained. Pre-clock configuration remains settled; live patch changes preserve charge.
- Filter envelope: a live IL change after the envelope becomes idle now retains charge and returns using the existing release law. The guide supports continuity but contains ambiguous idle-gm wording; the post-idle rate is a minimal model continuation rather than newly measured timing.
- Oscillator: the published current/frequency calibration implies nominal59.7286us, replacing6us. The exposed saw is modeled as conditioned plateau/step, not the raw integrating-capacitor recharge. The same waveform feeds the ideal WSC sine fold; both derived waveforms receive analytic edge corrections. Exact clamp/slice behavior and fast-FM monostable dynamics remain approximate. PWM overlapping reset retains an independent documented fixed-edge approximation.
- Ring carrier: four100uF capacitors and the100ohm differential pin7/8 network now precede the existing normalized ideal product. This passive limit is independently verified; it does not predict the populated chip's nonlinear wet-output response. A newly acquired original Fairchild transistor schematic is being evaluated before extending the switching stage.

## Fixed-gain listening comparisons

All demos were declared before rendering, use matched source code and the same driver per comparison, contain no normalization/fitting, and retain commands, source/binary identities and source WAV hashes. The three comparisons play parent then candidate for each scene, with0.25s gaps.

- [Q and envelope comparison](capacitor-run-001/comparison.wav),14.65s: LP-Q step, HP-Q step, then a note after an idle IL edit. In the first20-120ms after the Q edit, candidate RMS is2.084dB lower for LP and6.267dB lower for HP; those differences decay as the capacitor settles. In the first50ms of the note after the IL edit, RMS differs by0.819dB and the waveform-difference RMS is6.23dB below the parent signal. Both static controls are byte-identical. This is an audible transient comparison, not a hardware accuracy score.
- [VCO/WSC comparison](oscillator-run-001/comparison.wav),16.75s: saw ascending through440/880/1760/3520Hz, parent then candidate, followed by the same sine sequence. Separate pulse negative controls are byte-identical. Candidate sine THD below20kHz is-58.63/-43.81/-29.50/-16.92dB; the old ideal sine is about-73.63dB. These quantify the chosen reset-fold approximation, not measured original sine distortion.
- [Passive ring comparison](ring-run-001/comparison.wav),13.25s:1/30/220Hz carriers with a440Hz source. The measured candidate/parent RMS ratios .04768/.67083/.98862 agree with passive differential-carrier predictions .04766/.67065/.98864. **These are not calibrated hardware wet-output ratios:** the original chip's switching/compression, source impedance and gain trim still matter. This comparison intentionally freezes the older ideal-product approximation so the newly added passive network remains isolated.

The retained earlier12-render oscillator experiment found conditioned59us close to, but slightly worse than, its6us effective-patch baseline. Raw59us was substantially worse. Those adverse results remain intact. No claim of improved public-recording fidelity follows from adding topology or from passing native tests. A combined-model frozen recording evaluation is still outstanding.

## Evidence

Plans: `capacitor-plan.json`, `oscillator-plan.json`, `ring-plan.json`. Each binds its exact driver. Each `*-run-001/manifest.json` binds the compiled sources, library and driver binaries and all native WAVs. Analysis scripts verify the retained hashes before computing results. `analysis.json` and `listening.json` retain measurement windows and exact listening segment boundaries. The primary source and independent implementation reviews are in the adjacent `filter-eg-capacitor-continuity/` and `audible-osc-ring/` directories.

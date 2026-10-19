# Why isolate effective filter release

This review precedes the eight native audio calls. It finds no source-supported sign or target bug in the current envelope engine. The next useful separation is **filter release versus amplifier release**, with the existing controls and reconstructed gates held fixed. It is a diagnostic, not a recovered timing law.

The direct [CS-50 service description](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%201%20of%202%20%28%208.5%20x%2011%20-%20Double%20Sided%20%29.pdf), PDF 10 / printed 20, specifies IL → AL → zero sustain → IL. The current `Envelope::off/advance` preserves capacitor state at key-off and returns to the same negative initial target. A free negative held-decay target would contradict grounded SL pin 15. The published attack range is 1 ms–1 s versus 10 ms–10 s for decay/release, so it still supports the existing nominal decade. No new evidence makes a .1 factor the true default.

The direct [M-board schematic](https://bama.edebris.com/download/yamaha/cs-50/Yamaha%20CS-50%20Service%20Manual%202%20of%202%20%28%2011%20x%2017%20-%20Double%20Sided%20%29.pdf), PDF 12 / printed 16, has **separate** filter/amplifier IG00151 timing VCAs and 0.22 µF capacitors. Filter TC12 passes through 82 kΩ + VR19; amplifier TC12 passes through 82 kΩ + VR20. Their timing biases use VR15 / VR16. Each uses 100 kΩ buffered-state feedback. Within a given envelope, A/D/R share this path. Thus equal actual TC voltage and local error imply the same rate in the ideal shared-loop reduction, but the simplified sources do not identify selector loading or endpoint thresholds.

The SUB schematic, the same PDF 6 / printed 8, supplies RF and RA through separate diode/follower paths clamped by the common SUS source. Equal SUS does **not** prove equal physical filter/amplifier time constants. Conversely, changing a timing trim alone would affect A/D/R together: the experiment below must not be described as a literal VR19 adjustment or a physical common-SUS setting.

Current `effectiveRelease` at `Source/DSP/ceres.cpp:122` and coefficient construction at lines 559–565 use `max(selected,global)` for each envelope. The raw Brass I row sets both releases to 10 V, nominally 0.013815510558 s T60. The two saved cases instead have effective floors 0.436884804013 s and 0.218961086337 s. Lowering `filter_release` beneath either floor does nothing.

The exact engineering isolation is therefore:

```text
global_release' = 0
amp_release' = max(amp_release, global_release)
filter_release' = factor * max(filter_release, global_release)
factor = 1, .5, .25, .1
```

The amplifier release coefficient remains identical. Attack, decay, endpoints, Q, cutoff, common AM, original MIDI, observation masks and recording gain stay unchanged. Factor 1 must produce a byte-identical baseline; all factors must preserve samples before the first key-off. A different filter trajectory can still change measured fundamental amplitude despite identical amplifier state.

The bounded historical inventory covers 90 patches in seven relevant directories. Eight have explicit filter-release overrides: four move amplifier/filter together under nominal global-sustain choices and four slow filter release to 30 s. None isolates faster effective filter release while preserving amplifier timing. The completed D-only/Qmax grid and LP-envelope-span failures remain adverse context. At Q1 the stationary C4 contrast can be close while continuous histories fail; that does not establish Q1 or a timing correction. The current experiment retains factory Q and D, rather than following that stationary witness.

An independent closed-form check explains why this test is distinct. With unchanged D, history 20 reaches the previous negative stationary witness around 137.504 s at its current effective release, versus 137.339 s at .1 release. History10 moves from 137.510 s to 137.430 s; its late key-off still prevents a negative own-EG value at any of the four second-lobe centers 137.32–137.38 s. These are state-reachability facts, not predictions of windowed nonlinear spectra. No new gate history is chosen.

The exact primary images, code, previous studies, 90-patch inventory and numerical inputs are hash-bound in `source-evidence.json`. `plan.json` separately freezes the measurement protocol and the current renderer/compiled-source snapshot before audio. Public recordings and both gate histories remain reused development evidence without original hardware MIDI or measured control voltages. No default promotion follows from a favorable cell or aggregate score.

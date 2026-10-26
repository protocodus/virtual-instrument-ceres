# Live Q and idle filter-envelope capacitor continuity

19 October 2026. This isolated change starts from SUB-polarity-corrected parent `d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43`. It changes no oscillator equation, parameter key, preset value, cutoff mapping, envelope timing calibration, allocator or GUI. The embedded parameter contract remains 57 keys.

Two live-state bypasses are corrected:

* Both stage resonance anchors now traverse the existing pin7 RC response. Previously only `resonance_offset` traversed it; edits to `hp_resonance`, `lp_resonance` and preset recall jumped across the capacitor.
* An idle filter-envelope capacitor keeps its charge when `filter_initial` changes. It follows the existing release coefficient toward the new initial level, and a subsequent attack begins from the retained voltage. The software Idle stage remains Idle during this movement; it does not create an active amplitude voice.

## Source and scope

The reviewed source derivation is [filter/EG capacitor source audit](/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/filter-eg-capacitor-continuity/source-audit.md), SHA-256 `4febb599e7b01dfdfdf8276a4749c4454f58b0a044986ec71547f29807f84028`. Its primary references are Yamaha CS50 service part1 PDF8/printed14 (two 220k Q-source branches), part2 PDF12/printed16 (10uF pin7 capacitors and 0.22uF EG integration), Yamaha IC Guide PDF33/printed2-3 (EG target/rate switching), and CS Series Servicing Guide PDF52–53/printed43–44 (no-key IL/RT paths). The validation manifest binds these retained local PDFs.

Under the already declared stiff-source, zero-chip-input-current limit, `tau = 10uF*(220k||220k) = 1.1s`. (Since Ceres 0.6.5 pin 7 is taken as a 1.72k current input, so `tau = 10uF*(220k||220k||1.72k) = 16.9ms` and the 1.1s figures below read 16.9ms; the structure is unchanged. See [DSP source](../../research/dsp-source.md).) The existing normalized anchor is an effective calibration coordinate, not a measured raw preset voltage. Two smoothed anchor terms and the unchanged common delta represent a linear decomposition of two stage capacitor coordinates. There are physically four HP and four LP capacitors; this reduced symmetric model gives identical state to equal boards. It does not introduce three physical capacitors or a newly identified VQ-to-Q curve.

The old common-offset update remains literally unchanged. For each internal sample:

```
a = -expm1(-1/(internalFs*1.1))
offset += (0.5*resonance_offset - offset)*a
hpAnchor += (hp_resonance - hpAnchor)*a
lpAnchor += (lp_resonance - lpAnchor)*a
hpEffective = clamp(hpAnchor - 0.1*offset, 0, 1)
lpEffective = clamp(lpAnchor - 0.1*offset, 0, 1)
```

When anchors are fixed at their initialized values, their increments are exactly zero. The old global-offset audio path retains its previous arithmetic.

Idle filter EG now uses the same finite return already used during Release:

```
E_next = IL + (E_previous - IL)*exp(-ln(1000)/(internalFs*max(filter_release,global_release)))
```

The existing absolute `1e-7` terminal snap remains. Guide printed43 also calls the no-key transconductance maximum, while retaining the RT/IL switch path. Thus finite capacitor continuity is supported; extending the current release law after an idle IL edit is a declared minimal model continuation, not a newly measured idle time constant. No recording is used to choose a rate.

## Initialization and lifecycle

Before the first internally processed sample, `setPatch` precharges the two Q anchors and idle filter IL to the configured values. This retains the established offline settled-patch convention. The old common-offset initialization remains zero. Silent processing permanently closes this exception until an explicit `reset()`.

After processing starts, `setPatch` changes targets and coefficients without snapping Q or idle EG charge. Note events preserve the Q coordinates. `allSoundOff()` retains its explicit audio-panic behavior: it clears voice/audio capacitors and puts the filter EG at the current IL, but preserves Q charge and clock-start history. A following patch edit cannot silently reopen initialization. `reset()` restores seeded state and the current configured anchors/IL, clears the historical common delta, and permits pre-clock configuration again.

Read-only native `SoundPathState` adds `hpResonanceAnchor`, `lpResonanceAnchor` and `hasProcessed`. Those diagnostics remain excluded from the embedded ABI; no runtime allocation or exceptions are added.

## Verification and reproduction

Run from this worktree:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
python3 Tests/check_capacitor_parent_parity.py --build-dir build --report Docs/capacitor-continuity/parent-parity.json
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON
cmake --build build-sanitize --target ceres_capacitor_tests ceres_sound_path_tests ceres_embedded_contract -j 4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure -R 'capacitor_continuity_contracts|sound_path_circuit_contracts|embedded_allocation_contract'
python3 Tests/record_capacitor_validation.py
```

`CapacitorContinuityTests.cpp` independently computes the combined affine Q-coordinate KCL and closed-form 1.1s steps/reversals at 8/48/192kHz host rates; checks independent stages, common-offset superposition, audible-filter connection, live preset recall, panic and reset. Its EG checks cover every idle board, global-release selection, target reversal, retained-charge attack, and a previously sounding board that has reached numerical Idle. These are conditional model/circuit checks, not new absolute voltage calibration.

The existing Release suite also checks the native DSP, sound path, per-voice expression, zero post-construction embedded allocation and native/embedded audio parity. A new live-Q/IL driver compares 384,000 exact native/embedded float samples across four rates and eight scenes.

`check_capacitor_parent_parity.py` recompiles the immutable parent and checks **882,688 byte-identical float samples**: two 384,000-sample manual/control sequences (including a changing common resonance offset with static anchors/IL) and 114,688 samples covering all 14 `presetNames()` entries at two rates. Notes, pressure, bend, pedal, release, panic and reset are exercised. Temporary comparison binaries/float streams are deleted; the committed drivers, commands and hashes reproduce the check. No public recording-linked render, audio fit or hardware-accuracy score is part of this validation.

The resulting live resonance/IL transients intentionally differ from the parent. Static-patch parity does not claim that dynamic edits are identical, nor that finite source impedance, actual chip input current or exact idle-rate behavior has been identified. Controlled listening demonstrations are a separate parent-owned task.

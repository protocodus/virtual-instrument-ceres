# Shared SUB modulation polarity candidate

18 October 2026. Isolated DSP worktree based on `55869df8ba728e25a6b5792aa07dfd55f134439c`. No canonical checkout, Rack source, factory controls or recording fit changed. This is a circuit-sign correction, not a measured improvement against the public recordings.

## Evidence and change

Direct CS-50 PN3 (large-sheet service manual PDF21/printed27) connects both VCF SVR3 and VCA SVR4 to the same buffered SUO endpoint and ground. R7 (small-sheet manual PDF8/printed14) sends the VCF wiper to HP/LP through positive 150k/47k resistors and the VCA wiper L1 to LC through120k. The M sheet (large-sheet PDF12/printed16) sends LC to IC5 linear gain input LI2. The Servicing Guide PDF49–50/printed40–41 states that increasing pin2 control increases signal level, with zero at0V and maximum at10V. These paths contain no relative inversion. A final audio-output inversion does not reverse the gain-versus-cutoff relation.

The cached original sources are bound by SHA256:

| Source | SHA256 |
|---|---|
|`/tmp/ceres-cs50-1x8LGd/research/service2.pdf`|`1102e55b949378e7776555bca1a4d73105ca2d76d6fcf3aa4c2b103566201cfc`|
|`/tmp/ceres-cs50-1x8LGd/research/service1.pdf`|`c3a4b0d4e27e9af90c88258d03a1c09c5d080ea162e562d9bf267fc8ffbbadc1`|
|`/tmp/ceres-cs50-1x8LGd/research/cs-servicing-guide.pdf`|`2738f7ddc46f0bedbab38dcad8e2c112c41b3e6ca34670d471d0cb8c0dbde633`|
|`/tmp/ceres-cs50-1x8LGd/research/pitch-modulation-pn3-routing.png`|`767235e24ffb5b01e862b164965ab8074665b50d4ab8c77fe560a7658d3532d0`|
|`/tmp/ceres-cs50-1x8LGd/research/control-review-vca-path.png`|`6f9857765efb600373f7967d8a865e2fcf7b2c71dacf774e7c090cbb5f9fce5a`|

The only production equation change is:

```cpp
// Before: positive SUB lowers common gain.
max(0, ampLevel - tremolo*(.5 + .5*SUB))
// Candidate: positive SUB raises common gain and both filter cutoffs.
max(0, ampLevel - tremolo*(.5 - .5*SUB))
```

The unclipped midpoint `ampLevel-tremolo/2`, excursion `tremolo/2` and closure boundary are retained. Absolute depth/current scaling, the negative midpoint offset and ideal clipping remain engineering approximations. For zero-mean SUB the unclipped mean is unchanged; this does not promise identical finite-window means or identical noise samples after clipping. The source clock, oscillator waveforms, filters, envelopes and allocations are unchanged. With `tremolo=0`, the equation retains exact previous arithmetic. With nonzero tremolo, old samples generally change; for sinusoidal pure AM the waveform shifts by half a cycle. That is an intentional relative-polarity change, not a newly calibrated phase of any recording.

The historical `sound-path-gap-audit.md` preserved the old AM formula for compatibility. Its chosen sign is superseded by the direct relative-routing evidence above; its loading/depth qualifications remain applicable.

## Checks performed

- All7 Release CTest checks passed (23.96s), including native/embedded audio parity, active sound-path parity, expression parity and the existing576×64-frame zero-allocation contract.
- All7 ASan/UBSan CTest checks passed (109.26s), with `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`.
- The new production-state regression exercises sine/down/up/square/noise. Every source visits both signs; positive values raise actual HP and LP variable-gm frequency and common gain, negative values lower all three. The nonclipped gain midpoint/depth are checked separately from closure.
- Note assignment, note-off, pedal, all-notes-off and panic are compared with an uninterrupted reference source clock. Explicit reset reproduces a fresh synth. The square closure test checks that output-capacitor tails continue after gain closes; panic clears those tails without restarting SUB.
- A fresh build of the exact baseline source and candidate, using the existing parity driver with **only tremolo changed from.71 to0**, produced byte-identical384000-sample float32 streams. The four rates are44.1/48/96/192k, eight scenes each; active filter/vibrato SUB, all five waveforms, ring, noise, expression, live edits, steals, pedal and reset remain active. Audio SHA256: `8c6f8bc0bc337c29c60b2e5a04a1ed98b6ca529791fde1f79fe101e4ab67d4cc` (1536000bytes each). This is a direct old-core comparison, not merely two candidate paths.

No public-recording depth, phase, gate or parameter was fitted here. The frozen eight-case recording battery also completed. All six zero-tremolo cases remain byte-identical. At the unchanged initial SUB phase, Brass six-cell MAE changes from3.510/3.514 to3.626/3.635dB and amplitude tails worsen, as expected for a half-cycle pure-AM phase reversal. A separate research-only reset-phase+.5cycle control recovers seven baseline WAVs byte-for-byte; history20 differs by at most1.86264515e-9 per sample, with its harmonic score changed by less than3e-10dB. No gates, gains, depths or timing were fitted. This algebraic phase control does not recover a hardware clock phase, validate the relative routes against a known-settings recording, or establish improved raw factory mapping. Frozen reports live in the parent checkout under `Docs/research/accuracy/runs/sub-polarity-phase0` and `sub-polarity-phasepi`.

## Candidate identity and logs

- `Source/DSP/ceres.cpp`: `cc2ee3cdcf7dfab7ebc60071627a2557520f6cdbadd2a590796629a9aad8fd10`
- Unchanged `Source/DSP/ceres.hpp`: `577cbd5909705d7db3a1522edbbfc71ec056d03751bbb97390413cf8cd6375f8`
- `Tests/SoundPathTests.cpp`: `151351db9ba0d8499a17dcaea1321024c01ca1a59df14a1edce80ea6a0efecbb`
- Release `build/ceres_render`: `42640ded137b2bf9c2a821db7ccf6db7cd7874cdfd75c26b9934d1c438d905c6`; its embedded build-info agrees with these source files.
- `/tmp/ceres-sub-polarity-release-tests.log`: `3117c4d235134b3f49b5ff9057d24c5927e8f5e81e4135bfbdef7a6da5ad39ce`
- `/tmp/ceres-sub-polarity-sanitize-tests.log`: `a7d8b0f1bdc51d5008df8656d942b4381991f8491fe6c09fa828426759ff4700`
- `build/zero-tremolo/report.json`: `e59780ba74e5fd65d6c12c11031a0aac62565817023f4a758dbbb7bac5616e46`

Build outputs and detailed temporary logs remain outside versioned source. The fix is retained on the separate `codex/ceres-sub-polarity-20261018` branch; the Rack Extension remains pinned to its separately validated source revision.

## Reproduce

Run from the detached worktree root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DCERES_SANITIZE=ON
cmake --build build-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
```

For the exact baseline comparison, save the following as `build/check_zero_tremolo.py` and run `python3 build/check_zero_tremolo.py`. It exports the frozen source from Git and does not edit any tracked test or production file:

```python
import hashlib,json,pathlib,subprocess
root=pathlib.Path.cwd();out=root/'build/zero-tremolo';out.mkdir(exist_ok=True)
base='55869df8ba728e25a6b5792aa07dfd55f134439c'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
driver=(root/'Tests/ParityDriver.cpp').read_text()
assert driver.count('patch.tremolo=.71;')==1
driver=driver.replace('patch.tremolo=.71;','patch.tremolo=0;')
harness=out/'ParityDriver.cpp';harness.write_text(driver)
variants={};reference=None
for label in ('baseline','candidate'):
 source=out/label;source.mkdir(exist_ok=True)
 for name in ('ceres.cpp','ceres.hpp'):
  rel='Source/DSP/'+name
  data=subprocess.check_output(['git','show',base+':'+rel]) if label=='baseline' else (root/rel).read_bytes()
  (source/name).write_bytes(data)
 binary=source/'parity';audio=source/'audio.f32'
 cmd=['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(source),str(harness),str(source/'ceres.cpp'),'-o',str(binary)]
 subprocess.run(cmd,check=True);subprocess.run([str(binary),str(audio),'expression'],check=True)
 data=audio.read_bytes();assert len(data)==384000*4
 if reference is None:reference=data
 else:assert data==reference,'Candidate differs at zero tremolo'
 variants[label]={'source_sha256':{n:sha(source/n) for n in ('ceres.cpp','ceres.hpp')},'compile_command':cmd,'binary_sha256':sha(binary),'audio_sha256':sha(audio),'audio_bytes':len(data)}
r={'status':'pass','baseline_commit':base,'scope':'Exact old-versus-candidate float32 audio, existing ParityDriver expression mode with only tremolo .71 replaced by0; four rates44100/48000/96000/192000, eight scenes,384000 samples. Active SUB filter/vibrato, allfive SUB waves, ring, noise, expression, edits, stolen/repeated notes, pedal, releases, panic/reset remain exercised.','driver_sha256':sha(harness),'original_driver_sha256':sha(root/'Tests/ParityDriver.cpp'),'script_sha256':sha(pathlib.Path(__file__)),'variants':variants}
(out/'report.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
```

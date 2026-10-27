# Flute key and gate history: independent source review

19 October 2026. **The retained source supports recent keyboard activity before both excerpts, but not the benchmark's exact gates, a one-voice census, fresh capacitor state or physical voice assignment.** The 225 ms / 179.167 ms ages are explicitly synthetic history choices. This review inspected existing stills and retained source-analysis metadata only; it performed no new audio measurement, source decoding, model evaluation, native render or fit. No additional frame extraction was necessary.

The reference is Andy Whitmore's [original CS-50 demonstration](https://www.youtube.com/watch?v=Fj21t8MIS_o). The previous Brass-II-to-Flute tablet audit was read as established context, not repeated or used to infer note-on times. The preceding pitch-route experiment remains a bounded, modest sensitivity result; neither its better dark-network case nor the stopped brightness-fit callback recovers hardware history.

## Independently visible observations

Times below are the retained frame labels/requested seek times. The source is 25 fps, so ordinary frame sampling is 40 ms. These are not independently calibrated audio-synchronous contact timestamps.

| Region | What the inspected frames show | What remains unresolved |
|---|---|---|
| 203.00–204.90 | Both hands occupy several keyboard regions in the retained contact sheet. | Complete depressed-key set, electrical gates and channel assignments. |
| 205.05 / 205.20 | Hands withdraw; the full 205.20 frame shows the keybed clear of hand contact. | Pedal state and residual EG/filter/output capacitor charge. Hand lift is not an electrical reset. |
| 205.30–205.56 | Right hand returns to the upper register; contact is visible by the 205.40–205.50 views. It is near the upper black/white key group in the full 205.56 frame. | Which of the covered keys first closes, exact contact time, and whether the later target is the same physical board. |
| 205.60–205.80 | Right-hand contact changes position. The left hand also returns to a lower keyboard region. The high ordinal crop supports contact at white index23/E with adjacent fingers covering other keys. | Additional pressed notes, overlapping gates, pressure, or an exclusively single sounding voice. |
| 207.88–208.04 | Both hands change position around the end of the high held passage. | An exact high-note release event or which EG(s) change stage. |
| 208.15–210.52 | Retained frames show a continuing, changing keyboard passage. | Its complete note/overlap schedule and resulting per-board capacitor histories. |
| 210.56–210.72 | Right-hand fingering moves again; the left hand is away from the keybed. The lower ordinal crop at210.90 supports contact at white index21/C. | A new isolated gate from silence, precise gate timing, residual prior notes and physical board choice. |

The already audited four-octave key geometry plus the visible 1 OCT DOWN state support physical normal-note MIDI88 for the high E-like target and MIDI84 for the lower C-like target, with `octave=-1`. These assignments use visible contact region, ordinal geometry and the previously established sounding pitches together; the annotated ordinals are counting aids, not depressed-key detectors. They do not establish that all other fingers/voices are inactive.

The existing frame inventory also contains duplicate image bytes under nearby requested labels: 205.50/205.52, 205.65/205.68 and 210.50/210.52. This is consistent with the25fps sampling and is an additional reason not to claim20ms or finer independent visual gate observations. A/V synchronization remains unmeasured. More crops of these same images would not expose hidden contacts or pedal state.

## Retained acoustic constraints, not newly measured here

`calibration/whitmore-flute-history-boundaries.json` records a quiet-output interval205.20–205.44, a new transient205.455–205.465, an approximately628–630Hz precursor during205.48–205.56, and the E-like667–670Hz ladder arriving around205.60–205.64. Its10ms centered-RMS crossings explicitly include ±5ms temporal support and describe output amplitude, not a gate. The high205.75 excerpt therefore begins about0.29s after the first transient but only0.11–0.15s after the reported target-ladder arrival. A precursor followed by a separate key, same-board pitch transition, and other allocation histories are not distinguished by that evidence.

For the lower excerpt, the same retained analysis records a rapid preceding passage and a395→525Hz transition around210.58–210.66, with no clean all-sound silence immediately before the target. The210.80 start is0.14–0.22s after this pitch transition. Its later disappearance/next transition is not an isolated release-to-silence observation. A single dominant harmonic ladder is insufficient to count physical voices, especially if other boards are quiet, releasing, masked or at related pitches.

These old output observations constrain simple uninterrupted-held-note stories. They do not convert silence into zero initial voltage, prove simultaneous zero input to every EG, or identify whether an observed output transition retriggered the particular physical board carrying the later target. Pressure, sustain/pedal, modulation and recording-chain contributions remain possible confounds.

## Exact origin of the frozen ages

The earlier source-bounded sensitivity plan declared high ages `[0.15, 0.225, 0.30]` and lower ages `[0.14, 0.18, 0.22]` seconds. Its stated construction was **one fresh voice with initially empty states**, first excerpt bend held beforehand, and no reconstructed precursor, prior voices, pressure, pedal or glide. It explicitly reported all six choices rather than selecting a recovered gate.

`tools/profile_whitmore_fresh_history.py:75` quantizes each age as `round(age_requested*480)/480` at60BPM/480PPQ. Thus225ms stays225ms (108 ticks/10,800 samples at48kHz), while180ms becomes179.1666667ms (86 ticks/8,600 samples). `tools/benchmark_whitmore_transpose.py:104` deliberately selects those two middle histories and preserves their ages while correcting physical note/transpose coordinates. The current battery inherited them; they were not re-estimated by the pitch-route study.

Aligning those synthetic note-on-at-zero histories to the excerpt starts implies205.525s and210.6208333s, respectively. The first lies between the reported initial precursor and later target arrival; the second lies inside the reported lower transition interval. Neither arithmetic coincidence supplies an identified electrical gate. The high225ms history was also the declared training condition for an earlier effective D/S fit, so that fit cannot independently validate its assumed onset.

## Consequence for the next action

Preserve these histories as explicit reproducible approximations, not authoritative performance MIDI. The footage excludes treating the high tone as a plainly uninterrupted four-second hold and makes a zero-history isolated lower note especially weak. It does **not** uniquely supply a replacement chord, per-board initial voltage or note allocator order. Any such replacement must remain a declared sensitivity, not silently become new ground truth.

The useful next distinction is between source-supported input constraints and the separate primary-circuit channel-allocation/retrigger rules. A board-history test can be grounded in those circuit rules, but the video cannot choose its hidden initial board state. Do not fit arbitrary residual capacitor values or reselect ages from the reused source error and then call the resulting history observed. No new gate grid or production change is authorized by this source review alone.

## Evidence bindings

The retained video hash was independently verified as `3debd6a44350cf973de5c0ad85820acb0eaa1c5575b809e7c5340e6b8d0294ee`. Eight pertinent original stills were also verified against the boundary report's bound hashes. The following inspected artifacts are under `/tmp/ceres-cs50-1x8LGd`; source measurement values above are quotations in substance from the retained reports, not new target estimates.

| Artifact | SHA-256 |
|---|---|
| `research/whitmore-flute-history.md` | `6f2183206abe1c43ceb5931678fc4aa74ef058fa14206a5d0ab1849d359ab4af` |
| `calibration/whitmore-flute-history.json` | `80dffe037b333d5b0d9c841cfdb8d9576ef677679a0220b4b8df674d411bbfcb` |
| `calibration/whitmore-flute-history-boundaries.json` | `7218c77553f290e823e0f41bfb67b235273aa9928d5a3b80c4a952a02e20c1d1` |
| `research/whitmore-flute-preset-transition.md` | `cbba7b176edc441ccd972b150ded3887ffc23e4d9255f10d1aadfe7fbcb730aa` |
| `research/whitmore-transpose-keyboard.md` | `9cae46b87cab08f24ad9632adda83195a9a2f1a371d9d799080f1966ac1b54e4` |
| `research/whitmore-transpose-keyboard.json` | `b1351f629eb4bbaa491335591a57d072d8e57b18f6bd133617378bc0c3e32263` |
| `calibration/whitmore-fresh-history-plan.json` | `216ae0186ec7042f4b41039ed01e53a07094bbb90ce6418582dea0cc2eeceff2` |
| `tools/profile_whitmore_fresh_history.py` | `5d4a085603fb67af7a7c5be685a9a25419c878a5c7d1f3b0025d7e4cee661019` |
| `tools/benchmark_whitmore_transpose.py` | `332b141a5842d2669f05a6eb0f758eae1fb70017b6819de7ef116644046f4841` |
| `calibration/whitmore-transpose-plan.json` | `7d0b4d66b25665a68c112ea7c20eb235978f2fa51cfeacee6322cc735da27544` |
| `research/whitmore-flute-history-frames/contact-1.jpg` | `b131cf73fed916fc9e2a4ac34538813f19a54d980dec84af4101b834023989c5` |
| `research/whitmore-flute-history-frames/keys-1.jpg` | `c22b634899d3c37d43973bc0866cb1acaa83a6009dfffdf0ee7d13d5b4a08196` |
| `research/whitmore-flute-history-frames/keys-2.jpg` | `9b78989abd1df3732cbf91d8c95133b1d80ff0487e3e662ef6bf21a84eb3f02c` |
| `research/whitmore-flute-history-frames/205.20.png` | `ddae599df199bd40193b55e844e094a86d298980a8fba69656cb6fc691696447` |
| `research/whitmore-flute-history-frames/205.56.png` | `d993e31d501ad8195298b23c71083f5c317516d94bf6d38269458621757ef9f9` |
| `research/whitmore-transpose-keyboard/205.80-ordinal.png` | `38cd6923ddd723d8f47f46398651abb29ecacbe1016a03bca0c1f1b25917ae47` |
| `research/whitmore-transpose-keyboard/210.90-ordinal.png` | `a3d99585e80f8c0d4a1644b3df7340146c3048f4ee6312f7e3d5abe837d0eb30` |

Current pitch-route handoff inputs:

- `Docs/accuracy/pitch-route/checkpoint.json`: `544cf8b8e770b0095e37298b3de342db1111e6f99c771eaa90c041781ad17be4`.
- `Docs/accuracy/pitch-route/RESULT.md`: `7be3ce5ba285526fce5e884c3801a156cf5b988fca319d28333f636c72afcd0f`.

Original source files, images, events/MIDI, measurements, native outputs, DSP and previous reports remain unchanged.

# CS50 allocation and envelope-history source audit

2026-10-19. Read-only source/code audit; no DSP change, audio render, recording fit, or user interface change.

## Finding

Ceres's current amplifier-Idle/quietest-release/oldest-held allocator is an engineering policy, not the documented CS50 KAS policy. The complete YM26600 pin table and direct CS50 KAS/SH connections provide **no intentional amplifier- or filter-envelope feedback input**. Yamaha's worked four-channel example explicitly leaves a fifth held key unassigned. Thus envelope-dependent channel selection and mandatory oldest-held stealing are concrete mismatches. The documents do **not** completely specify arbitration among free channels, same-key reattack, or recovery of an ignored key that remains held. No exact replacement algorithm is claimed here.

This matters internally even without additional keyboard features: changing which physical M board receives the next trigger changes the retained VCF/VCA capacitor voltages and subsequent transient. It does not establish the unknown original gates or board history of the public Flute recordings.

## Primary evidence

The following page numbers distinguish printed pages from one-based PDF pages. All files are local retained primary sources; hashes are below.

* Yamaha *CS Series Servicing Guide*, printed 12–19 (PDF 21–28): YM26600 pins, coder/assigner operation and the worked assignment chart. Printed 23 and 25–26 (PDF 32, 34–35) distinguish free-channel key-code copying from Sustain I/II analog sample-and-hold behavior.
* Yamaha *IC Guide*, YM26600 printed 1-4 (PDF 15) and its following block page (PDF 16): complete 40-pin table and the internal `Key Code Memory`, `Key ON Register`, `Old Key OFF CH Detector`, and `Key ON-OFF Controller`. The lower application illustration on PDF 16 is CS80, so it is not used as CS50 board wiring evidence.
* CS50 service manual part 2, printed 3 (PDF 3), direct KAS schematic, and printed 7 (PDF 5), direct SH board wiring. These identify the four-channel connections independently of the generic chip description.

The Guide chart is directly viewable at [printed 19/PDF 28](/tmp/ceres-allocator-review-20261019/guide-28.png). The comparison/storage rule is [printed 18/PDF 27](/tmp/ceres-allocator-review-20261019/guide-27.png), and the internal digital block is [IC Guide PDF 16](/tmp/ceres-allocator-review-20261019/ic-016.png).

### Complete chip boundary and absence of envelope feedback

| YM26600 pins | Documented function | Direct CS50 connection |
| --- | --- | --- |
| 1, 6 | Supply rails | Local +8.5/−6.5 V |
| 2–4 | Master and synchronized clocks | KAS clock/division paths; nominal master 94±5 kHz |
| 5 | Initial clear | KAS power-on clear circuit |
| 7–19 | Note data CL through C | Keyboard note contacts |
| 20–24 | Octave data U1 through U5 | Keyboard octave contacts |
| 25–31 | Note/octave code output | YM26700 D/A converter and exported code lines |
| 32–39 | KO1–KO8 / mode configuration in unused channels | Four used KO signals, remaining channel configuration |
| 40 | Mode | Fixed four-note mode wiring |

All 40 pins are accounted for. On the CS50 board, KO1–4 feed buffers/transistors and TR1–4; key-code outputs feed YM26700, whose K1–4 outputs go through SH to the four M-board key-voltage inputs. The SH drawing routes TR onward to the M-board trigger. The M/KAS `EK` connection is the analog earth/reference, not an envelope-level signal. There is no M-board envelope-return wire to a YM26600 decision input. This establishes absence of a designed EG-level/EG-completion control within the provided netlist, not absence of incidental supply/ground coupling in real hardware.

A key-off register can track digital release history. It cannot measure amplifier voltage, detect the software's numerical Idle threshold, or know selected A/D/R settings from these connections. Quietest and oldest-key-off selection may sometimes coincide; that coincidence does not make an envelope measurement part of KAS.

### Exact worked sequence

The following uses Yamaha's printed note labels without imposing a MIDI octave convention.

| Action | Key-code memory ch1/ch2/ch3/ch4 | Trigger consequence shown |
| --- | --- | --- |
| Press C2, G2, E3, A4 in that order | C2 / G2 / E3 / A4 | TR1 / TR2 / TR3 / TR4 turn on respectively |
| Press A5 while the four remain held | Unchanged | A5 is not stored; no held channel is stolen |
| Release A5, then C2, then A4 | Unchanged | TR1 and TR4 clear; TR2/TR3 remain on |
| Press C#5 | C#5 / G2 / E3 / C#5 | TR1 turns on; TR4 remains off |

The chart proves that copying a new key code into a free channel is distinct from triggering that channel. It also proves that a key-off need not rewrite stored key codes. Channel 1, released before channel 4, receives the next gate; this single example is compatible with both oldest-key-off ordering and a fixed channel scan preference. It does not discriminate between them.

Printed 18 gives the free-channel replacement condition in terms of a stored code unequal to the new key and its corresponding key being up. There is no wait for the analog release envelope to finish. A sounding release tail can therefore be eligible for a new assignment. This is the source-backed reason to separate digital allocation eligibility from an audio envelope's Idle/level state.

## What the sources leave unresolved

**Ignored fifth key and deferred admission.** Printed 15 says the coder repeats its cycle while a key is held; printed 16 shows repeated held-key codes and higher-note encoder priority. This supports a scanner-based *inference* that an ignored still-held key could be admitted after a channel becomes free. But the figure also describes “up to 4 notes,” and the worked p19 example releases ignored A5 **before** freeing C2/A4. No inspected truth table specifies overflow recovery, a queue, or a need for a new physical edge. Do not turn that inference into a demonstrated hardware rule. Higher-note coder priority is not proof of high-note voice-stealing priority; the fifth/highest A5 is explicitly ignored in the example.

**Choice among free channels.** The IC block's `Old Key OFF CH Detector` is evidence of a digital key-off detector, but its name is not a complete priority algorithm. The internal gate/state equations and tie handling are absent. The p19 chart supports ch1 over ch4 in one ordered release sequence only. Neither round robin nor arbitrary oldest-note-on reuse is established.

**Same physical key released and pressed again.** A held key's repeated scan data keeps its gate on rather than causing repeated attacks. A release/repress must be represented as a new physical gate transition, but the sources do not state which free channel is chosen when identical key codes remain copied in several channels. The p18 unequal-code sentence cannot be implemented as the entire algorithm: after one note has filled all four empty memories, a release/repress of that same note would otherwise have no eligible different-code memory. Key-ON-register logic must handle cases omitted by that simplified explanation. It would be incorrect to claim the sentence proves “never reuse the same-code board.”

**MIDI overlap and pedal.** Two overlapping note-ons for one pitch, FIFO note-offs and a MIDI damper are software adaptations, not distinct physical contacts on one CS50 key. They need a separate declared policy; they cannot resolve the source's same-key ambiguity. Existing ring-key bookkeeping is outside this allocation audit.

**Sustain II and key-code memory.** Printed 19 says key-off alone leaves raw KAS codes unchanged; printed 26 says the characteristic changed-pitch tail requires another key after release. Free-channel code copying can change oscillator/filter key voltage without regating the A/F envelopes. Ceres currently mirrors `lastAssignedKey` on note-off/pedal-up as well as new note-on in mode II. That is already an explicit reduced event policy; its exact selector/scan transient is not established by these pages. This secondary observation does not justify changing it in this read-only audit.

## Code comparison and smallest defensible next experiment

In the bound `ceres.cpp`, `Synth::noteOn` at lines 866–890 first prefers `amp.stage == Idle`, then minimizes released `amp.voltage`, then steals the oldest held voice. Both first choices consult analog-model state unavailable to KAS. `Envelope::on` at line 389 changes stage/gate without clearing voltage; that retained-capacitor behavior is appropriate and should survive any allocator experiment. Note-off uses FIFO for overlapping same-pitch MIDI notes.

A source-constrained internal allocator experiment can implement these **verified invariants** without claiming a complete YM26600 emulation:

1. Decide free/held eligibility solely from digital key/gate bookkeeping, not amp/filter level or completion.
2. Do not steal one of four physically held assignments for the demonstrated fifth distinct key.
3. On a selected free-board trigger, preserve oscillator phase, filter states and both envelope capacitor voltages; enter Attack from that retained state.
4. Keep free-channel key-code copying separate from the one selected trigger.

A *complete runnable policy* additionally needs choices not identified by these sources. A bounded research implementation could explicitly choose oldest-key-off then channel index, and separately declare either deferred rescan or edge-only overflow recovery; it must label those assumptions and test their consequences. Oldest-key-off is consistent with the named detector and p19, but is not uniquely proved. Repeated physical-pitch handling must operate on a unique held-key set; any multiple-note MIDI policy stays separate. Do not silently promote one such completion as exact hardware behavior or introduce a new panel control.

The following state-level tests would discriminate real corrections without recording fits:

* Reproduce the full printed p19 sequence, including ignored fifth note, unchanged code memory at key-off, and ch1 trigger/ch4 copied code on C#5.
* Feed identical digital key events under different amplifier A/D/R, sustain, level and velocity settings: channel/gate decisions must remain identical. This is a counterexample test for the current envelope-based allocator.
* Release a board while its A/F voltages remain nonzero, then reassign it: eligibility must not wait for numerical Idle, and voltage/phase must be continuous at the assignment boundary.
* Repeated scan observations of a held physical key must not retrigger. Test release/repress separately under the declared completion policy rather than presenting its board choice as source-verified.
* Keep all four held and try a fifth; then free one while the fifth stays down. This is a required future hardware observation or explicitly labeled policy branch, not a behavior demonstrated by p19.
* Reverse the release order of channels 1 and 4 and repeat the p19 new-note step. This distinguishes oldest-key-off from fixed channel priority; the retained documents do not supply that observation.

No default change is recommended solely from a guessed complete policy. The unambiguous findings justify testing an envelope-independent, no-held-steal allocator separately; any resulting audio improvement would still be conditional on the recording's unknown prior keys/gates.

## Bound inputs

Before forming new claims, existing keyboard/master-pitch, sustain/release, retrigger/EG and amp-bias allocation notes were searched/read. Their hashes and the exact inspected code/primary pages are retained below. Raster pages were generated with `pdftoppm -f N -singlefile -png` from the bound PDFs; the PDF and page references are authoritative even if temporary raster files are later removed.

| Input | SHA-256 |
| --- | --- |
| [cs-servicing-guide.pdf](/tmp/ceres-cs50-1x8LGd/research/cs-servicing-guide.pdf) | `2738f7ddc46f0bedbab38dcad8e2c112c41b3e6ca34670d471d0cb8c0dbde633` |
| [yamaha-ic-guide.pdf](/tmp/ceres-cs50-1x8LGd/research/yamaha-ic-guide.pdf) | `d728bfa1b5dc4575414f8d2bca4a13aef95008f7f2ae21b6e2e9cc28f8c2ee09` |
| [service2.pdf](/tmp/ceres-cs50-1x8LGd/research/service2.pdf) | `1102e55b949378e7776555bca1a4d73105ca2d76d6fcf3aa4c2b103566201cfc` |
| [tracking-cs50-kas.png](/tmp/ceres-cs50-1x8LGd/research/tracking-cs50-kas.png) | `048185f249d9a1ddb507647c35136b6455baf2be0b4aacabf8966b2b3f7cf4ea` |
| [tracking-cs50-sh-board.png](/tmp/ceres-cs50-1x8LGd/research/tracking-cs50-sh-board.png) | `57fc71e894d33ffbb723fc9bb5e7463813e925416857b64ef2e5a6707fe70c9a` |
| [keyboard-tracking-and-master-pitch.md](/tmp/ceres-cs50-1x8LGd/research/keyboard-tracking-and-master-pitch.md) | `d3b865472270f5c21d27e69d2f486c18e9836941127a82de8352a17b5d5b92a5` |
| [sustain-release-routing.md](/tmp/ceres-cs50-1x8LGd/research/sustain-release-routing.md) | `4df91ecef7eb484805bd84bef20f0b01460d8cb34e8d7702bf82ed1c4a3ed87d` |
| [dsp-model-audit.md](/tmp/ceres-cs50-1x8LGd/research/dsp-model-audit.md) | `83519fc2300389853fb74e4640da23c5de6afa3f70ac8d0281565432b29fcac3` |
| [amp-bias-candidate-review.md](/tmp/ceres-cs50-1x8LGd/research/amp-bias-candidate-review.md) | `adcd557b88d7d127f51a2c5721f0edc5bf1574a59e58fef06a46f6a8e5efca03` |
| [whitmore-flute-history.md](/tmp/ceres-cs50-1x8LGd/research/whitmore-flute-history.md) | `6f2183206abe1c43ceb5931678fc4aa74ef058fa14206a5d0ab1849d359ab4af` |
| [guide-24.png](/tmp/ceres-allocator-review-20261019/guide-24.png) | `d85a8e5bd2af62f7a9623b33a60e5e9874daed0c45e81f9fe06e5f6e42485971` |
| [guide-25.png](/tmp/ceres-allocator-review-20261019/guide-25.png) | `e210aeeaf143999e7d5a27ca01f5e09c36446a8d205ce259199e6aaec051207e` |
| [guide-26.png](/tmp/ceres-allocator-review-20261019/guide-26.png) | `ae22dad91587f3ee41bb9f3dfbcff29e4f8909f8408cbcdd4d9f7591c2d3d62f` |
| [guide-27.png](/tmp/ceres-allocator-review-20261019/guide-27.png) | `63e4b781911673ca2998d10251152fcda15c5754101eab17648a3a66c33bd530` |
| [guide-28.png](/tmp/ceres-allocator-review-20261019/guide-28.png) | `8832e44322cab094ab5c2bc9b476163c9de5c74567ed9ff6329007f97ed59cdd` |
| [ic-015.png](/tmp/ceres-allocator-review-20261019/ic-015.png) | `4fa503c4f73d401007109aef13fe2f72e7f72f9343fdc74722cb5ef20a3bc9ab` |
| [ic-016.png](/tmp/ceres-allocator-review-20261019/ic-016.png) | `114a850b6f4240ab3444c5868e909929397f0d4afa58961b2415b555bbc4b1a8` |
| [ceres.cpp](/tmp/ceres-pitch-route-20261019/Source/DSP/ceres.cpp) | `7b0bfc9b9ff2377390ca43ebc4491bf804d53e83ebcff273dfa20cbcfd780306` |

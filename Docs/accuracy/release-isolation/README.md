# Effective VCF release isolation

Eight predeclared native conditions hold both reconstructed gate histories, factory D/Q and all other controls fixed. Global release is flattened into the selected amplifier duration, preserving AMP timing exactly; only effective VCF release varies. No DSP/default change or fit is made.

| History | Filter R factor | Six-cell ratio MAE, dB | A3/C4 tail RMS, dB | E4 tail RMS, dB | Weak records |
|---:|---:|---:|---:|---:|---:|
| 20 | 1 | 3.510026 | 4.348570 | 5.947597 | 3 |
| 20 | 0.5 | 3.718462 | 4.362111 | 5.901174 | 3 |
| 20 | 0.25 | 3.817532 | 4.405325 | 5.881780 | 4 |
| 20 | 0.1 | 4.015466 | 4.576204 | 5.870227 | 5 |
| 10 | 1 | 3.514455 | 5.435264 | 3.135498 | 0 |
| 10 | 0.5 | 3.479834 | 5.439161 | 3.109755 | 0 |
| 10 | 0.25 | 3.467580 | 5.443243 | 3.101941 | 0 |
| 10 | 0.1 | 3.489557 | 5.448775 | 3.099738 | 0 |

Strict cross-history transfer criterion: 0/3 factors pass. This criterion also requires no worsening of any note's release-cell/tail error and no increased weakness. All per-note/early/release/C4 higher-ratio values remain in report.json, including adverse outcomes.

Both factor1 controls are byte-identical to the current renderer baseline. All eight renders preserve their guaranteed prefix before the first note-off. This is an implementation isolation check, not evidence that either gate history equals the hardware performance.

These reused public recordings have named Brass I but no original MIDI, measured timing voltages or fully identified global modulation. The result tests effective filter-release sensitivity; it does not identify a physical timing law, repair the published A-versus-D/R discrepancy, or authorize a factory/default change.

Reproduce without changing evidence: `python3 Tools/accuracy/release_isolation.py check`. The immutable plan binds the retained renderer and its five compiled inputs, exact commands, patches, MIDI, original source masks/gain, and unmodified measurement helper. `declare` and `run` refuse overwrite. Audio is local ignored analysis evidence, never a synthesis asset.

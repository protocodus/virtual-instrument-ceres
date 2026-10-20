# Hardware realism previews, 2026-10-20

One-time review evidence for the 0.3.0 key-assigner and touch changes, rendered with `ceres_render` at 48 kHz, 4 voices, 4×, seed 50, no normalization. "Before" is the parent `35c1aaf`. Frozen on merge.

| File | Preset and events | Shows |
| --- | --- | --- |
| `glide-off.mp3` | brass1, legato 48→60→55→67→43 | Glide off (byte-identical to parent) |
| `glide-portamento.mp3` | same, `glide_mode=1 glide_rate=24` | Smoothed counter glide |
| `glide-glissando.mp3` | same, `glide_mode=2 glide_rate=24` | Semitone steps |
| `repeated-note-before.mp3` / `-after.mp3` | electric_piano, 60, 64, 60, 60 | Repeated key re-keys its own board |
| `touch-before.mp3` / `-after.mp3` | string1, velocity source, `pressure_filter=1.2 pressure_vibrato=40`; after adds `pressure_wah=1.5` | Unequal touch brilliance, raised touch vibrato, touch wah |

# Library calls the 1.0.4 engine makes, by compiler and flag set

`nm -u` on `Source/DSP/ceres.cpp` compiled at `-std=c++17 -O3 -DNDEBUG` (this
container: GCC 13.3, clang 18.1 with libstdc++, clang 18.1 with libc++ 18),
keeping the libc/libm names only. The Rack's SDK libc++ additionally turns
`std::abs` and `std::isfinite` into calls (Acustra, measured on SDK object
code); libstdc++ and libc++ 18 spell them with `__builtin_` themselves.

| Build | Calls |
| --- | --- |
| GCC `-O3` | asinh exp exp2 expm1 log log1p log2 lround memcmp memcpy memmove memset pow round sin sinh sqrt strcmp strlen |
| GCC `-O3 -fno-builtin` | the above plus **ceil copysign floor tanh** |
| clang `-O3` (libstdc++) | asinh bcmp ceil exp exp2 expm1 floor log log1p log2 lround memcpy memmove memset pow round sin sinh sqrt strcmp strlen tanh |
| clang `-O3 -fno-builtin` (libstdc++) | the above plus **copysign memcmp** |
| clang `-O3` (libc++ 18) | as clang with libstdc++ |
| clang `-O3 -fno-builtin` (libc++ 18) | as clang `-fno-builtin` with libstdc++ |

On the audio path of 1.0.4 only `copysign` (the ring signal pair, about 1.1
per internal sample) was among the calls `-fno-builtin` adds here; `floor`
(oscillatorTrace, reset), `ceil` (configureRate, setPatch), `sqrt` (decimator
construction, configureRate, updateBoardScales) and `tanh`
(ringWetBufferGain) run on the control path or at construction. `std::abs`
and `std::isfinite` on the audio path (fastTanh, the filter solver, the
envelopes, the ring signal pair, the EXT IN guard) are the ones the SDK
libc++ would call; 1.0.5 spells all of them `__builtin_`.

Calls per 240 blocks of 64 frames at four voices/1x on all 14 factory presets
(callgrind, either compiler, `-fno-builtin`): exp2 2,798,878 (three per
voice-sample: pitch, HP and LP cutoff factors; one per sample for the ring
carrier frequency), expm1 237,017 (ring signal pair), copysign 234,568, exp
229,004 (fastTanh beyond |x| = .625, mostly the WSC shaper), sin 220,248 (ring
carrier), log 24,970, log1p 13,118, sqrt 5,334, pow 1,979.

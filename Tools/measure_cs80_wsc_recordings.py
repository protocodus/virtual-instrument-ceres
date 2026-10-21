#!/usr/bin/env python3
"""Measure WSC waveform relationships in a public Yamaha CS-80 sample set.

This is the measurement protocol behind the CS-80 evidence cited beside
`Oscillator` in Source/DSP/ceres.cpp and in
Docs/research/vco-authenticity-2026-10-20.md. It is sibling evidence: the
CS-80 uses the same M board (#21233; its CS-80 column differs only in the
transposition parts, SM2 p.16) and so the same IG00158 WSC as the CS-50.

Recording. Freesound user "mogigrumbles", Yamaha CS-80, uploaded 2020-07-07:
"I sampled all 61 notes for each of the CS80 tone selector presets, and also
the raw waveforms". Packs SAW 29608, SQUARE 29624, PULSE 75% 29623, PULSE 90%
29613, PWM SLOW 29612, PWM MED 29607, SINE 29632; licence CC BY-NC 3.0. The
originals are 44.1 kHz/16-bit mono WAV; this tool reads the public "hq"
previews (MP3 about 255 kb/s, Ogg Vorbis) and pins their SHA-256. Every file
is peak-normalised, starts with about 0.8 s of silence and sounds one octave
below its MIDI note (16'). Capture chain, panel settings and voice
allocation are not documented, and the takes are separate recordings that
are assumed to share one signal path. Audio is downloaded only into a
directory you name, outside the repository, and is never committed.

Method (all on the steady sustain: onset at 10 % of the peak 10 ms RMS,
then 0.3 s skipped, 0.1 s for PWM, and the last 0.05 s dropped):
- f0 is the maximum of the summed magnitudes of harmonics 1-8 within 3 % of
  the 16' nominal; every edge or harmonic measurement uses it.
- SAW: skewness of the first difference. Negative means the steep edge
  falls, i.e. a rising ramp with a falling reset.
- SQUARE, PULSE 75 %, PULSE 90 %: every rising and falling edge, found as
  for PWM below; the high fraction is the median, over periods, of the time
  from a rising edge to the next falling one over that period.
- PWM SLOW/MED: every rising and falling edge (derivative peaks above half
  the median per-period maximum, one per half period, parabolic sub-sample
  position). The fixed edge is the one whose period-to-period interval does
  not move. The low fraction is the phase from each falling edge to the next
  rising edge; its main frequency is the PWM rate, and a joint least-squares
  fit of that rate's first three harmonics gives the trajectory's H2 and H3
  (a sine PWM source on a linear ramp leaves none).
- SINE: least-squares fit of DC and harmonics 1-9 in 0.25 s frames (f0
  refined per frame within 0.1 %), median level of each harmonic relative to
  H1, and the median phase of H3 relative to 3x the H1 phase in the sine
  convention (0 deg: flattened peaks, compressive; 180 deg: sharpened peaks,
  triangle-like).
- Chains (from the report, no audio): each candidate shaper on an ideal
  triangle, then zero to two OTA input stages modelled as differential pairs,
  tanh(a*y)/a, each at a drive a of 0 or 0.02-10 (131 values). For each note,
  the H5 and H7 reachable with H3 within 0.5 dB of the note's, against the
  measurement; and the least rms error to the median H3/H5/H7.

Uncertainty. Codec: MP3 and Vorbis previews of the same note are both
measured where both exist (8 sine notes): H2-H5 agree within 0.6 dB, H6-H9
within 1.0 dB. Edges: interval spreads are in samples at 44.1 kHz; on every
PWM note the moving edge's spread is 3.7 to 98 times the fixed edge's (least
at the highest slow-PWM note, where the edge moves least per period).
Chain: each sine passes the sine-level, common and envelope VCAs (three
IG00151 OTAs), the output stage and an unknown capture, so its harmonics
bound the whole path, not the WSC shaper alone. The PWM/pulse/saw edge
directions are relative to the same assumed chain polarity; duty values
depend on unknown PW lever positions behind the pack labels.

Usage:
  measure_cs80_wsc_recordings.py fetch DIR     # download and verify
  measure_cs80_wsc_recordings.py measure DIR [--report FILE.json]
  measure_cs80_wsc_recordings.py chains FILE.json
Requires Python 3 with NumPy, ffmpeg for decoding and curl for fetching.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import time

import numpy as np

RATE = 44100
USER = 11399618  # mogigrumbles' Freesound user id, part of each preview URL
# (pack, MIDI note, Freesound sound id, preview codec, SHA-256 of the preview)
FILES = [
    ('SAW', 36, 525432, 'ogg', '3bb50a601206fdf6e1a1f72423b65f2c32a83319fe532963bd976e8bfdb67599'),
    ('SAW', 41, 525470, 'ogg', '416dc253e498d3df1dffa7e30c2ba31d854c9021b525c7f89a43362c375d2c81'),
    ('SAW', 45, 525439, 'ogg', 'c5e6ebffd243bd50faa5431bfa95b09af34af14e30da918169e3fe4cfc58b27a'),
    ('SAW', 48, 525474, 'ogg', '90884a2a32367a31bf129d784e95a69ea3d71f2f9bf9d4a868fa3d8109692ede'),
    ('SAW', 53, 525419, 'ogg', '6fb46a7be684c0373781593c3efeb44d14d3287ea2fde8c766046e877d1e0d5b'),
    ('SAW', 56, 525476, 'ogg', '1967621c28cc1b4fff9eb96735ad8535dcf5b8556e38538a72327bcbf71d1b8c'),
    ('SAW', 65, 525448, 'ogg', '085f943324d76cd31d3fb1e882bb957ac61f49f79c3a931da22d7872f6f282c5'),
    ('SAW', 69, 525450, 'ogg', '9a62ea829df8d3a26cff56e3b8fe145f5a3e669316e6017d2723c6afed225d6f'),
    ('SAW', 72, 525416, 'ogg', 'b2db157fb40817a8b4ae00636b5506315fb6345bc1850692c193a3241c585e9d'),
    ('SAW', 77, 525444, 'ogg', '9a586541774de0459aada0aab1affb67542fbf2698c21e1a83e4780af0cdc0c9'),
    ('SAW', 84, 525471, 'ogg', 'c94d86c732498dec7c665810598885bd6addeac35cf4a8ab89699738a4ee6a45'),
    ('SQUARE', 36, 526412, 'ogg', '9616876138a1b1245703a6671d041017123d7cfcc159f497b27948dc70469f77'),
    ('SQUARE', 41, 526450, 'ogg', '2716a9c33dcd5b45c970d8809d7e73f3479f47865863049ae0d76e083aa14053'),
    ('SQUARE', 45, 526410, 'ogg', 'c2df1b78364a2378747080564fa44d3bd36c1f0df28fe4b094e49243623b3b55'),
    ('SQUARE', 48, 526440, 'ogg', '24cb1a717a58e119205aa589f6e69b7a91e26bee85e3b1f279e196502f3efaf8'),
    ('SQUARE', 53, 526400, 'ogg', 'acd1731589c8752f8227bed05b108a6b9290d923eef3521c138092d831033eea'),
    ('SQUARE', 56, 526431, 'ogg', 'd4b928fba22d3ad6a86780ba060eaf3242362903a446acbf4ebb6a1970248bab'),
    ('SQUARE', 60, 526433, 'ogg', '5693b94e4cfc02eafc6501c2023a2d0ae708932e4cb677c417b5aea95f1c4c84'),
    ('SQUARE', 65, 526429, 'ogg', '14a33958632fdfb4c5220309239d0f47e84c043ce6908a80b62cd2b8c5b22891'),
    ('SQUARE', 69, 526428, 'ogg', 'c3b6db950a3bbb6c0862d5adf48032ec8883dce3ac9f25b112aff82766deb37c'),
    ('SQUARE', 77, 526415, 'ogg', 'b3d0ee872a0fc8fe74b609ac41cf4cb0ba624ac83470e9a49da964c1a60626ec'),
    ('SQUARE', 84, 526438, 'ogg', 'f9b16572f50e30b3188d5208ff0f37520196a984eca1417da611d41692ed60dd'),
    ('PULSE75', 36, 526367, 'ogg', 'aebbdb928b8ca5fd30b509198ececd1ea5035d8b87fb64b2fc96726b6c6cf18e'),
    ('PULSE75', 41, 526353, 'ogg', 'daac61485aa73f725b77886df2ac5cd9181ac44fa3e7ec1363ef665afe24186b'),
    ('PULSE75', 45, 526363, 'ogg', '287b41161c62b6f5aad7423a0ef298ec9daf7f798082c81ca85fd25444ccf82a'),
    ('PULSE75', 48, 526343, 'ogg', 'f71c1b12dcef65b35ca1d6ce86fc6ff9b73abd3e8dd603558f9783a7ad77457f'),
    ('PULSE75', 53, 526382, 'ogg', 'ff7eb96887eb5d7bb392d094470e75204f564a2d4ec578ec8763d06d5624fbe9'),
    ('PULSE75', 56, 526336, 'ogg', '4854f1481aa0cc21439372d89e883cbefb609f372648797ff4565c322a641726'),
    ('PULSE75', 60, 526332, 'ogg', 'c8b9b0f4d1f7156c7df9cc616f33cda3d2f9ce8fb340230325bc9797563842fd'),
    ('PULSE75', 65, 526361, 'ogg', '683cc319a2de3f7ca9267dd4e698ab8777b1548a3bea0c9f7caa83c9a975cd76'),
    ('PULSE75', 69, 526359, 'ogg', '1facf631d38343a8586ec713caf7f18dee5d1e1a3c06512e6d8aa28e7c7e1bc4'),
    ('PULSE75', 72, 526380, 'ogg', 'e8f70d4956d977ae3eb21a81a1b5fea547c508badb6130845b7b66ba17dcf2cf'),
    ('PULSE75', 77, 526369, 'mp3', '940af2a6a1468ceb0b313319c5de8fad403628cc5c2cc1e5da60cc22034bd72b'),
    ('PULSE75', 84, 526340, 'ogg', 'b661e486a11706c12b752a3283e6a97bf2318e1e675a3dc58f23bdc9d9a3ee1f'),
    ('PULSE90', 36, 525727, 'ogg', '0088727b49c5bc7cb1964b37c617d3c946a08794ec8ffb34db4791d5d4634dbe'),
    ('PULSE90', 41, 525763, 'ogg', '62007c4c50890c33baa725d04daa2a9c040f88128468d3fd7a241b03afbb5b9d'),
    ('PULSE90', 45, 525723, 'ogg', 'bcec2798ef2e723cdf64719eecb9865758eec00a6bf4240be1723d42dae95e45'),
    ('PULSE90', 48, 525758, 'ogg', 'da434767e6cfd39760efa1be79f15b3a81482a693940c3deeeeb4fc7a7e62b84'),
    ('PULSE90', 53, 525746, 'ogg', '025b973a95b14e6bf901506e08efa17b91eabfef743bd52eecd3f0d09119ecd1'),
    ('PULSE90', 56, 525780, 'ogg', '417484b9d6f97779fb4e681f9c314ec22ea7e27e47656ec0f7fa535ee0198c65'),
    ('PULSE90', 60, 525752, 'ogg', 'd5f8dceba9d5c62d71ce91c624ba54ed283929501111d33477e5f934f01c8b2e'),
    ('PULSE90', 65, 525781, 'ogg', 'fe6e8358ac8c4e67bfdbb12ba939fadff593644a557f7edd0cdc1d67ea26f451'),
    ('PULSE90', 69, 525776, 'ogg', 'a21d5b566c13fd0f00b411b0d98648ef65665c425622f96d9691795f282eb1a0'),
    ('PULSE90', 72, 525744, 'ogg', '3d3d13847d545821934ab7abe4ad860aca20efe9c42849520aa7fcbc47ea8510'),
    ('PULSE90', 77, 525731, 'ogg', '42ae2d926da388e0ad5ac787c4e4d8418080209262ba6d7c49da9f83248d9fcc'),
    ('PULSE90', 84, 525755, 'ogg', '2db9a10e66817d1e45f9bf3cad6f583885de1fde9145559dbfab617206aa4387'),
    ('PWMSLOW', 48, 525712, 'ogg', '9533eb78e34028f840770030cae94242f4cd22e1cc73fe9de409963b7cc404b6'),
    ('PWMSLOW', 53, 525678, 'ogg', 'bc142c8e0a8678ae83a13ddf6dfbcccf004260fee34cfe63880b2867665198b7'),
    ('PWMSLOW', 57, 525719, 'ogg', '956469cfa7389a09324bea9a774d1d65e7a04bd70504e27bb766ea5017e49112'),
    ('PWMSLOW', 60, 525697, 'ogg', '399444fae5209e1d5f9cd0fe8bdcc926efc5e889b5a1dddd9a4333023bedc00e'),
    ('PWMSLOW', 65, 525718, 'ogg', '4217ae44b3a61371b03375f6f27794e075a638e023aa1b2d99220a674b32e09d'),
    ('PWMSLOW', 69, 525720, 'ogg', '3191111fbb8d4c4c5a5949235bd57057b06cd92107f850ed320daa14f35bac34'),
    ('PWMMED', 48, 525374, 'ogg', '70b44077406cd3e813f960cb6c9457a220aae8fbde8723dc9fb733a17496e6a2'),
    ('PWMMED', 53, 525403, 'ogg', 'acc97e4db8cd7e89625badf463da1ab168ba50c9685b87e6d2d444d8831e6f10'),
    ('PWMMED', 57, 525384, 'ogg', '9a38fce0fdd4a74d0da19ce872dbc36ee481cb8037e787aa20a0df0d2f2437e2'),
    ('PWMMED', 60, 525363, 'ogg', '865a6a85b6dade42841403f46690f7a48038e996347be8ba70ca0d8ee47710d8'),
    ('PWMMED', 65, 525380, 'ogg', '3ee9bb2574d685a4a21c42532e64603c500c340fb2220684b056dba63d338c82'),
    ('PWMMED', 69, 525382, 'ogg', '2783476c4cc62b4bd1d7e94d179dcd50741cfe3ddbfbe0bfbabab6d8b3ca4472'),
    ('SINE', 36, 526909, 'mp3', '5b58eab92ecf11f73797410d664fad292535a6c36bda48f3cf685994a5e63d6e'),
    ('SINE', 36, 526909, 'ogg', '54b23b0c426c700703a9f74ddf5cf668fa93ac5705973e8f7c12a1517270dec1'),
    ('SINE', 45, 526905, 'ogg', '1e50f0b54cf1813e2bf976b842c8ef6dcf8039504f42802fb0154c4aba989f8e'),
    ('SINE', 48, 526925, 'mp3', '950ea419cd2b86f303f044a6c017f261264582b58e748ab44bad121f7df5da9e'),
    ('SINE', 48, 526925, 'ogg', '0b78d745458416aca42dbb8894a17374df3f35145bbf0e553591d70d5acda028'),
    ('SINE', 53, 526894, 'mp3', '8177cd481cf4db8feafa67e841b5ee9042f7a804bc2411d27f10879e2eeecd23'),
    ('SINE', 53, 526894, 'ogg', '875681eaabfe2dce85be74faf8ded292fe2067b795c4e0c0f85bcc8a2f723d36'),
    ('SINE', 56, 526939, 'mp3', 'c3c4f9f2a9c99877972221982fa1fe25faeee70fcd4fa2b14e5e4d35f5edbb1f'),
    ('SINE', 56, 526939, 'ogg', '1da3c5a126b959f531333d6fd400abd757eb2728946468af2d20616df052835d'),
    ('SINE', 60, 526934, 'mp3', 'ae2579dcd2d1a39de0a2c6a7bb9b2b294c97bb2cc31f0e727ac3742aa3d428e0'),
    ('SINE', 65, 526912, 'mp3', '4d3bb7ff50e62f350e0f2f0d4fded08cc1e995976584d4f88a9a7a69dae000a5'),
    ('SINE', 65, 526912, 'ogg', '38e22fe1d193d94fda9d40c0e1363d6e3137146d707ad8502095eb89cc026f26'),
    ('SINE', 69, 526910, 'mp3', '1ebff23b379177e2b7fb56c42c60871aec49ad32e8962a7129704cb706b85b2a'),
    ('SINE', 69, 526910, 'ogg', 'cb9d9ccb51eca677096783364ac2b4bf232f197688c3780ff114e99a9a0b2176'),
    ('SINE', 72, 526891, 'mp3', '4e4dace9a4945417dafc4dcd817d41438b1b1d37bb89fdf6bc74ad1c54283bd8'),
    ('SINE', 72, 526891, 'ogg', '15b9062b9130eca19b3f56c53c56fb486b8a5a85e9a52f25772fd1628598653a'),
    ('SINE', 77, 526898, 'mp3', 'be69dc84f16add76235ec9cfc0a3fcfe0be945cbc73f1474f22a27054d38b23b'),
    ('SINE', 84, 526919, 'mp3', '48c03699fa03c48ba5af3dc03e4632dd56c0a3993188d6725a1a01c9bdfa87e5'),
    ('SINE', 84, 526919, 'ogg', '8e7cd7f861081997bb0b083b16d95a0269e609eb49dc9cfaf68fc93599984e64'),
]


def name(pack: str, note: int, codec: str) -> str:
    return f'{pack}_{note}.{codec}'


def url(sound: int, codec: str) -> str:
    return f'https://cdn.freesound.org/previews/{sound // 1000}/{sound}_{USER}-hq.{codec}'


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch(directory: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for pack, note, sound, codec, digest in FILES:
        target = directory / name(pack, note, codec)
        if not (target.exists() and sha(target) == digest):
            subprocess.run(['curl', '-sS', '-f', '-m', '60', '-o', str(target), url(sound, codec)],
                           check=True)
            time.sleep(2)  # the preview CDN rate-limits bursts
        if sha(target) != digest:
            sys.exit(f'{target.name}: SHA-256 differs from the pinned preview')
    print(f'{len(FILES)} previews verified in {directory}')


def decode(path: Path) -> np.ndarray:
    pcm = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(path), '-f', 'f32le', '-ac', '1',
                          '-ar', str(RATE), '-'], check=True, capture_output=True).stdout
    return np.frombuffer(pcm, dtype='<f4').astype(np.float64)


def sustain(x: np.ndarray, lead: float) -> np.ndarray:
    hop = int(.01 * RATE)
    envelope = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) for i in range(0, len(x) - hop, hop)])
    onset = int(np.argmax(envelope > .1 * envelope.max())) * hop
    return x[onset + int(lead * RATE):len(x) - int(.05 * RATE)]


def nominal(note: int) -> float:
    return 440 * 2 ** ((note - 69) / 12) / 2  # 16'


def refine_f0(x: np.ndarray, guess: float) -> float:
    n = len(x)
    spectrum = np.abs(np.fft.rfft(x * np.hanning(n), 4 * n))
    frequency = np.fft.rfftfreq(4 * n, 1 / RATE)
    grid = np.linspace(guess * .97, guess * 1.03, 1201)
    score = np.interp(np.outer(grid, np.arange(1, 9)).ravel(), frequency, spectrum)
    return float(grid[int(np.argmax(score.reshape(len(grid), 8).sum(1)))])


def saw_skew(x: np.ndarray) -> float:
    d = np.diff(x)
    d = d - d.mean()
    return float(np.mean(d ** 3) / np.mean(d ** 2) ** 1.5)


def high_fraction(x: np.ndarray, f0: float):
    """Median fraction of each period from a rising edge to the next falling one."""
    period = RATE / f0
    rise, fall = edges(x, 1, period), edges(x, -1, period)
    fractions = []
    for a, b in zip(rise[:-1], rise[1:]):
        inside = fall[(fall > a) & (fall < b)]
        if len(inside) == 1 and .8 * period < b - a < 1.2 * period:
            fractions.append((inside[0] - a) / (b - a))
    if len(fractions) < 10:
        return None
    return float(np.median(fractions)), float(np.percentile(fractions, 75) - np.percentile(fractions, 25))


def edges(x: np.ndarray, sign: int, period: float) -> np.ndarray:
    d = np.diff(x) * sign
    span = int(round(period))
    threshold = .5 * np.median([d[i:i + span].max() for i in range(0, len(d) - span, span)])
    candidates = np.where((d[1:-1] > threshold) & (d[1:-1] >= d[:-2]) & (d[1:-1] >= d[2:]))[0] + 1
    taken = np.zeros(len(d), bool)
    kept = []
    for k in candidates[np.argsort(-d[candidates])]:
        lo, hi = max(0, int(k - .5 * period)), min(len(d), int(k + .5 * period) + 1)
        if not taken[lo:hi].any():
            taken[k] = True
            kept.append(k)
    out = []
    for k in sorted(kept):
        a, b, c = d[k - 1], d[k], d[k + 1]
        den = a - 2 * b + c
        out.append(k + .5 + (.5 * (a - c) / den if den else 0))
    return np.array(out)


def pwm(x: np.ndarray, f0: float) -> dict:
    period = RATE / f0
    rise, fall = edges(x, 1, period), edges(x, -1, period)

    def interval_std(t):
        step = np.diff(t)
        ok = (step > .8 * period) & (step < 1.2 * period)
        return float(np.std(step[ok]))
    low, times = [], []
    for a, b in zip(fall[:-1], fall[1:]):
        inside = rise[(rise > a) & (rise < b)]
        if len(inside) == 1:
            low.append((inside[0] - a) / (b - a))
            times.append(a / RATE)
    low, times = np.array(low), np.array(times)
    centred = low - low.mean()
    spacing = float(np.median(np.diff(times)))
    spectrum = np.abs(np.fft.rfft(centred * np.hanning(len(low)), 16 * len(low)))
    rate = float(np.fft.rfftfreq(16 * len(low), spacing)[1 + int(np.argmax(spectrum[1:]))])

    # Joint least-squares fit of the PWM rate's first three harmonics.
    basis = np.vstack([f(2 * np.pi * k * rate * times) for k in (1, 2, 3) for f in (np.sin, np.cos)]
                      + [np.ones_like(times)]).T
    c, *_ = np.linalg.lstsq(basis, low, rcond=None)
    amplitude = [math.hypot(c[2 * i], c[2 * i + 1]) for i in range(3)]
    return {'rising_interval_std_samples': interval_std(rise),
            'falling_interval_std_samples': interval_std(fall),
            'low_fraction_min': float(low.min()), 'low_fraction_max': float(low.max()),
            'low_fraction_mean': float(low.mean()), 'pwm_rate_hz': rate,
            'trajectory_h2_db': 20 * math.log10(amplitude[1] / amplitude[0]),
            'trajectory_h3_db': 20 * math.log10(amplitude[2] / amplitude[0]), 'periods': len(low)}


def sine(x: np.ndarray, f0: float) -> dict:
    frame = int(.25 * RATE)
    t = np.arange(frame) / RATE
    levels, phases = [], []
    for start in range(0, len(x) - frame, frame):
        segment = x[start:start + frame]
        grid = np.linspace(f0 * .999, f0 * 1.001, 41)
        local = grid[int(np.argmax([abs(np.sum(segment * np.exp(-2j * np.pi * g * t))) for g in grid]))]
        columns = []
        for k in range(1, 10):
            columns += [np.cos(2 * np.pi * k * local * t), np.sin(2 * np.pi * k * local * t)]
        basis = np.vstack(columns + [np.ones_like(t)]).T
        c, *_ = np.linalg.lstsq(basis, segment, rcond=None)
        cosine, sinus = c[0:18:2], c[1:18:2]
        amplitude = np.hypot(cosine, sinus)
        phase = np.arctan2(cosine, sinus)  # segment = sum A_k sin(k w t + phase_k)
        levels.append(20 * np.log10(amplitude[1:] / amplitude[0]))
        phases.append(phase[2] - 3 * phase[0])
    median = np.median(np.array(levels), 0)
    relative = math.degrees(np.angle(np.mean(np.exp(1j * np.array(phases)))))
    return {'harmonics_db': {f'H{k}': round(float(median[k - 2]), 2) for k in range(2, 10)},
            'h3_relative_phase_deg': round(relative, 1), 'frames': len(levels)}


def chains(report: Path) -> None:
    """Which shaper, followed by OTA input stages, can give the measured sine?"""
    measured = {}
    for e in json.loads(report.read_text())['results']:
        if e['pack'] == 'SINE' and (e['note'] not in measured or e['codec'] == 'ogg'):
            measured[e['note']] = np.array([e['harmonics_db'][h] for h in ('H3', 'H5', 'H7')])
    median = np.median(list(measured.values()), 0)
    n = 4096
    tri = 2 / np.pi * np.arcsin(np.sin(2 * np.pi * np.arange(n) / n))
    drives = np.concatenate([[0], np.geomspace(.02, 10, 130)])

    def odd_db(y):
        c = np.abs(np.fft.rfft(y))[1:8]
        return 20 * np.log10(c[[2, 4, 6]] / c[0])

    def stage(y, a):
        return y if a == 0 else np.tanh(a * y) / a

    def table(shaped):
        rows = []
        for a1 in drives:
            first = stage(shaped, a1)
            rows += [odd_db(stage(first, a2)) for a2 in drives]
        return np.array(rows)

    low, high = min(v[0] for v in measured.values()), max(v[0] for v in measured.values())
    for label, k in (('ideal sin(pi/2*tri)', None), ('differential pair k=1.4020', 1.4020)):
        shaped = np.sin(np.pi / 2 * tri) if k is None else np.tanh(k * tri) / np.tanh(k)
        rows = table(shaped)
        inside = rows[(rows[:, 0] >= low) & (rows[:, 0] <= high)]
        fit = np.sqrt(np.mean((rows - median) ** 2, 1)).min()
        print(f'{label}: with H3 in {low:.1f}..{high:.1f} dB, H5 {inside[:, 1].min():.1f}..'
              f'{inside[:, 1].max():.1f}, H7 {inside[:, 2].min():.1f}..{inside[:, 2].max():.1f} dB; '
              f'best rms error to the median H3/H5/H7 {fit:.2f} dB')
        for note, target in sorted(measured.items()):
            near = rows[np.abs(rows[:, 0] - target[0]) < .5]
            gaps = []
            for i, h in ((1, 'H5'), (2, 'H7')):
                lo, hi = near[:, i].min(), near[:, i].max()
                gap = lo - target[i] if target[i] < lo else (hi - target[i] if target[i] > hi else 0)
                gaps.append(f'{h} {lo:.1f}..{hi:.1f} vs {target[i]:.1f} (gap {gap:+.1f})')
            print(f'  note {note}: ' + '; '.join(gaps))
    free = min(np.sqrt(np.mean((odd_db(stage(np.tanh(k * tri) / np.tanh(k), a)) - median) ** 2))
               for k in np.linspace(.6, 4, 35) for a in drives)
    print(f'differential pair at any k 0.6..4, one stage: best rms error {free:.2f} dB')


def measure(directory: Path, report: Path | None) -> None:
    results = []
    for pack, note, sound, codec, digest in FILES:
        path = directory / name(pack, note, codec)
        if not path.exists() or sha(path) != digest:
            sys.exit(f'{path}: missing or not the pinned preview; run fetch first')
        x = sustain(decode(path), .1 if pack.startswith('PWM') else .3)
        f0 = refine_f0(x, nominal(note))
        entry = {'pack': pack, 'note': note, 'sound': sound, 'codec': codec, 'f0_hz': round(f0, 3)}
        if pack == 'SAW':
            entry['derivative_skew'] = round(saw_skew(x), 2)
        elif pack in ('SQUARE', 'PULSE75', 'PULSE90'):
            fraction = high_fraction(x, f0)
            entry['high_fraction'] = None if fraction is None else round(fraction[0], 3)
            entry['high_fraction_iqr'] = None if fraction is None else round(fraction[1], 4)
        elif pack.startswith('PWM'):
            entry |= {k: round(v, 4) if isinstance(v, float) else v for k, v in pwm(x, f0).items()}
        else:
            entry |= sine(x, f0)
        results.append(entry)
        print(json.dumps(entry))
    if report:
        report.write_text(json.dumps({'protocol': 'Tools/measure_cs80_wsc_recordings.py',
                                      'results': results}, indent=2) + '\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('command', choices=['fetch', 'measure', 'chains'])
    parser.add_argument('path', type=Path,
                        help='download directory, outside the repository (chains: the report)')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.command == 'fetch':
        fetch(args.path)
    elif args.command == 'measure':
        measure(args.path, args.report)
    else:
        chains(args.path)


if __name__ == '__main__':
    main()

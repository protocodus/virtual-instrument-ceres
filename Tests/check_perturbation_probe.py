#!/usr/bin/env python3
"""The one-float-ulp perturbation probe: are last-bit arithmetic changes null-testable?

Compiles Tests/PerturbationProbeDriver.cpp against Source/DSP as checked out and
renders every factory preset (a chord of four voices, 48 kHz, released after
three quarters of the length) unperturbed and with one control multiplied by
1 + 1.2e-7 (a float32 ulp): hp_cutoff, lp_cutoff, hp_resonance, lp_resonance,
at 1x and at 4x. Reports, per preset and control, the residual perturbed minus
reference in 250 ms windows as RMS relative to the reference window's RMS,
over the held chord (the first three quarters) and its release tail. An engine
whose residual stays far below the signal and does not grow while the chord
is held (no limit cycle, no self-oscillation) lets a null test grade last-bit
changes; a preset that runs away makes them listening decisions there
(YouKnow has such presets, Ceres has none).
Example: python3 Tests/check_perturbation_probe.py --report out.json
"""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from array import array

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / 'Tests/PerturbationProbeDriver.cpp'
FLAGS = ['-std=c++17', '-O3', '-DNDEBUG']
PRESETS = ['init', 'string1', 'string2', 'brass1', 'brass2', 'flute', 'electric_piano',
           'clavichord', 'harpsichord', 'guitar1', 'guitar2', 'funky1', 'funky2', 'funky3']
CONTROLS = ['hp_cutoff', 'lp_cutoff', 'hp_resonance', 'lp_resonance']
RATE, SECONDS, WINDOW = 48000, 4.0, 0.25


def decibels(ratio):
    return -math.inf if ratio <= 0 else 20 * math.log10(ratio)


def rms(values):
    return math.sqrt(sum(v * v for v in values) / len(values)) if values else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--grows-db', type=float, default=6.0,
                        help='a second half of the held chord this much above its first half counts as growing')
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    report = {'protocol': 'ceres-perturbation-probe-v1', 'factor': 1 + 1.2e-7, 'rate': RATE,
              'seconds': SECONDS, 'window_seconds': WINDOW, 'compiler_flags': FLAGS, 'results': {}}
    worst = (-math.inf, None)
    grows = []
    with tempfile.TemporaryDirectory(prefix='ceres-probe-') as directory:
        directory = Path(directory)
        binary = directory / 'probe'
        subprocess.run(compiler + FLAGS + ['-I', str(ROOT / 'Source/DSP'), str(DRIVER),
                                           str(ROOT / 'Source/DSP/ceres.cpp'), '-o', str(binary)], check=True)
        for factor in (1, 4):
            for control in ['none'] + CONTROLS:
                subprocess.run([str(binary), str(directory), str(factor), str(SECONDS), control], check=True)
            for preset in PRESETS:
                reference = array('d')
                reference.frombytes((directory / ('%s-%dx-none.f64' % (preset, factor))).read_bytes())
                for control in CONTROLS:
                    perturbed = array('d')
                    perturbed.frombytes((directory / ('%s-%dx-%s.f64' % (preset, factor, control))).read_bytes())
                    windows = []
                    size = int(RATE * WINDOW)
                    for start in range(0, len(reference) - size + 1, size):
                        r = rms(reference[start:start + size])
                        d = rms([b - a for a, b in zip(reference[start:start + size], perturbed[start:start + size])])
                        windows.append(decibels(d / r) if r > 0 else (-math.inf if d == 0 else math.inf))
                    # The chord is released after three quarters of the length: the
                    # held windows show whether the residual grows while the signal
                    # is steady; the release windows divide a decaying signal.
                    held = windows[:int(len(windows) * .75)]
                    peak = max(windows)
                    # The envelope's own sweep settles the residual over the
                    # first windows; growth is the second half of the held part
                    # rising above the first half.
                    half = len(held) // 2
                    growing = max(held[half:]) > max(held[:half]) + args.grows_db
                    key = '%dx/%s/%s' % (factor, preset, control)
                    report['results'][key] = {'max_window_db_re_signal': peak,
                                              'max_held_window_db_re_signal': max(held),
                                              'held_second_half_minus_first_db': max(held[half:]) - max(held[:half]),
                                              'windows_db_re_signal': windows, 'grows': growing}
                    if peak > worst[0]:
                        worst = (peak, key)
                    if growing:
                        grows.append(key)
    report['worst_window'] = {'db_re_signal': worst[0], 'where': worst[1]}
    report['growing'] = grows
    print('worst window: %.1f dB re signal at %s; growing: %s' % (worst[0], worst[1], grows or 'none'))
    for key, item in report['results'].items():
        print('  %-32s held max %7.1f, second half %+5.1f dB, release max %7.1f%s' % (
            key, item['max_held_window_db_re_signal'], item['held_second_half_minus_first_db'],
            max(item['windows_db_re_signal'][int(len(item['windows_db_re_signal']) * .75):]),
            ' GROWS' if item['grows'] else ''))
    if args.report:
        args.report.write_text(json.dumps(report, indent=1) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())

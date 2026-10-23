#!/usr/bin/env python3
"""Measure what resuming an idle noise source does, against a Git baseline.

Compiles Tests/NoiseResumeDriver.cpp against the baseline revision's core and
against Source/DSP as checked out, at 1x and 4x, and reports the residual
candidate-minus-baseline before the NOISE control leaves zero (1.5 s), then in
5 ms windows over the first 80 ms and 50 ms windows to the end, as RMS relative
to the baseline RMS of the window and as peak dBFS, with both files' RMS over
50-300 ms after the change. A limiter that runs whether or not NOISE is used
and one that holds its state while unused give the same samples up to 1.5 s
and a different realisation of the same noise for the first milliseconds after.
Example: python3 Tests/check_noise_resume.py --baseline-ref 8655ab1 --report out.json
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
DRIVER = ROOT / 'Tests/NoiseResumeDriver.cpp'
FLAGS = ['-std=c++17', '-O3', '-DNDEBUG']
RATE, ENABLE_SECONDS = 48000, 1.5


def decibels(ratio):
    return -math.inf if ratio <= 0 else 20 * math.log10(ratio)


def rms(values):
    return math.sqrt(sum(v * v for v in values) / len(values)) if values else 0.0


def window(before, after, start, stop):
    b, a = before[start:stop], after[start:stop]
    differences = [y - x for x, y in zip(b, a)]
    signal = rms(b)
    return {'from_ms': (start - ENABLE_SECONDS * RATE) / RATE * 1000,
            'to_ms': (stop - ENABLE_SECONDS * RATE) / RATE * 1000,
            'residual_rms_db_re_signal': decibels(rms(differences) / signal) if signal else None,
            'residual_peak_dbfs': decibels(max(abs(d) for d in differences)),
            'identical_samples': sum(1 for d in differences if d == 0), 'samples': len(b)}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--baseline-ref', required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    baseline = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                        args.baseline_ref + '^{commit}'], cwd=ROOT, text=True).strip()
    report = {'protocol': 'ceres-noise-resume-v1', 'baseline_commit': baseline, 'compiler_flags': FLAGS,
              'rate': RATE, 'enable_seconds': ENABLE_SECONDS, 'factors': {}}
    with tempfile.TemporaryDirectory(prefix='ceres-noise-resume-') as directory:
        directory = Path(directory)
        binaries = {}
        for variant in ('baseline', 'candidate'):
            core = directory / variant
            core.mkdir()
            for name in ('ceres.cpp', 'ceres.hpp'):
                relative = 'Source/DSP/' + name
                content = (subprocess.check_output(['git', 'show', baseline + ':' + relative], cwd=ROOT)
                           if variant == 'baseline' else (ROOT / relative).read_bytes())
                (core / name).write_bytes(content)
            binaries[variant] = core / 'driver'
            subprocess.run(compiler + FLAGS + ['-I', str(core), str(DRIVER), str(core / 'ceres.cpp'),
                                               '-o', str(binaries[variant])], check=True)
        for factor in (1, 4):
            streams = {}
            for variant, binary in binaries.items():
                output = directory / ('%s-%dx.f64' % (variant, factor))
                subprocess.run([str(binary), str(output), str(factor)], check=True)
                samples = array('d')
                samples.frombytes(output.read_bytes())
                streams[variant] = samples
            before, after = streams['baseline'], streams['candidate']
            if len(before) != len(after):
                sys.exit('stream lengths differ at %dx' % factor)
            enable = int(RATE * ENABLE_SECONDS)
            fine = [window(before, after, enable + k * 240, enable + (k + 1) * 240) for k in range(16)]
            coarse = [window(before, after, enable + k * 2400, min(enable + (k + 1) * 2400, len(before)))
                      for k in range(30) if enable + k * 2400 < len(before)]
            settled = slice(enable + int(.05 * RATE), enable + int(.3 * RATE))
            result = {'identical_before_enable': before[:enable] == after[:enable],
                      'windows_5ms': fine, 'windows_50ms': coarse,
                      'rms_50_300ms_after_dbfs': {'baseline': decibels(rms(before[settled])),
                                                  'candidate': decibels(rms(after[settled]))}}
            report['factors'][str(factor)] = result
            print('%dx: identical before the change: %s; RMS 50-300 ms after: baseline %.3f, candidate %.3f dBFS' % (
                factor, result['identical_before_enable'], result['rms_50_300ms_after_dbfs']['baseline'],
                result['rms_50_300ms_after_dbfs']['candidate']))
            for w in fine[:6] + coarse[1:4]:
                print('  %6.0f-%5.0f ms: residual %7.1f dB re signal, peak %7.1f dBFS' % (
                    w['from_ms'], w['to_ms'], w['residual_rms_db_re_signal'], w['residual_peak_dbfs']))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())

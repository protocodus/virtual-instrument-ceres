#!/usr/bin/env python3
"""Null test of the CpuParityDriver stream against a Git baseline, scene by scene.

check_cpu_parity.py asks for byte equality; this companion measures what a
change that is not bit-exact in double precision does at the float32 output.
It compiles Tests/CpuParityDriver.cpp against the baseline revision's core and
against Source/DSP as checked out, with identical compiler flags, runs both and
reports, per scene of the driver's fixed layout (factory preset x {1,4,16}
voices x {1,2,4} quality, then {44.1,48,96,192} kHz x voices x quality, then
three long scenes), the residual candidate-minus-baseline as RMS relative to
the baseline's RMS and as peak dBFS. Byte-identical scenes report -inf.
Example: python3 Tests/check_cpu_null.py --baseline-ref 8655ab1 --report out.json
"""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
from array import array

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / 'Tests/CpuParityDriver.cpp'
FLAGS = ['-std=c++17', '-O3', '-DNDEBUG']
PRESETS = ['init', 'string1', 'string2', 'brass1', 'brass2', 'flute', 'electric_piano',
           'clavichord', 'harpsichord', 'guitar1', 'guitar2', 'funky1', 'funky2', 'funky3']
VOICES, FACTORS, RATES = (1, 4, 16), (1, 2, 4), (44100, 48000, 96000, 192000)


def scenes():
    layout = [('factory/%s/%dv/%dx' % (p, v, f), p, 1536)
              for p in PRESETS for v in VOICES for f in FACTORS]
    layout += [('controller/%dHz/%dv/%dx' % (r, v, f), 'controller', 4096)
               for r in RATES for v in VOICES for f in FACTORS]
    layout += [('long/%dv/%dx' % (VOICES[i], FACTORS[i]), 'long', 32768) for i in range(3)]
    return layout


def decibels(ratio):
    return -math.inf if ratio <= 0 else 20 * math.log10(ratio)


def compile_driver(compiler, core, output):
    command = compiler + FLAGS + ['-I', str(core), str(DRIVER), str(core / 'ceres.cpp'), '-o', str(output)]
    subprocess.run(command, check=True)
    return command


def render(binary, output):
    subprocess.run([str(binary), str(output)], check=True, stderr=subprocess.DEVNULL)
    samples = array('f')
    samples.frombytes(output.read_bytes())
    if samples.itemsize != 4 or not all(map(math.isfinite, samples)):
        raise RuntimeError(str(binary) + ' produced an invalid float32 stream')
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--baseline-ref', required=True, help='Git revision of the baseline core')
    parser.add_argument('--report', type=Path, help='JSON report path')
    parser.add_argument('--worst', type=int, default=10, help='scenes to list, largest residual first')
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get('CXX', 'c++'))
    baseline = subprocess.check_output(['git', 'rev-parse', '--verify', '--end-of-options',
                                        args.baseline_ref + '^{commit}'], cwd=ROOT, text=True).strip()
    with tempfile.TemporaryDirectory(prefix='ceres-cpu-null-') as directory:
        directory = Path(directory)
        cores = {}
        for variant in ('baseline', 'candidate'):
            core = directory / variant
            core.mkdir()
            for name in ('ceres.cpp', 'ceres.hpp'):
                relative = 'Source/DSP/' + name
                content = (subprocess.check_output(['git', 'show', baseline + ':' + relative], cwd=ROOT)
                           if variant == 'baseline' else (ROOT / relative).read_bytes())
                (core / name).write_bytes(content)
            command = compile_driver(compiler, core, core / 'driver')
            cores[variant] = {'core': core, 'command': command}
        streams = {variant: render(item['core'] / 'driver', directory / (variant + '.f32'))
                   for variant, item in cores.items()}
    before, after = streams['baseline'], streams['candidate']
    if len(before) != len(after):
        sys.exit('stream lengths differ: %d versus %d' % (len(before), len(after)))
    layout = scenes()
    if sum(n for _, _, n in layout) != len(before):
        sys.exit('the driver stream does not match the scene layout of this script')
    rows, groups, position = [], {}, 0
    signal_total = residual_total = 0.0
    peak_total = 0.0
    identical_total = 0
    for name, group, count in layout:
        b, c = before[position:position + count], after[position:position + count]
        position += count
        signal = sum(x * x for x in b)
        differences = [y - x for x, y in zip(b, c)]
        residual = sum(d * d for d in differences)
        peak = max(abs(d) for d in differences)
        identical = sum(1 for d in differences if d == 0)
        rows.append({'scene': name, 'group': group, 'samples': count,
                     'baseline_rms_dbfs': decibels(math.sqrt(signal / count)),
                     'residual_rms_db_re_signal': decibels(math.sqrt(residual / signal)) if signal else None,
                     'residual_peak_dbfs': decibels(peak), 'identical_samples': identical})
        entry = groups.setdefault(group, {'signal': 0.0, 'residual': 0.0, 'peak': 0.0})
        entry['signal'] += signal
        entry['residual'] += residual
        entry['peak'] = max(entry['peak'], peak)
        signal_total += signal
        residual_total += residual
        peak_total = max(peak_total, peak)
        identical_total += identical
    report = {
        'protocol': 'ceres-cpu-null-v1', 'baseline_commit': baseline, 'compiler_flags': FLAGS,
        'compile_commands': {variant: item['command'] for variant, item in cores.items()},
        'sample_count': len(before), 'identical_samples': identical_total,
        'byte_identical': identical_total == len(before) and before.tobytes() == after.tobytes(),
        'residual_rms_db_re_signal': decibels(math.sqrt(residual_total / signal_total)),
        'residual_peak_dbfs': decibels(peak_total),
        'groups': {group: {'residual_rms_db_re_signal': decibels(math.sqrt(e['residual'] / e['signal'])),
                           'residual_peak_dbfs': decibels(e['peak'])} for group, e in groups.items()},
        'scenes': rows}
    print('baseline %s: %d/%d samples identical, residual %.1f dB re signal, peak %.1f dBFS%s' % (
        baseline[:12], identical_total, len(before), report['residual_rms_db_re_signal'],
        report['residual_peak_dbfs'], ', byte-identical stream' if report['byte_identical'] else ''))
    for group, entry in report['groups'].items():
        print('  %-16s residual %7.1f dB re signal, peak %7.1f dBFS' % (
            group, entry['residual_rms_db_re_signal'], entry['residual_peak_dbfs']))
    worst = sorted((r for r in rows if r['residual_rms_db_re_signal'] is not None),
                   key=lambda r: -r['residual_rms_db_re_signal'])[:args.worst]
    for row in worst:
        print('  %-36s %7.1f dB re signal, peak %7.1f dBFS' % (
            row['scene'], row['residual_rms_db_re_signal'], row['residual_peak_dbfs']))
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())

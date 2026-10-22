#!/usr/bin/env python3
"""Compare one fixed native DSP timing pass against a Git baseline.

Runs baseline Init, candidate Init, baseline active, candidate active, baseline
configuration matrix, candidate configuration matrix, exactly once in that order.
All output and deadline misses are retained; no retries, selection, or tolerance.
Run without concurrent builds/renders. This is not Rack/host/target CPU evidence.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
FLAGS = ['-std=c++17', '-O3', '-DNDEBUG', '-DCERES_EMBEDDED_TARGET=1',
         '-fno-exceptions', '-Wall', '-Wextra', '-Wpedantic']
DRIVERS = {'native': 'native_performance.cpp', 'configuration': 'benchmark_configuration.cpp'}
RUNS = [('baseline', 'init', 'native', []), ('candidate', 'init', 'native', []),
        ('baseline', 'active', 'native', ['active']), ('candidate', 'active', 'native', ['active']),
        ('baseline', 'configuration', 'configuration', []),
        ('candidate', 'configuration', 'configuration', [])]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def save(handle, report):
    handle.seek(0)
    json.dump(report, handle, indent=2, allow_nan=False)
    handle.write('\n')
    handle.truncate()
    handle.flush()


def command_record(command, label, directory):
    started = time.perf_counter()
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                            encoding='utf-8', errors='replace')
    record = {'command': command, 'returncode': result.returncode,
              'elapsed_seconds': time.perf_counter() - started,
              'stdout': result.stdout, 'stderr': result.stderr}
    for stream in ('stdout', 'stderr'):
        path = directory / (label + '.' + stream + '.log')
        path.write_text(record[stream])
        record[stream + '_log'] = str(path)
    return record


def git_output(*arguments):
    result = subprocess.run(['git', *arguments], cwd=ROOT, capture_output=True)
    if result.returncode:
        raise RuntimeError('git ' + shlex.join(arguments) + ': ' +
                           result.stderr.decode('utf-8', errors='replace').strip())
    return result.stdout


def invalid_json_constant(value):
    raise ValueError('nonfinite JSON constant: ' + value)


def compare(report):
    """Compare exact checksum bits and retain each median ratio independently."""
    indexed = {(run['variant'], run['workload']): run for run in report['runs']}
    comparisons, errors = [], []
    for workload in ('init', 'active', 'configuration'):
        pair = [indexed[variant, workload] for variant in ('baseline', 'candidate')]
        if any('measurement' not in run for run in pair):
            errors.append(workload + ': a timing run did not produce valid JSON')
            continue
        rows = [run['measurement'] for run in pair]
        if workload == 'configuration':
            expected = [(voices, factor) for voices in (1, 4, 16) for factor in (1, 2, 4)]
            if any(not isinstance(items, list) or len(items) != len(expected) or
                   not all(isinstance(item, dict) for item in items) or
                   [(item.get('voices'), item.get('oversampling')) for item in items] != expected
                   for items in rows):
                errors.append('configuration: expected all nine rows in voice/quality order')
                continue
            median_key = 'thread_cpu_median_ms'
        else:
            rows = [[row] for row in rows]
            median_key = 'cpu_median_ms'
        for before, after in zip(*rows):
            label = workload
            try:
                if not isinstance(before, dict) or not isinstance(after, dict):
                    raise ValueError('measurement is not an object')
                if workload == 'configuration':
                    label += '/' + str(before['voices']) + 'v/' + str(before['oversampling']) + 'x'
                checksums = [float(row['checksum']) for row in (before, after)]
                medians = [float(row[median_key]) for row in (before, after)]
                if not all(map(math.isfinite, checksums + medians)) or min(medians) <= 0:
                    raise ValueError('nonfinite checksum or invalid median CPU time')
                checksum_hex = [struct.pack('>d', value).hex() for value in checksums]
                equal = checksum_hex[0] == checksum_hex[1]
                ratio = medians[1] / medians[0]
                comparisons.append({'workload': label, 'checksum_exact': equal,
                                    'baseline_checksum': checksums[0], 'candidate_checksum': checksums[1],
                                    'checksum_float64_bits': checksum_hex,
                                    'baseline_cpu_median_ms': medians[0], 'candidate_cpu_median_ms': medians[1],
                                    'candidate_over_baseline_cpu_median': ratio,
                                    'cpu_median_reduction_percent': (1 - ratio) * 100})
                if not equal:
                    errors.append(label + ': baseline/candidate checksums differ exactly')
            except (KeyError, TypeError, ValueError) as error:
                errors.append(label + ': ' + str(error))
    report['comparisons'] = comparisons
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-ref', default='1cad081', help='Git baseline revision (default: 1cad081)')
    parser.add_argument('--output', type=Path, required=True, help='new JSON report path; must not already exist')
    parser.add_argument('--build-dir', type=Path, help='retain sources, binaries and logs in a new or empty directory')
    args = parser.parse_args()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        handle = output.open('x')
    except FileExistsError:
        parser.error('--output already exists: ' + str(output))

    report = {'status': 'running', 'started_utc': utc_now(),
              'scope': 'One native embedded-ABI synth; no Rack, host or target CPU claim. '
                       'One pass per workload, no retry, selection, removal of overruns or checksum tolerance.',
              'baseline_ref_requested': args.baseline_ref,
              'platform': {'description': platform.platform(), 'uname': dict(platform.uname()._asdict()),
                           'logical_cpu_count': os.cpu_count(), 'python': sys.version},
              'runner_sha256': sha(Path(__file__)), 'compiler_flags': FLAGS,
              'fixed_run_order': [variant + '/' + workload for variant, workload, _, _ in RUNS],
              'compilations': [], 'runs': [], 'errors': []}
    directory = None
    with handle:
        try:
            if args.build_dir:
                directory = args.build_dir.resolve()
                allowed = {output, *output.parents}
                if directory.exists() and (not directory.is_dir() or
                                          any(path not in allowed for path in directory.rglob('*'))):
                    raise ValueError('--build-dir must be a new or empty directory')
                directory.mkdir(parents=True, exist_ok=True)
            else:
                directory = Path(tempfile.mkdtemp(prefix='ceres-cpu-benchmark-'))
            report['build_directory'] = str(directory)
            save(handle, report)
            compiler = shlex.split(os.environ.get('CXX', 'c++'))
            if not compiler:
                raise ValueError('CXX is empty')
            report['compiler'] = command_record(compiler + ['--version'], 'compiler-version', directory)
            if report['compiler']['returncode']:
                raise RuntimeError('compiler --version failed')
            baseline = git_output('rev-parse', '--verify', '--end-of-options', args.baseline_ref + '^{commit}').decode().strip()
            report['baseline_commit'] = baseline
            report['candidate_head'] = git_output('rev-parse', 'HEAD').decode().strip()
            report['sources'] = {}
            for variant in ('baseline', 'candidate'):
                core = directory / variant
                core.mkdir()
                report['sources'][variant] = {}
                for name in ('ceres.cpp', 'ceres.hpp'):
                    relative = 'Source/DSP/' + name
                    content = git_output('show', baseline + ':' + relative) if variant == 'baseline' else (ROOT / relative).read_bytes()
                    destination = core / name
                    destination.write_bytes(content)
                    report['sources'][variant][relative] = {'sha256': sha(destination), 'snapshot': str(destination)}
            drivers = directory / 'drivers'
            drivers.mkdir()
            report['drivers'] = {}
            for name in DRIVERS.values():
                path = drivers / name
                path.write_bytes((ROOT / 'Tools' / name).read_bytes())
                report['drivers']['Tools/' + name] = {'sha256': sha(path), 'snapshot': str(path)}
            binaries = {}
            for variant in ('baseline', 'candidate'):
                for driver, name in DRIVERS.items():
                    binary = directory / (variant + '-' + driver)
                    core = directory / variant
                    command = compiler + FLAGS + ['-I', str(core), str(drivers / name),
                                                 str(core / 'ceres.cpp'), '-o', str(binary)]
                    record = command_record(command, 'compile-' + variant + '-' + driver, directory)
                    record.update({'variant': variant, 'driver': driver})
                    report['compilations'].append(record)
                    save(handle, report)
                    if record['returncode']:
                        raise RuntimeError('compilation failed: ' + variant + '/' + driver)
                    record['binary_sha256'] = sha(binary)
                    binaries[variant, driver] = binary
            for variant, workload, driver, arguments in RUNS:
                record = command_record([str(binaries[variant, driver]), *arguments],
                                        'run-' + variant + '-' + workload, directory)
                record.update({'variant': variant, 'workload': workload,
                               'binary_sha256': sha(binaries[variant, driver])})
                report['runs'].append(record)
                if record['returncode']:
                    report['errors'].append(variant + '/' + workload + ': exit ' + str(record['returncode']))
                else:
                    try:
                        record['measurement'] = json.loads(record['stdout'], parse_constant=invalid_json_constant)
                    except ValueError as error:
                        record['json_error'] = str(error)
                save(handle, report)
            report['errors'].extend(compare(report))
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
            report['errors'].append(type(error).__name__ + ': ' + str(error))
        report['status'] = 'fail' if report['errors'] else 'pass'
        report['completed_utc'] = utc_now()
        retain = bool(args.build_dir) or report['status'] == 'fail'
        report['build_directory_retained'] = retain
        if directory is not None and not retain:
            try:
                shutil.rmtree(directory)
            except OSError as error:
                report['errors'].append('Cannot remove temporary build: ' + str(error))
                report['status'] = 'fail'
                report['build_directory_retained'] = True
        save(handle, report)
    print(report['status'].upper() + ': ' + str(output))
    for row in report.get('comparisons', []):
        print(row['workload'] + ': candidate/baseline median CPU ratio ' +
              format(row['candidate_over_baseline_cpu_median'], '.6f') +
              '; exact checksum ' + ('PASS' if row['checksum_exact'] else 'FAIL'))
    if report['status'] == 'fail':
        for error in report['errors']:
            print(error, file=sys.stderr)
        if directory is not None:
            print('Retained build/logs: ' + str(directory), file=sys.stderr)
    return 0 if report['status'] == 'pass' else 1


if __name__ == '__main__':
    sys.exit(main())

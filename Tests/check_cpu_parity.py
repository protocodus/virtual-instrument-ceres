#!/usr/bin/env python3
"""Run baseline/candidate CpuParityDriver binaries and require byte-exact audio.

Compile the same Tests/CpuParityDriver.cpp against each version of the DSP,
using the same compiler/flags. No frozen recordings or tolerance are used.
Example: python3 Tests/check_cpu_parity.py --baseline /tmp/cpu-before \
             --candidate build/ceres_cpu_parity --report /tmp/cpu-parity.json
"""

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = {'protocol': 'ceres-cpu-parity-v1', 'byte_order': sys.byteorder,
              'comparison': 'byte-exact IEEE float32', 'variants': {}}
    streams = {}
    with tempfile.TemporaryDirectory(prefix='ceres-cpu-parity-') as directory:
        for name in ('baseline', 'candidate'):
            binary = getattr(args, name).resolve()
            output = Path(directory) / (name + '.f32')
            started = time.perf_counter()
            subprocess.run([str(binary), str(output)], check=True)
            elapsed = time.perf_counter() - started
            data = output.read_bytes()
            if not data or len(data) % 4:
                raise RuntimeError(name + ' output is not a nonempty float32 stream')
            samples = array('f')
            samples.frombytes(data)
            if samples.itemsize != 4 or not all(map(math.isfinite, samples)):
                raise RuntimeError(name + ' output contains invalid float32 samples')
            streams[name] = (data, samples)
            report['variants'][name] = {
                'executable': str(binary), 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                'audio_sha256': hashlib.sha256(data).hexdigest(),
                'sample_count': len(samples), 'elapsed_seconds': round(elapsed, 6),
            }

    before_bytes, before = streams['baseline']
    after_bytes, after = streams['candidate']
    identical = before_bytes == after_bytes
    common = min(len(before), len(after))
    first_mismatch = None
    if not identical:
        for index in range(common):
            offset = index * 4
            if before_bytes[offset:offset + 4] != after_bytes[offset:offset + 4]:
                first_mismatch = {
                    'sample_index': index,
                    'baseline': before[index], 'candidate': after[index],
                    'baseline_bytes': before_bytes[offset:offset + 4].hex(),
                    'candidate_bytes': after_bytes[offset:offset + 4].hex(),
                }
                break
        if first_mismatch is None:
            first_mismatch = {'sample_index': common, 'reason': 'stream lengths differ'}
    report.update({
        'status': 'pass' if identical else 'fail',
        'sample_count': len(before) if len(before) == len(after) else None,
        'compared_sample_count': common,
        'first_mismatch': first_mismatch,
        'maximum_absolute_difference': max((abs(a - b) for a, b in zip(before, after)), default=0.0),
    })
    text = json.dumps(report, indent=2) + '\n'
    if args.report:
        args.report.write_text(text)
    print(text, end='')
    return 0 if identical else 1


if __name__ == '__main__':
    sys.exit(main())

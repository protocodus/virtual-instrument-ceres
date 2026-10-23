#!/usr/bin/env python3
"""Require byte-exact actual Rack output for the approved 23-patch bank.

Both variants compile the same driver and baseline Rack MOM harness with
identical flags, but each variant's actual wrapper and engine sources. Source,
binary, bank and audio hashes are retained alongside scene offsets and logs.
This is native callback equivalence; it does not measure Reason/SDK-target CPU.
"""
import argparse
from array import array
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


DRIVER = Path(__file__).with_name('RackCpuParityDriver.cpp')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_bank(root, keys):
    bank = {}
    for path in sorted((root / 'Resources/Public').rglob('*.repatch')):
        entries = ET.parse(path).findall('.//Object[@name="custom_properties"]/Value')
        values = {entry.attrib['property']: float(entry.text) for entry in entries}
        if len(entries) != len(values) or set(values) != set(keys):
            raise RuntimeError(f'{path}: incomplete/duplicate/unknown saved properties')
        if not all(math.isfinite(v) for v in values.values()):
            raise RuntimeError(f'{path}: nonfinite saved property')
        bank[path.relative_to(root / 'Resources/Public').as_posix()] = values
    if len(bank) != 23 or 'Init.repatch' not in bank:
        raise RuntimeError(f'{root}: expected all 23 approved patches including Init')
    return bank


def read_scenes(path):
    scenes = []
    expected = 0
    for line in path.read_text().splitlines():
        name, start, count, peak, stereo = line.split('\t')
        scene = dict(name=name, start_sample=int(start), sample_count=int(count),
                     peak=float(peak), stereo=bool(int(stereo)))
        if scene['start_sample'] != expected or scene['sample_count'] <= 0:
            raise RuntimeError('scene map does not partition the audio stream')
        expected += scene['sample_count']
        scenes.append(scene)
    if len(scenes) != 165:
        raise RuntimeError(f'expected 69 bank + 66 control + 30 reset scenes, got {len(scenes)}')
    return scenes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-rack', type=Path, required=True)
    parser.add_argument('--candidate-rack', type=Path, required=True)
    parser.add_argument('--baseline-instrument', type=Path)
    parser.add_argument('--candidate-instrument', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error('--output must be a new or empty directory; prior evidence is never overwritten')
    output.mkdir(parents=True, exist_ok=True)
    roots = {name: getattr(args, name + '_rack').resolve() for name in ('baseline', 'candidate')}
    specs = {name: json.loads((root / 'Tools/parameters.json').read_text())['parameters']
             for name, root in roots.items()}
    if specs['baseline'] != specs['candidate']:
        raise RuntimeError('control/default contract changed; this is not a pure CPU optimization')
    keys = [row['key'] for row in specs['baseline']]
    if len(keys) != 66 or len(set(keys)) != 66:
        raise RuntimeError('expected the complete 66-control Rack contract')
    if (roots['baseline'] / 'Parameters.h').read_bytes() != (roots['candidate'] / 'Parameters.h').read_bytes():
        raise RuntimeError('generated parameter/default contracts differ')
    banks = {name: read_bank(root, keys) for name, root in roots.items()}
    if banks['baseline'] != banks['candidate']:
        raise RuntimeError('serialized preset values changed')
    # Filenames are percent encoded solely to keep the driver format tokenized.
    from urllib.parse import quote
    bank_file = output / 'bank.txt'
    with bank_file.open('w') as file:
        print(len(banks['baseline']), file=file)
        for name, values in banks['baseline'].items():
            print(quote(name, safe='/.'), file=file)
            for key in keys:
                print(key, repr(values[key]), file=file)
    report = {
        'protocol': 'ceres-rack-cpu-parity-v1',
        'comparison': 'byte-exact interleaved native-endian IEEE float32 stereo',
        'byte_order': sys.byteorder, 'block_frames': 64, 'engine_seeds': [50, 51],
        'coverage': {'bank': list(banks['baseline']), 'bank_qualities': [1, 2, 4],
                     'controls': keys, 'continuous_positions': [0, .5, 1],
                     'selectors': 'every legal state',
                     'reset_rates': [44100, 48000, 88200, 96000, 192000],
                     'mode': 'saved mode plus live Mono/Double transitions and reversal',
                     'notes': 'E2 G2 B2 velocity100, repeated keys, sustain, release, reset',
                     'limits': 'native wrapper; control sweep complements run_native reachability tests'},
        'driver_sha256': sha(DRIVER), 'bank_sha256': sha(bank_file), 'variants': {},
    }
    streams = {}
    for name, rack in roots.items():
        instrument = (getattr(args, name + '_instrument') or rack / 'Instrument').resolve()
        source = instrument / 'Source'
        source_files = [DRIVER, roots['baseline'] / 'Tests/test_wrapper.cpp',
                        roots['baseline'] / 'Tests/Mock/Jukebox.h', rack / 'CeresRack.cpp',
                        rack / 'Ceres.h', rack / 'Parameters.h', rack / 'OutputProtection.h',
                        rack / 'FactoryMemories.h', rack / 'JukeboxExports.cpp',
                        rack / 'Tools/parameters.json', rack / 'Tools/preset_library.json']
        source_files += sorted((source / 'DSP').rglob('*.cpp')) + sorted((source / 'DSP').rglob('*.hpp'))
        source_files += sorted((rack / 'Resources/Public').rglob('*.repatch'))
        source_hashes = {str(p): sha(p) for p in source_files}
        binary = output / (name + '-rack-parity')
        # The baseline test harness is intentionally shared. Include resolution
        # selects each variant's Ceres.h and core from the following directories.
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-O2', '-DNDEBUG',
                   '-Wall', '-Wextra', '-Werror', '-DCERES_EMBEDDED_TARGET=1',
                   '-fno-exceptions', '-fno-rtti',
                   '-I' + str(roots['baseline'] / 'Tests/Mock'), '-I' + str(rack),
                   '-I' + str(source), '-I' + str(roots['baseline'] / 'Tests'),
                   str(DRIVER), str(rack / 'CeresRack.cpp'), str(rack / 'JukeboxExports.cpp'),
                   str(source / 'DSP/ceres.cpp'), '-o', str(binary)]
        with (output / (name + '-compile.log')).open('w') as log:
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
        audio = output / (name + '.f32')
        scene_file = output / (name + '-scenes.tsv')
        with (output / (name + '-render.log')).open('w') as log:
            subprocess.run([str(binary), str(bank_file), str(audio), str(scene_file)],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        data = audio.read_bytes()
        samples = array('f')
        if not data or len(data) % 8 or samples.itemsize != 4:
            raise RuntimeError('invalid stereo float32 stream')
        samples.frombytes(data)
        if not all(map(math.isfinite, samples)):
            raise RuntimeError('nonfinite output')
        scenes = read_scenes(scene_file)
        if sum(scene['sample_count'] for scene in scenes) != len(samples):
            raise RuntimeError('scene manifest/audio length mismatch')
        if source_hashes != {str(p): sha(p) for p in source_files}:
            raise RuntimeError(name + ' sources changed during compilation/render; run is invalid')
        report['variants'][name] = {
            'rack': str(rack), 'instrument': str(instrument), 'command': command,
            'binary_sha256': sha(binary), 'audio_sha256': sha(audio),
            'sample_count': len(samples), 'scenes': scenes,
            'source_sha256': source_hashes, 'source_hashes_verified_before_and_after': True,
        }
        streams[name] = (data, samples)
    for name, variant in report['variants'].items():
        if variant['source_sha256'] != {path: sha(Path(path)) for path in variant['source_sha256']}:
            raise RuntimeError(name + ' sources changed before comparison; run is invalid')
    before, after = streams['baseline'], streams['candidate']
    identical = before[0] == after[0]
    common = min(len(before[1]), len(after[1]))
    mismatch = None
    if not identical:
        for i in range(common):
            if before[0][4*i:4*i+4] != after[0][4*i:4*i+4]:
                scene = next(s for s in report['variants']['baseline']['scenes']
                             if s['start_sample'] <= i < s['start_sample'] + s['sample_count'])
                mismatch = {'sample': i, 'scene': scene['name'], 'scene_frame': (i-scene['start_sample'])//2,
                            'channel': i % 2, 'baseline': before[1][i], 'candidate': after[1][i],
                            'baseline_bits': before[0][4*i:4*i+4].hex(),
                            'candidate_bits': after[0][4*i:4*i+4].hex()}
                break
        if mismatch is None:
            mismatch = {'sample': common, 'reason': 'stream lengths differ'}
    report.update(status='pass' if identical else 'fail',
                  compared_sample_count=common, first_mismatch=mismatch,
                  maximum_absolute_difference=max(abs(a-b) for a, b in zip(before[1], after[1])))
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('status', 'compared_sample_count', 'first_mismatch',
                                                  'maximum_absolute_difference')}, indent=2))
    return 0 if identical else 1


if __name__ == '__main__':
    sys.exit(main())

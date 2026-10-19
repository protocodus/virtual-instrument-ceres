#!/usr/bin/env python3
"""Analyze the declared fixed-gain capacitor scenes and assemble listening clips."""
import hashlib
import json
import math
import struct
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RUN = ROOT / 'capacitor-run-001'


def audio(path):
    data = path.read_bytes()
    offset = 12
    while offset + 8 <= len(data):
        size = struct.unpack_from('<I', data, offset + 4)[0]
        if data[offset:offset+4] == b'data':
            result = array('f')
            result.frombytes(data[offset+8:offset+8+size])
            return result
        offset += 8 + size + size % 2
    raise ValueError('no WAV data')


def rms(values):
    return math.sqrt(sum(x*x for x in values) / len(values))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    stats = {}
    plan = json.loads((ROOT / 'capacitor-plan.json').read_text())
    manifest = json.loads((RUN / 'manifest.json').read_text())
    assert manifest['plan_sha256'] == sha(ROOT / 'capacitor-plan.json')
    assert manifest['driver_sha256'] == sha(ROOT / 'capacitor_demo.cpp')
    for scene in plan['scene_order']:
        paths = [RUN / f'{variant}-{scene}.wav' for variant in ('parent', 'capacitors')]
        for variant, path in zip(('parent', 'capacitors'), paths):
            assert sha(path) == manifest['renders'][f'{variant}-{scene}']['wav_sha256']
        a, b = map(audio, paths)
        windows = ([(.95, 1), (1.02, 1.12), (1.5, 1.7), (5.8, 6.2)]
                   if 'q_' in scene else [(.55, .60), (.60, .70), (.70, .80), (.9, 1.1)])
        rows = []
        for begin, end in windows:
            j, k = round(begin*48000), round(end*48000)
            x, y = a[j:k], b[j:k]
            error = rms([u-v for u, v in zip(x, y)])
            rows.append(dict(seconds=[begin, end], parent_rms=rms(x), candidate_rms=rms(y),
                             candidate_parent_db=20*math.log10(rms(y)/rms(x)), difference_rms=error,
                             difference_to_parent_db=None if error == 0 else 20*math.log10(error/rms(x))))
        stats[scene] = dict(byte_identical=a.tobytes() == b.tobytes(), sample_count=len(a),
                            max_sample_difference=max(abs(x-y) for x, y in zip(a, b)), windows=rows)
    assert stats['lp_q_static']['byte_identical'] and stats['il_static']['byte_identical']
    assert all(not stats[s]['byte_identical'] for s in ('lp_q_step', 'hp_q_step', 'idle_il'))
    result = dict(manifest_sha256=sha(RUN/'manifest.json'), plan_sha256=sha(ROOT/'capacitor-plan.json'),
                  analysis_script_sha256=sha(Path(__file__)), scenes=stats,
                  interpretation='Fixed-gain native A/B; transient differences with exact static controls. No hardware accuracy claim or fit.')
    (RUN/'analysis.json').write_text(json.dumps(result, indent=2)+'\n')
    output, segments = array('f'), []
    for scene, begin, end in [('lp_q_step', .75, 3.75), ('hp_q_step', .75, 3.75), ('idle_il', .5, 1.2)]:
        for variant in ('parent', 'capacitors'):
            if output:
                output.extend(array('f', [0]) * 12000)
            start = len(output)
            samples = audio(RUN/f'{variant}-{scene}.wav')
            output.extend(samples[round(begin*48000):round(end*48000)])
            segments.append(dict(scene=scene, variant=variant, source_seconds=[begin, end],
                                 start_seconds=start/48000, end_seconds=len(output)/48000))
    data = output.tobytes()
    fmt = struct.pack('<HHIIHH', 3, 1, 48000, 192000, 4, 32)
    wav = (b'RIFF'+struct.pack('<I', 48+len(data))+b'WAVEfmt '+struct.pack('<I', 16)+fmt+
           b'fact'+struct.pack('<II', 4, len(output))+b'data'+struct.pack('<I', len(data))+data)
    (RUN/'comparison.wav').write_bytes(wav)
    (RUN/'listening.json').write_text(json.dumps(dict(wav_sha256=sha(RUN/'comparison.wav'), gain=1,
                                                   fades=False, segments=segments), indent=2)+'\n')
    print(json.dumps(dict(static_controls_exact=True, analysis_sha256=sha(RUN/'analysis.json'),
                          listening_seconds=len(output)/48000,
                          first_after_edit_delta_db={s: stats[s]['windows'][1]['candidate_parent_db']
                                                     for s in ('lp_q_step', 'hp_q_step', 'idle_il')}), indent=2))


if __name__ == '__main__':
    main()

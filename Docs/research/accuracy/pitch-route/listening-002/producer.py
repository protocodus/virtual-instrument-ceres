#!/usr/bin/env python3
"""Derived comparison playback only; canonical mono downmix, no fitting, normalization or fades."""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import sys
import traceback

import numpy as np
import scipy
from scipy.io import wavfile

FOLDER = Path(__file__).resolve().parent
ROOT = FOLDER.parents[3]
PLAN = FOLDER.parent / 'plan.json'
NATIVE_REPORT = FOLDER.parent / 'run-001/report.json'
MANIFEST = FOLDER / 'manifest.json'
OUTPUT = FOLDER / 'comparison.wav'
RATE = 48000
GAP = 12000
ROUTES = ('shared', 'vco-only', 'static-master')


def path(value):
    p = Path(value)
    return p if p.is_absolute() else ROOT / p


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def write(p, value):
    with Path(p).open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write('\n')


def declare():
    assert not MANIFEST.exists() and not OUTPUT.exists(), 'Preserve prior attempts'
    plan, report = read(PLAN), read(NATIVE_REPORT)
    assert sha(PLAN) == report['plan_sha256']
    assert report['native_render_count'] == 24
    receipts = {r['id']: r for r in report['render_receipts']}
    groups = []
    for c in plan['cases']:
        if c['role'] == 'matrix':
            key = (c['original_case']['id'], c['context'])
            if key not in groups:
                groups.append(key)
    assert len(groups) == 6
    bindings = {str(p): sha(p) for p in (PLAN, NATIVE_REPORT, Path(__file__).resolve())}
    previous = FOLDER.parent / 'listening-001'
    bindings.update({str(previous / name): sha(previous / name) for name in ('producer.py', 'manifest.json', 'failure.json')})
    segments, gaps = [], []
    cursor = 0
    for group_index, (clip_id, context) in enumerate(groups):
        cases = [next(c for c in plan['cases'] if c['role'] == 'matrix'
                      and c['original_case']['id'] == clip_id and c['context'] == context
                      and c['route'] == route) for route in ROUTES]
        clip = cases[0]['original_case']
        assert all(c['original_case'] == clip for c in cases)
        frames = round(clip['duration'] * RATE)
        assert abs(clip['duration'] * RATE - frames) < 1e-8
        source = path(clip['reference'])
        bindings[str(source)] = sha(source)
        expected_source = plan['input_sha256'].get(str(source), plan['input_sha256'].get(clip['reference']))
        assert bindings[str(source)] == expected_source
        entries = [(None, source, 0, 1.)]
        for c in cases:
            p = path(c['audio'])
            assert p == path(receipts[c['id']]['audio'])
            bindings[str(p)] = sha(p)
            assert bindings[str(p)] == receipts[c['id']]['audio_sha256']
            entries.append((c, p, round(clip['offset_seconds'] * RATE), clip['recording_gain']))
        for c, p, offset, gain in entries:
            if segments:
                gaps.append(dict(start_frame=cursor, frames=GAP, seconds=.25))
                cursor += GAP
            source_kind = 'public_recording' if c is None else 'native_render'
            route = 'source' if c is None else c['route']
            segments.append(dict(index=len(segments) + 1, group=group_index + 1,
                clip=clip_id, context=context, route=route, source_kind=source_kind,
                label=f'{clip_id} / {context} / {route}', start_frame=cursor, frames=frames,
                start_seconds=cursor / RATE, duration_seconds=frames / RATE,
                input_audio=str(p), input_sha256=bindings[str(p)], input_start_frame=offset,
                expected_input_channels=2 if c is None else 1,
                channel_conversion='arithmetic mean after float64 conversion' if c is None else 'unchanged mono',
                input_end_frame_exclusive=offset + frames, recording_gain=gain,
                common_playback_gain=1., combined_gain=gain,
                case_id=None if c is None else c['id'], source_url=clip['source_url'],
                creator=clip['creator'], source_seconds=clip['source_seconds'],
                common_master_semitones=plan['common_master_semitones']))
            cursor += frames
    assert len(segments) == 24 and len(gaps) == 23
    write(MANIFEST, dict(status='declared_before_audio_decoding_or_output', input_sha256=bindings,
        sample_rate=RATE, sample_format='float32', channels=1, output=str(OUTPUT),
        groups=[dict(clip=c, context=k) for c, k in groups], segments=segments, gaps=gaps,
        frames=cursor, duration_seconds=cursor / RATE, common_playback_gain=1.,
        common_master_semitones=plan['common_master_semitones'],
        common_master_origin=plan['common_master_origin'],
        ordering='Fixed declared clip/context order; source, shared, vco-only, static-master.',
        cropping='Source starts at its existing excerpt frame 0; native starts at original offset_seconds. All lengths use original duration. No fades; hard cuts are retained.',
        sample_conversion='Exactly run_battery.audio: convert signed PCM by full-scale division or float input to float64, then arithmetic-mean stereo source channels. Native is mono. Apply frozen native recording gain and common gain 1; round once to float32.',
        source_channel_policy='Derived source is canonical benchmark mono, not preservation of original stereo channels. Original stereo source files remain byte-unchanged.',
        previous_failed_attempt=str(previous), previous_failure_reason='Initial producer required mono inputs; existing source excerpts are stereo. No WAV was written and no normalization occurred.',
        per_segment_normalization=False, fades=False, effects=False, parameter_fits=0,
        gain_fits=0, native_rerenders=0, original_native_render_count_unchanged=24,
        purpose='Derived listening comparison, not a newly synthesized native validation case.',
        rights='Public recordings remain local analysis material. This artifact does not imply permission to redistribute creator audio.'))
    print('MANIFEST', sha(MANIFEST), flush=True)


def verify_inputs():
    manifest = read(MANIFEST)
    assert manifest['status'] == 'declared_before_audio_decoding_or_output'
    assert manifest['sample_rate'] == RATE and manifest['common_playback_gain'] == 1.
    assert len(manifest['segments']) == 24 and len(manifest['gaps']) == 23
    for p, digest in manifest['input_sha256'].items():
        assert sha(p) == digest, p
    return manifest


def expected_segment(segment):
    rate, samples = wavfile.read(segment['input_audio'])
    assert rate == RATE and samples.ndim in (1, 2)
    channels = 1 if samples.ndim == 1 else samples.shape[1]
    assert channels == segment['expected_input_channels']
    dtype = str(samples.dtype)
    if samples.dtype.kind == 'i':
        samples = samples.astype(np.float64) / 2 ** (8 * samples.dtype.itemsize - 1)
    else:
        assert samples.dtype.kind == 'f', dtype
        samples = samples.astype(np.float64)
    if samples.ndim == 2:
        samples = samples.mean(axis=1)
    start, end = segment['input_start_frame'], segment['input_end_frame_exclusive']
    assert 0 <= start < end <= len(samples)
    assert end - start == segment['frames']
    samples = samples[start:end] * segment['recording_gain'] * segment['common_playback_gain']
    assert np.isfinite(samples).all()
    converted = samples.astype(np.float32)
    assert np.isfinite(converted).all()
    return converted, dtype


def render():
    manifest = verify_inputs()
    assert not OUTPUT.exists() and not (FOLDER / 'report.json').exists()
    assert not (FOLDER / 'failure.json').exists(), 'Preserve failed attempts'
    try:
        output = np.zeros(manifest['frames'], dtype=np.float32)
        rows = []
        for segment in manifest['segments']:
            samples, dtype = expected_segment(segment)
            start, count = segment['start_frame'], segment['frames']
            output[start:start + count] = samples
            peak = float(np.max(np.abs(samples)))
            rows.append(dict(index=segment['index'], label=segment['label'], input_dtype=dtype,
                frames=count, peak=peak, RMS=float(np.sqrt(np.mean(samples.astype(np.float64) ** 2))),
                float32_sample_bytes_sha256=hashlib.sha256(samples.tobytes()).hexdigest()))
        peak = float(np.max(np.abs(output)))
        assert np.isfinite(output).all() and peak < 1., ('Playback would clip; no normalization allowed', peak)
        wavfile.write(OUTPUT, RATE, output)
        verify_inputs()
        write(FOLDER / 'report.json', dict(status='pass', manifest_sha256=sha(MANIFEST),
            output=str(OUTPUT), output_sha256=sha(OUTPUT), sample_rate=RATE, sample_format='float32',
            channels=1, frames=len(output), duration_seconds=len(output) / RATE,
            peak=peak, peak_dbfs=20 * float(np.log10(peak)), segments=rows, gap_count=23,
            common_playback_gain=1., per_segment_normalization=False, fades=False,
            effects=False, native_rerenders=0, original_native_render_count_unchanged=24,
            runtime=dict(python=sys.version, numpy=np.__version__, scipy=scipy.__version__, platform=platform.platform()),
            rights=manifest['rights']))
        print('LISTENING', str(OUTPUT), 'REPORT', sha(FOLDER / 'report.json'), flush=True)
    except Exception:
        write(FOLDER / 'failure.json', dict(status='failed_attempt_preserved', manifest_sha256=sha(MANIFEST),
            traceback=traceback.format_exc(), output_exists=OUTPUT.exists(), normalization_performed=False))
        raise


def check():
    assert not (FOLDER / 'verification.json').exists(), 'Preserve previous verification'
    manifest = verify_inputs()
    report = read(FOLDER / 'report.json')
    assert report['manifest_sha256'] == sha(MANIFEST) and report['output_sha256'] == sha(OUTPUT)
    rate, samples = wavfile.read(OUTPUT)
    assert rate == RATE and samples.dtype == np.dtype('float32') and samples.ndim == 1
    assert len(samples) == manifest['frames'] and np.isfinite(samples).all()
    assert float(np.max(np.abs(samples))) == report['peak'] < 1.
    rows = []
    assert len(report['segments']) == 24
    for segment, original in zip(manifest['segments'], report['segments']):
        expected, dtype = expected_segment(segment)
        start = segment['start_frame']
        actual = samples[start:start + segment['frames']]
        assert actual.tobytes() == expected.tobytes(), segment['label']
        assert hashlib.sha256(actual.tobytes()).hexdigest() == original['float32_sample_bytes_sha256']
        rows.append(dict(index=segment['index'], float32_sample_bytes_exact=True,
                         frozen_gain_exact=segment['combined_gain'] == segment['recording_gain'],
                         input_dtype=dtype))
    for gap in manifest['gaps']:
        value = samples[gap['start_frame']:gap['start_frame'] + gap['frames']]
        assert value.tobytes() == bytes(4 * GAP)
    verify_inputs()
    write(FOLDER / 'verification.json', dict(status='pass', report_sha256=sha(FOLDER / 'report.json'),
        manifest_sha256=sha(MANIFEST), output_sha256=sha(OUTPUT), segments=rows,
        exact_zero_gap_count=23, native_rerenders=0, per_segment_normalization=False,
        all_float32_segment_bytes_exact=True, all_input_hashes_unchanged=True))
    print('VERIFIED', sha(FOLDER / 'verification.json'), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('declare', 'render', 'check'))
    args = parser.parse_args()
    {'declare': declare, 'render': render, 'check': check}[args.command]()

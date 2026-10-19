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
    import numpy as np
    run = ROOT / 'oscillator-run-001'
    manifest = json.loads((run/'manifest.json').read_text())
    assert manifest['plan_sha256'] == sha(ROOT/'oscillator-plan.json')
    assert manifest['driver_sha256'] == sha(ROOT/'oscillator_demo.cpp')
    rows = []
    for wave in ('saw', 'sine', 'pulse'):
        tracks = {}
        for variant in ('parent', 'oscillator'):
            path = run/f'{variant}-{wave}.wav'
            assert sha(path) == manifest['renders'][f'{variant}-{wave}']['wav_sha256']
            tracks[variant] = np.array(audio(path), dtype=float)
        for index, note in enumerate((69, 81, 93, 105)):
            frequency = 440 * 2**((note-69)/12)
            start, end = index*.9+.3, index*.9+.5
            j, k = round(start*48000), round(end*48000)
            metrics = {}
            for variant, track in tracks.items():
                segment = track[j:k]
                time = np.arange(len(segment))/48000
                harmonics = np.array([abs(np.sum(segment*np.exp(-2j*np.pi*frequency*h*time)))*2/len(segment)
                                      for h in range(1, int(20000/frequency)+1)])
                metrics[variant] = dict(rms=rms(segment), h1=float(harmonics[0]),
                                       thd_db=float(20*np.log10(np.linalg.norm(harmonics[1:])/harmonics[0])),
                                       harmonic_amplitudes=harmonics.tolist())
            x, y = tracks['parent'][j:k], tracks['oscillator'][j:k]
            difference = rms(x-y)
            rows.append(dict(wave=wave, note=note, frequency=frequency, seconds=[start, end],
                             byte_identical=np.array_equal(x,y), variants=metrics,
                             difference_to_parent_db=None if difference == 0 else 20*math.log10(difference/rms(x))))
    assert all(row['byte_identical'] for row in rows if row['wave'] == 'pulse')
    result = dict(manifest_sha256=sha(run/'manifest.json'), analysis_script_sha256=sha(Path(__file__)),
                  scope='Fixed known-frequency native waveform harmonic analysis; not a hardware comparison. THD includes harmonics up to20kHz.', rows=rows)
    (run/'analysis.json').write_text(json.dumps(result, indent=2)+'\n')
    output, segments = array('f'), []
    for wave in ('saw', 'sine'):
        for variant in ('parent', 'oscillator'):
            if output:
                output.extend(array('f', [0])*12000)
            start = len(output)
            output.extend(audio(run/f'{variant}-{wave}.wav'))
            segments.append(dict(wave=wave, variant=variant, start_seconds=start/48000, end_seconds=len(output)/48000))
    data = output.tobytes()
    fmt = struct.pack('<HHIIHH',3,1,48000,192000,4,32)
    wav = (b'RIFF'+struct.pack('<I',48+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+
           b'fact'+struct.pack('<II',4,len(output))+b'data'+struct.pack('<I',len(data))+data)
    (run/'comparison.wav').write_bytes(wav)
    (run/'listening.json').write_text(json.dumps(dict(wav_sha256=sha(run/'comparison.wav'),gain=1,fades=False,segments=segments),indent=2)+'\n')
    print(json.dumps(dict(pulse_exact=True, seconds=len(output)/48000,
                          sine_thd_db={row['note']:{v:row['variants'][v]['thd_db'] for v in ('parent','oscillator')}
                                       for row in rows if row['wave']=='sine'}),indent=2))


if __name__ == '__main__':
    main()

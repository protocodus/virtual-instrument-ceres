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
    run=ROOT/'ring-switch-run-001';base=ROOT/'ring-run-001'
    plan=json.loads((ROOT/'ring-switch-plan.json').read_text())
    manifest=json.loads((run/'manifest.json').read_text())
    assert manifest['plan_sha256']==sha(ROOT/'ring-switch-plan.json')
    assert manifest['driver_sha256']==sha(ROOT/'ring_demo.cpp')
    rows=[];output,segments=array('f'),[]
    for rate in (1,30,220):
        tracks={}
        for variant in ('parent','ring','switching'):
            path=(run if variant=='switching' else base)/f'{variant}-{rate}.wav'
            expected=(manifest['renders'][str(rate)]['wav_sha256'] if variant=='switching'
                      else plan['reused_wav_sha256'][f'{variant}-{rate}'])
            assert sha(path)==expected
            tracks[variant]=np.array(audio(path),dtype=float)
        j,k=120000,216000;time=np.arange(k-j)/48000
        frequencies=sorted({440+n*rate for n in range(-5,6) if 20<=440+n*rate<=20000})
        metrics={}
        for variant,x in tracks.items():
            segment=x[j:k]
            metrics[variant]=dict(rms=rms(segment),peak=float(np.max(np.abs(x))),
                                 frequencies_hz=frequencies,
                                 amplitudes=[float(abs(np.sum(segment*np.exp(-2j*np.pi*f*time)))*2/len(segment)) for f in frequencies])
        rows.append(dict(rate=rate,measurement_seconds=[2.5,4.5],variants=metrics,
                         physical_parent_rms_ratio=metrics['switching']['rms']/metrics['parent']['rms'],
                         physical_passive_rms_ratio=metrics['switching']['rms']/metrics['ring']['rms']))
        for variant in ('parent','ring','switching'):
            if output:output.extend(array('f',[0])*12000)
            start=len(output);output.extend(array('f',tracks[variant][96000:192000]))
            segments.append(dict(rate=rate,variant=variant,source_seconds=[2,4],start_seconds=start/48000,end_seconds=len(output)/48000))
    result=dict(manifest_sha256=sha(run/'manifest.json'),base_manifest_sha256=sha(base/'manifest.json'),
                analysis_script_sha256=sha(Path(__file__)),rows=rows,
                scope='Fixed native sine440Hz ring scenes, original ideal carrier -> passive network -> physical BJT steering. No calibration against hardware wet gain. Coincident sidebands at220Hz are reported as combined measured frequencies, not isolated carrier coefficients.')
    (run/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    data=output.tobytes();fmt=struct.pack('<HHIIHH',3,1,48000,192000,4,32)
    wav=(b'RIFF'+struct.pack('<I',48+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+
         b'fact'+struct.pack('<II',4,len(output))+b'data'+struct.pack('<I',len(data))+data)
    (run/'comparison.wav').write_bytes(wav)
    (run/'listening.json').write_text(json.dumps(dict(wav_sha256=sha(run/'comparison.wav'),gain=1,fades=False,segments=segments),indent=2)+'\n')
    print(json.dumps(dict(seconds=len(output)/48000,physical_parent_rms_ratios={row['rate']:row['physical_parent_rms_ratio'] for row in rows}),indent=2))


if __name__=='__main__':main()

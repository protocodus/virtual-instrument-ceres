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
    import cmath
    import importlib.util
    import numpy as np
    run = ROOT/'ring-run-001'
    manifest = json.loads((run/'manifest.json').read_text())
    assert manifest['plan_sha256'] == sha(ROOT/'ring-plan.json')
    assert manifest['driver_sha256'] == sha(ROOT/'ring_demo.cpp')
    source = ROOT.parent/'audible-osc-ring/passive_carrier.py'
    spec = importlib.util.spec_from_file_location('carrier_oracle', source)
    oracle = importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
    rows=[]
    for rate in (1,30,220):
        tracks={}
        for variant in ('parent','ring'):
            path=run/f'{variant}-{rate}.wav'
            assert sha(path)==manifest['renders'][f'{variant}-{rate}']['wav_sha256']
            tracks[variant]=np.array(audio(path),dtype=float)
        j,k=120000,216000
        time=np.arange(k-j)/48000
        peaks={v:{f:complex(np.sum(track[j:k]*np.exp(-2j*np.pi*f*time))*2/(k-j))
                  for f in (440-rate,440+rate)} for v,track in tracks.items()}
        ratios=[peaks['ring'][f]/peaks['parent'][f] for f in (440-rate,440+rate)]
        predicted=oracle.network(rate)
        rows.append(dict(rate=rate,seconds=[2.5,4.5],
                         output_rms_ratio=rms(tracks['ring'][j:k])/rms(tracks['parent'][j:k]),
                         sideband_ratios=[dict(frequency=f,magnitude=abs(z),phase_degrees=math.degrees(cmath.phase(z)))
                                          for f,z in zip((440-rate,440+rate),ratios)],
                         passive_differential_carrier_prediction=predicted))
    result=dict(manifest_sha256=sha(run/'manifest.json'),analysis_script_sha256=sha(Path(__file__)),
                passive_oracle_sha256=sha(source),rows=rows,
                limit='This evaluates the new passive network inside an ideal normalized multiplier. It is not calibrated physical hardware wet attenuation or a switching-core model.')
    (run/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
    output,segments=array('f'),[]
    for rate in (1,30,220):
        for variant in ('parent','ring'):
            if output:output.extend(array('f',[0])*12000)
            start=len(output);output.extend(audio(run/f'{variant}-{rate}.wav')[96000:192000])
            segments.append(dict(rate=rate,variant=variant,source_seconds=[2,4],start_seconds=start/48000,end_seconds=len(output)/48000))
    data=output.tobytes();fmt=struct.pack('<HHIIHH',3,1,48000,192000,4,32)
    wav=(b'RIFF'+struct.pack('<I',48+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+
         b'fact'+struct.pack('<II',4,len(output))+b'data'+struct.pack('<I',len(data))+data)
    (run/'comparison.wav').write_bytes(wav)
    (run/'listening.json').write_text(json.dumps(dict(wav_sha256=sha(run/'comparison.wav'),gain=1,fades=False,segments=segments),indent=2)+'\n')
    print(json.dumps(dict(seconds=len(output)/48000,ratios=[dict(rate=row['rate'],measured=row['output_rms_ratio'],passive=row['passive_differential_carrier_prediction']['magnitude']) for row in rows]),indent=2))


if __name__ == '__main__':
    main()

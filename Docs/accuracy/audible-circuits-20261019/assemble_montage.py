#!/usr/bin/env python3
"""Assemble only the predeclared fixed-gain component listening segments."""
import json
import struct
from array import array
from pathlib import Path
from analyze_capacitors import audio,sha
ROOT=Path(__file__).resolve().parent

def main():
    plan=json.loads((ROOT/'montage-plan.json').read_text())
    assert plan['gain']==1 and plan['sample_rate']==48000 and not plan['fades']
    output,segments=array('f'),[]
    for entry in plan['segments']:
        path=Path(entry['path']);assert sha(path)==entry['sha256']
        samples=audio(path);a,b=(round(t*48000) for t in entry['source_seconds'])
        assert 0<=a<b<=len(samples)
        if output:output.extend(array('f',[0])*12000)
        start=len(output);output.extend(samples[a:b])
        segments.append(dict(entry,start_seconds=start/48000,end_seconds=len(output)/48000))
    data=output.tobytes();fmt=struct.pack('<HHIIHH',3,1,48000,192000,4,32)
    wav=(b'RIFF'+struct.pack('<I',48+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+
         b'fact'+struct.pack('<II',4,len(output))+b'data'+struct.pack('<I',len(data))+data)
    path=ROOT/'comparison-circuits.wav';path.write_bytes(wav)
    report=dict(plan_sha256=sha(ROOT/'montage-plan.json'),script_sha256=sha(Path(__file__)),
                wav_sha256=sha(path),frames=len(output),seconds=len(output)/48000,
                peak=max(abs(x) for x in output),gain=1,fades=False,segments=segments)
    (ROOT/'montage.json').write_text(json.dumps(report,indent=2)+'\n')
    assert len(segments)==12 and len(output)==900000
    print(json.dumps({k:report[k] for k in ('wav_sha256','seconds','peak')},indent=2))
if __name__=='__main__':main()

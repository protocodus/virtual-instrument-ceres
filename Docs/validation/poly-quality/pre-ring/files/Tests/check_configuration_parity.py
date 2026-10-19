#!/usr/bin/env python3
"""Exact native/embedded output for 45 configured synthetic scenes, no recordings."""
import argparse, hashlib, json, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--native',type=Path,required=True);p.add_argument('--embedded',type=Path,required=True)
p.add_argument('--report',type=Path)
a=p.parse_args();results={};reference=None
with tempfile.TemporaryDirectory(prefix='ceres-config-parity-') as d:
    for name,binary in [('native',a.native),('embedded',a.embedded)]:
        out=Path(d)/(name+'.f32');subprocess.run([str(binary.resolve()),str(out)],check=True)
        data=out.read_bytes();assert len(data)==5*3*3*4096*4
        if reference is None:reference=data
        else:assert data==reference,'native/embedded mismatch'
        results[name]={'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'audio_sha256':hashlib.sha256(data).hexdigest()}
report={'status':'pass','sample_count':5*3*3*4096,'rates':[44100,48000,88200,96000,192000],'polyphony':[1,4,16],'oversampling':[1,2,4],'variants':results}
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

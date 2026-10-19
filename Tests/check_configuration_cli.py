#!/usr/bin/env python3
"""Exercise actual configurable renderer and invalid-input atomic rejection."""
import argparse, hashlib, json, math, struct, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--renderer',type=Path,required=True);p.add_argument('--report',type=Path);a=p.parse_args()
rows=[]
with tempfile.TemporaryDirectory(prefix='ceres-configuration-cli-') as d:
    t=Path(d);events=t/'events.tsv';events.write_text(''.join(f'0 on {40+i*3} .8\n' for i in range(16)))
    command=[str(a.renderer.resolve()),'--events',str(events),'--frames','512','--preset','init']
    default=t/'default.wav';subprocess.run(command+['--output',str(default)],check=True,capture_output=True)
    for voices in [1,4,16]:
        for factor in [1,2,4]:
            out=t/'case.wav';meta=json.loads(subprocess.check_output(command+['--output',str(out),'--voices',str(voices),'--oversampling',str(factor)]))
            b=out.read_bytes();offset=b.index(b'data')+8;values=struct.unpack('<512f',b[offset:]);assert all(map(math.isfinite,values));assert max(map(abs,values))>1e-7
            assert meta['voices']==voices and meta['oversampling']==factor and meta['nominal_latency_samples']==31
            if (voices,factor)==(4,4):assert b==default.read_bytes(),'explicit4/4 changed default output'
            rows.append({'voices':voices,'factor':factor,'sha256':hashlib.sha256(b).hexdigest(),'peak':meta['peak']})
    for flag,values in [('--voices',[0,17,18446744073709551615]),('--oversampling',[0,3,8,18446744073709551615])]:
        for value in values:
            bad=t/'invalid.wav';r=subprocess.run(command+['--output',str(bad),flag,str(value)],capture_output=True)
            assert r.returncode!=0 and not bad.exists(),(flag,value)
report={'status':'pass','cases':rows,'explicit_default_exact':True,'invalid_rejections':7}
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

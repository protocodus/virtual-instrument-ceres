#!/usr/bin/env python3
"""Synthetic CLI parsing/state-selection checks; no public-reference inputs."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary=Path(sys.argv[1]).resolve()/'ceres_render'
with tempfile.TemporaryDirectory(prefix='ceres-pitch-route-cli-') as name:
    temp=Path(name)
    events=temp/'events.tsv'
    events.write_text('0 on 60 .8\n20 bend 0 .7\n40 off 60 0\n')
    base=[str(binary),'--events',str(events),'--output',str(temp/'out.wav'),
          '--frames','64','--preset','init']
    outcomes={}
    for name,args in [('default',[]),('shared',['--research-pitch-route','shared']),
                     ('vco-only',['--research-pitch-route','vco-only']),
                     ('static-master',['--research-pitch-route','static-master','--research-master-semitones','.31'])]:
        out=subprocess.run(base+args,capture_output=True,text=True,check=True)
        row=json.loads(out.stdout)
        assert row['research_pitch_route']==('shared' if name=='default' else name)
        assert row['research_master_semitones']==(.31 if name=='static-master' else 0)
        outcomes[name]=(temp/'out.wav').read_bytes()
    assert outcomes['default']==outcomes['shared']
    invalid=[(['--research-pitch-route','other'],'unknown research pitch route'),
             (['--research-master-semitones','0'],'requires'),
             (['--research-pitch-route','vco-only','--research-master-semitones','0'],'requires')]
    invalid += [(['--research-pitch-route','static-master','--research-master-semitones',value],
                 'finite and within') for value in ('nan','inf','48.001','-48.001','2junk')]
    for args,message in invalid:
        out=subprocess.run(base+args,capture_output=True,text=True)
        assert out.returncode!=0 and message in out.stderr,(args,out.stderr)
    for endpoint in ('-48','48'):
        subprocess.run(base+['--research-pitch-route','static-master',
                            '--research-master-semitones',endpoint],capture_output=True,check=True)
print('PASS research CLI: 4 routes/default, 8 invalid inputs, 2 accepted endpoints; unchanged synthetic events')

#!/usr/bin/env python3
"""Compile frozen55869df and compare the same synthetic active-control stream."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE='55869df8ba728e25a6b5792aa07dfd55f134439c'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=ROOT/'build')
parser.add_argument('--report',type=Path,default=ROOT/'build/joint-disabled-parity.json')
args=parser.parse_args();build=args.build.resolve()
with tempfile.TemporaryDirectory(prefix='ceres-joint-disabled-') as tmp:
    temp=Path(tmp);source=temp/'source';source.mkdir()
    bound={}
    for name in ('ceres.hpp','ceres.cpp'):
        data=subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':Source/DSP/'+name])
        (source/name).write_bytes(data);bound[name]=hashlib.sha256(data).hexdigest()
    driver=ROOT/'Tests/ParityDriver.cpp'
    assert driver.read_bytes()==subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':Tests/ParityDriver.cpp'])
    command=['c++','-std=c++17','-O3','-DNDEBUG','-I',str(source),str(driver),str(source/'ceres.cpp'),'-o',str(temp/'baseline')]
    subprocess.run(command,check=True)
    streams={}
    for name,binary in [('baseline',temp/'baseline'),('candidate',build/'ceres_native_parity')]:
        out=temp/(name+'.f32')
        subprocess.run([str(binary),str(out),'expression'],check=True)
        streams[name]=out.read_bytes()
    assert streams['baseline']==streams['candidate'],'disabled research mode changed audio'
    assert len(streams['baseline'])==384000*4
    report={'status':'pass','baseline_commit':BASE,'mode':'expression; research disabled/default',
        'samples':384000,'rates':[44100,48000,96000,192000],'scenes_per_rate':8,
        'byte_exact':True,'audio_sha256':hashlib.sha256(streams['baseline']).hexdigest(),
        'baseline_sources':bound,'compile_command':command,
        'candidate_binary_sha256':sha(build/'ceres_native_parity'),
        'input_sha256':{str(p.relative_to(ROOT)):sha(p) for p in
            [ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp',driver,Path(__file__).resolve()]}}
    args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

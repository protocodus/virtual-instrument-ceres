#!/usr/bin/env python3
"""Compile the frozen parent and prove default/explicit Shared synthetic parity."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE='ff2dd42e71f545bd52e621bac896d1e46c132d53'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,default=ROOT/'build')
parser.add_argument('--report',type=Path,required=True)
args=parser.parse_args();build=args.build.resolve()
with tempfile.TemporaryDirectory(prefix='ceres-pitch-shared-') as name:
    temp=Path(name);source=temp/'source';source.mkdir()
    baseline_sources={}
    for filename in ('ceres.hpp','ceres.cpp'):
        data=subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':Source/DSP/'+filename])
        (source/filename).write_bytes(data)
        baseline_sources[filename]=hashlib.sha256(data).hexdigest()
    driver=ROOT/'Tests/PitchRouteParityDriver.cpp'
    command=['c++','-std=c++17','-O3','-DNDEBUG','-I',str(source),str(driver),
             str(source/'ceres.cpp'),'-o',str(temp/'parent')]
    subprocess.run(command,check=True)
    programs={'parent-default':[str(temp/'parent')],
              'candidate-default':[str(build/'ceres_pitch_route_parity')],
              'candidate-explicit-shared':[str(build/'ceres_pitch_route_parity')]}
    reference=None;variants={}
    for label,cmd in programs.items():
        output=temp/(label+'.f32')
        subprocess.run(cmd+[str(output)]+(['shared'] if label.endswith('explicit-shared') else []),check=True)
        data=output.read_bytes()
        assert len(data)==96000*4
        if reference is None:reference=data
        else:assert data==reference,label+' audio differs from frozen parent'
        variants[label]={'binary_sha256':sha(Path(cmd[0])),'audio_sha256':sha(output),'bytes':len(data)}
    files=[ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp',ROOT/'CMakeLists.txt',driver,Path(__file__).resolve()]
    report={'status':'pass','baseline_commit':BASE,'baseline_sources':baseline_sources,
            'baseline_compile_command':command,'samples_per_variant':96000,
            'rates':[44100,48000,96000,192000],'calibration_network_modes':[0,1,2],
            'frames_per_scene':8000,'byte_exact':True,'variants':variants,
            'scope':'Synthetic active-control scene, moving bend/pressure, mixed voices, pedal/steal/release/panic/reset; no recording inputs',
            'input_sha256':{str(p.relative_to(ROOT)):sha(p) for p in files}}
args.report.parent.mkdir(parents=True,exist_ok=True)
args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

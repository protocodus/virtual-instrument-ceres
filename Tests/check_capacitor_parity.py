#!/usr/bin/env python3
"""Exact native/embedded parity with live Q and idle-IL capacitor edits."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('build',type=Path)
parser.add_argument('--native',type=Path)
parser.add_argument('--embedded',type=Path)
args=parser.parse_args();build=args.build.resolve()
def executable(mode):
    override=getattr(args,mode)
    plain=build/('ceres_'+mode+'_parity')
    return override or (plain if plain.exists() else plain.with_suffix('.exe'))
with tempfile.TemporaryDirectory(prefix='ceres-capacitor-parity-') as tmp:
    streams=[]
    for mode in ('native','embedded'):
        path=Path(tmp)/(mode+'.f32')
        subprocess.run([str(executable(mode)),str(path),'capacitors'],check=True)
        streams.append(path.read_bytes())
    assert len(streams[0])==384000*4
    assert streams[0]==streams[1], 'Native/embedded live capacitor audio differs'
    print('PASS 384000 samples, 4 rates x8 scenes, live Q/IL, panic/reset; SHA256 '+hashlib.sha256(streams[0]).hexdigest())

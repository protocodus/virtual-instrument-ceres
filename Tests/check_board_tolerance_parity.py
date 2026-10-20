#!/usr/bin/env python3
"""Exact native/embedded comparison with active board tolerances."""
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
with tempfile.TemporaryDirectory(prefix='ceres-board-parity-') as temp:
    data=[]
    for mode in ('native','embedded'):
        path=Path(temp)/(mode+'.f32')
        subprocess.run([str(executable(mode)),str(path),'boards'],check=True)
        data.append(path.read_bytes())
    assert len(data[0])==4*8*12000*4
    assert data[0]==data[1], 'board-tolerance audio differs'
    # Same driver and panel controls without the tolerances: must differ, so
    # the comparison above exercised the per-board offsets.
    plain=Path(temp)/'identical-boards.f32'
    subprocess.run([str(executable('native')),str(plain),'identical-boards'],check=True)
    assert plain.read_bytes()!=data[0], 'board tolerances had no effect'
    print('PASS board tolerance parity: 384000 float32 samples, 4 rates, 8 scenes; SHA256 '+
          hashlib.sha256(data[0]).hexdigest())

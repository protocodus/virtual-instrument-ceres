#!/usr/bin/env python3
"""Compare active per-voice expression through native and embedded builds."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

build=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='ceres-expression-parity-') as temp:
    data=[]
    for mode in ('native','embedded'):
        path=Path(temp)/(mode+'.f32')
        subprocess.run([str(build/('ceres_'+mode+'_parity')),str(path),'expression'],check=True)
        data.append(path.read_bytes())
    assert len(data[0])==4*8*12000*4
    assert data[0]==data[1], 'per-voice expression audio differs'
    print('PASS active expression parity: 384000 float32 samples, 4 rates, 8 scenes; SHA256 '+
          hashlib.sha256(data[0]).hexdigest())

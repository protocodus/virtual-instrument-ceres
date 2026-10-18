#!/usr/bin/env python3
"""Exact native/embedded comparison with active sound-path controls."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

build=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='ceres-soundpath-parity-') as temp:
    data=[]
    for mode in ('native','embedded'):
        path=Path(temp)/(mode+'.f32')
        subprocess.run([str(build/('ceres_'+mode+'_parity')),str(path),'soundpath'],check=True)
        data.append(path.read_bytes())
    assert len(data[0])==4*8*12000*4
    assert data[0]==data[1], 'active-control audio differs'
    print('PASS active sound-path parity: 384000 float32 samples, 4 rates, 8 scenes; SHA256 '+
          hashlib.sha256(data[0]).hexdigest())

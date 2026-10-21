#!/usr/bin/env python3
"""Exact native/embedded comparison for SUB-EXT, the EXT IN jack."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('build',type=Path)
parser.add_argument('--native',type=Path)
parser.add_argument('--embedded',type=Path)
args=parser.parse_args();build=args.build.resolve()
def executable(core):
    override=getattr(args,core)
    plain=build/('ceres_'+core+'_external_parity')
    return override or (plain if plain.exists() else plain.with_suffix('.exe'))
def render(core,mode,temp):
    path=Path(temp)/(core+'-'+mode+'.f32')
    subprocess.run([str(executable(core)),str(path),mode],check=True)
    return path.read_bytes()
with tempfile.TemporaryDirectory(prefix='ceres-external-parity-') as temp:
    native=render('native','external',temp)
    assert len(native)==4*3*3*5*6000
    assert native==render('embedded','external',temp), 'EXT IN audio differs between native and embedded'
    # Without the feed the same stream must differ, so the comparison above
    # exercised the jack on every route.
    assert render('native','silent',temp)!=native, 'EXT IN had no effect'
    # The internal sources 0..4 never hear the jack, in either core.
    inert=render('native','inert',temp)
    assert inert==render('native','inert-silent',temp), 'internal SUB sources heard EXT IN (native)'
    assert inert==render('embedded','inert',temp), 'internal SUB sources differ between native and embedded'
    assert inert==render('embedded','inert-silent',temp), 'internal SUB sources heard EXT IN (embedded)'
    print('PASS SUB-EXT parity: 270000 float32 samples, 3 rates x 3 qualities x 5 scenes; SHA256 '+
          hashlib.sha256(native).hexdigest())

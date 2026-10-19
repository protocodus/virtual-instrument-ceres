#!/usr/bin/env python3
"""Recompile passive-ring parent and compare exact mix-zero audio."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
PARENT='8c5a96e75f426dd7854cfee8e76cccd9478bbbfe'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--build-dir',type=Path,default=ROOT/'build');ap.add_argument('--report',type=Path,required=True);args=ap.parse_args()
    build=args.build_dir.resolve();identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    records=[];parent_hashes={}
    with tempfile.TemporaryDirectory(prefix='ceres-ring-parent-') as tmp:
        tmp=Path(tmp)
        for name in ('ceres.cpp','ceres.hpp'):
            (tmp/name).write_bytes(subprocess.check_output(['git','show',PARENT+':Source/DSP/'+name],cwd=ROOT));parent_hashes[name]=sha(tmp/name)
        compile_command=[os.environ.get('CXX','c++'),'-std=c++17','-O3','-DNDEBUG','-I',str(tmp),str(ROOT/'Tests/ParityDriver.cpp'),str(tmp/'ceres.cpp'),'-o',str(tmp/'parent')]
        subprocess.run(compile_command,check=True)
        for scene,extra in [('default-controls',[]),('active-carrier-bypassed',['ring-bypass'])]:
            streams=[];variants={}
            for label,binary in [('parent',tmp/'parent'),('candidate',build/'ceres_native_parity')]:
                output=tmp/(label+'.f32');command=[str(binary),str(output),*extra]
                subprocess.run(command,check=True);streams.append(output.read_bytes())
                variants[label]={'command':command,'binary_sha256':sha(binary),'audio_sha256':sha(output),'samples':len(streams[-1])//4}
            assert len(streams[0])==384000*4 and streams[0]==streams[1],scene
            records.append({'scene':scene,'exact':True,'variants':variants})
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'parent_commit':PARENT,
        'parent_source_sha256':parent_hashes,'compiled_source_sha256':identity,'compile_command':compile_command,
        'comparisons':records,'samples_per_variant':768000,
        'scope':'mix0 exact dry path; active carrier rates/depths/EG, allSUB functions, note/pedal/pressure/panic/reset. Same compiler/math library. Temporary binaries/streams deleted; retained source/commands reproduce. No recording fit.',
        'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [Path(__file__),ROOT/'Tests/ParityDriver.cpp',ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp']}}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print('PASS mix0 parent parity:768000 float32 samples')
if __name__=='__main__':main()

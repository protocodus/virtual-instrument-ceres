#!/usr/bin/env python3
"""Build the frozen pre-expression core and require identical legacy audio."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASELINE='b22dda696090a754f0018f2646bf92b259de406f'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-dir',type=Path,default=ROOT/'build')
    ap.add_argument('--report',type=Path,default=ROOT/'Tests/Reports/expression/legacy-parity.json')
    args=ap.parse_args();build=args.build_dir.resolve()
    identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
    for path,digest in identity.items(): assert sha(ROOT/path)==digest,path
    with tempfile.TemporaryDirectory(prefix='ceres-legacy-expression-') as temp:
        folder=Path(temp);original={}
        for name in ('ceres.hpp','ceres.cpp'):
            data=subprocess.check_output(['git','show',BASELINE+':Source/DSP/'+name],cwd=ROOT)
            (folder/name).write_bytes(data);original[name]=sha(folder/name)
        command=[os.environ.get('CXX','c++'),'-std=c++17','-O3','-DNDEBUG','-I',str(folder),
                 str(ROOT/'Tests/ParityDriver.cpp'),str(folder/'ceres.cpp'),'-o',str(folder/'legacy')]
        subprocess.run(command,check=True)
        # expression_source is absent: this mode only sets previously existing
        # keys, with all pressure routes nonzero and live channel pressure.
        records={};audio=[]
        for label,binary in [('baseline',folder/'legacy'),('candidate',build/'ceres_native_parity')]:
            path=folder/(label+'.f32')
            subprocess.run([str(binary),str(path),'soundpath'],check=True)
            audio.append(path.read_bytes())
            records[label]={'binary_sha256':sha(binary),'audio_sha256':sha(path),'samples':len(audio[-1])//4}
        assert len(audio[0])==384000*4 and audio[0]==audio[1],'Legacy expression default changed audio'
        report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),
                'baseline_commit':BASELINE,'baseline_source_sha256':original,'baseline_compile_command':command,
                'compiled_source_sha256':identity,'streams':records,
                'scope':'Byte-exact default source0: 4 rates x8 scenes, all3 nonzero pressure depths, moving pressure, mixed velocities, chords, repeated-note steal, pedal, release, panic and reset.',
                'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in
                    [ROOT/'Tests/ParityDriver.cpp',Path(__file__),ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp']}}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS frozen b22 legacy audio:384000 float32 samples byte-identical; '+records['candidate']['audio_sha256'])
if __name__=='__main__':main()

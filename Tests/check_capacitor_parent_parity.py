#!/usr/bin/env python3
"""Recompile the frozen SUB-corrected parent; verify unchanged settled-patch audio."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
PARENT='d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-dir',type=Path,default=ROOT/'build')
    ap.add_argument('--report',type=Path,required=True)
    args=ap.parse_args();build=args.build_dir.resolve()
    identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
    for path,digest in identity.items():assert sha(ROOT/path)==digest,path
    commands=[];records=[];parent_hashes={}
    with tempfile.TemporaryDirectory(prefix='ceres-capacitor-parent-') as tmp:
        tmp=Path(tmp)
        for name in ('ceres.cpp','ceres.hpp'):
            (tmp/name).write_bytes(subprocess.check_output(['git','show',PARENT+':Source/DSP/'+name],cwd=ROOT))
            parent_hashes[name]=sha(tmp/name)
        def compile_(driver,folder,core,output):
            command=[os.environ.get('CXX','c++'),'-std=c++17','-O3','-DNDEBUG','-I',str(folder),str(driver),str(core),'-o',str(output)]
            commands.append(command);subprocess.run(command,check=True)
        compile_(ROOT/'Tests/ParityDriver.cpp',tmp,tmp/'ceres.cpp',tmp/'parent')
        compile_(ROOT/'Tests/CapacitorStaticDriver.cpp',tmp,tmp/'ceres.cpp',tmp/'parent-factory')
        compile_(ROOT/'Tests/CapacitorStaticDriver.cpp',ROOT/'Source/DSP',build/'libceres.a',tmp/'candidate-factory')
        for scene,old,new,extra in [
            ('manual-static-q-il',tmp/'parent',build/'ceres_native_parity',[]),
            ('active-global-q-static-anchors',tmp/'parent',build/'ceres_native_parity',['soundpath']),
            ('all-14-factory-and-init',tmp/'parent-factory',tmp/'candidate-factory',[])]:
            streams=[];variants={}
            for label,binary in [('parent',old),('candidate',new)]:
                output=tmp/(label+'.f32');command=[str(binary),str(output),*extra]
                subprocess.run(command,check=True);data=output.read_bytes();streams.append(data)
                variants[label]={'command':command,'binary_sha256':sha(binary),'samples':len(data)//4,'audio_sha256':sha(output)}
            expected=114688 if scene=='all-14-factory-and-init' else 384000
            assert len(streams[0])==expected*4 and streams[0]==streams[1],scene
            records.append({'scene':scene,'exact':True,'variants':variants})
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'parent_commit':PARENT,
        'parent_source_sha256':parent_hashes,'compiled_source_sha256':identity,'compile_commands':commands,
        'comparisons':records,'total_samples_per_variant':sum(r['variants']['parent']['samples'] for r in records),
        'scope':'Same compiler/math library; static Q anchors/IL, including dynamic old global offset. No hardware recording or fitting. Temporary binaries/float streams are deleted; source and commands reproduce the check.',
        'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [Path(__file__),ROOT/'Tests/ParityDriver.cpp',ROOT/'Tests/CapacitorStaticDriver.cpp',ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp']}}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS exact parent audio:',report['total_samples_per_variant'],'samples,',len(records),'comparisons')
if __name__=='__main__':main()

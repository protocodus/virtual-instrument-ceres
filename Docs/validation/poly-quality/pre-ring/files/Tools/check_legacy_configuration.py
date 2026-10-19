#!/usr/bin/env python3
"""Recompile immutable parent2ee6cf2 and compare default4/4 audio exactly.
No recording, parameter fitting or retained audio; tests sharednoise/drift,
nonzero pressure, source routes, ring, note stealing, live edits/pedal/reset.
"""
import argparse, hashlib, json, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PARENT='2ee6cf22deead7358d6b4ee7c294da955225af56'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd):subprocess.run([str(x) for x in cmd],check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    rows=[]
    with tempfile.TemporaryDirectory(prefix='ceres-parent-parity-') as d:
        t=Path(d)
        sources={}
        for name in ['ceres.hpp','ceres.cpp']:
            data=subprocess.check_output(['git','show',PARENT+':Source/DSP/'+name],cwd=ROOT)
            (t/name).write_bytes(data);sources[name]=hashlib.sha256(data).hexdigest()
        driver=ROOT/'Tests/ParityDriver.cpp'
        cmd=['c++','-std=c++17','-O3','-DNDEBUG','-I',t,driver,t/'ceres.cpp','-o',t/'parent']
        run(cmd)
        for mode in [None,'expression']:
            values={};reference=None
            for label,binary in [('parent',t/'parent'),('candidate',a.candidate.resolve())]:
                out=t/(label+'.f32');run([binary,out]+([mode] if mode else []));data=out.read_bytes()
                assert len(data)==384000*4
                if reference is None:reference=data
                else:assert data==reference,mode
                values[label]={'audio_sha256':sha(out),'binary_sha256':sha(binary)}
            rows.append({'mode':mode or 'legacy','samples':384000,'variants':values})
        report={'status':'pass','parent_commit':PARENT,'parent_source_sha256':sources,'exact_sample_count':768000,
            'scope':'Default4 voices/4x; temporary parent binary/audio re-created by this script, not retained','cases':rows,
            'compile_command':[str(x) for x in cmd],
            'inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp',driver,Path(__file__).resolve()]}}
    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()

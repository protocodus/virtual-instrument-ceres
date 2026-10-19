#!/usr/bin/env python3
"""Compare exact native/embedded samples; optionally verify the imported original."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build')
    parser.add_argument('--native',type=Path,help='Exact native executable from the CMake target')
    parser.add_argument('--embedded',type=Path,help='Exact embedded executable from the CMake target')
    parser.add_argument('--baseline-source',type=Path)
    parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    build=args.build_dir.resolve()
    def executable(name):
        plain=build/name
        return plain if plain.exists() else plain.with_suffix('.exe')
    programs={'native':args.native or executable('ceres_native_parity'),
              'embedded':args.embedded or executable('ceres_embedded_parity')}
    evidence={}
    with tempfile.TemporaryDirectory(prefix='ceres-parity-') as name:
        temp=Path(name)
        if args.baseline_source:
            baseline=args.baseline_source.resolve()
            imported=json.loads((ROOT/'Docs/EXPERIMENT_IMPORT.json').read_text())
            for filename in ['ceres.hpp','ceres.cpp']:
                expected=imported['files']['Source/DSP/'+filename]['original_sha256']
                assert sha(baseline/filename)==expected,filename
            command=['c++','-std=c++17','-O3','-DNDEBUG','-I',str(baseline),
                     str(ROOT/'Tests/ParityDriver.cpp'),str(baseline/'ceres.cpp'),'-o',str(temp/'original')]
            subprocess.run(command,check=True)
            programs['original']=temp/'original'
            evidence['baseline_compile_command']=command
        fingerprints={}
        reference=None
        for label,binary in programs.items():
            output=temp/(label+'.f32')
            subprocess.run([str(binary),str(output)],check=True)
            data=output.read_bytes()
            assert len(data)==4*8*12000*4
            if reference is None:reference=data
            else:assert data==reference,label+' differs'
            fingerprints[label]={'audio_sha256':sha(output),'bytes':len(data),'binary_sha256':sha(binary)}
        evidence|={'status':'pass','comparison':'byte-exact float32 stream on this compiler/math-library build',
            'rates':[44100,48000,96000,192000],'scenes_per_rate':8,'frames_per_scene':12000,
            'samples_per_variant':384000,'variants':fingerprints,
            'input_sha256':{str(p.relative_to(ROOT)):sha(p) for p in
                [ROOT/'Source/DSP/ceres.hpp',ROOT/'Source/DSP/ceres.cpp',ROOT/'Tests/ParityDriver.cpp',
                 ROOT/'Tools/check_embedded.py',ROOT/'Docs/EXPERIMENT_IMPORT.json']}}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))

if __name__=='__main__':main()

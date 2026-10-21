#!/usr/bin/env python3
"""Recompile Ceres 0.5.0, the parent without EXT IN, and the working core with
the unchanged parity drivers. With the jack never fed and lfo_waveform 0..4,
every float32 stream must be byte-identical, native and embedded."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
PARENT='0f1d00f096f968cf4ca027304123e28cf0f9aaf9'
DRIVERS={'ParityDriver':['','neutral','capacitors','expression','boards','identical-boards','soundpath','ring-bypass'],
         'ConfigurationParityDriver':['']}
TARGETS={'native':[],'embedded':['-DCERES_EMBEDDED_TARGET=1','-fno-exceptions']}
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--report',type=Path)
    args=ap.parse_args()
    for driver in DRIVERS:
        parent=subprocess.check_output(['git','show',PARENT+':Tests/'+driver+'.cpp'],cwd=ROOT)
        assert parent==(ROOT/'Tests'/(driver+'.cpp')).read_bytes(),driver+' changed since the parent'
    compiler=os.environ.get('CXX','c++')
    report={'parent':PARENT,'compiler':subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
            'command':[compiler,'-std=c++17','-O2','-DNDEBUG','<target flags>','-I','<core>','Tests/<driver>.cpp',
                       '<core>/ceres.cpp','-o','<binary>'],'target_flags':TARGETS,'streams':{}}
    with tempfile.TemporaryDirectory(prefix='ceres-external-parent-') as temp:
        temp=Path(temp);(temp/'parent').mkdir()
        for name in ('ceres.cpp','ceres.hpp'):
            (temp/'parent'/name).write_bytes(subprocess.check_output(['git','show',PARENT+':Source/DSP/'+name],cwd=ROOT))
        cores={'parent':temp/'parent','candidate':ROOT/'Source/DSP'}
        report['sources']={label:{name:sha((folder/name).read_bytes()) for name in ('ceres.cpp','ceres.hpp')}
                           for label,folder in cores.items()}
        for target,flags in TARGETS.items():
            for driver,modes in DRIVERS.items():
                streams={}
                for label,folder in cores.items():
                    binary=temp/(label+'-'+target+'-'+driver)
                    subprocess.run([compiler,'-std=c++17','-O2','-DNDEBUG',*flags,'-I',str(folder),
                                    str(ROOT/'Tests'/(driver+'.cpp')),str(folder/'ceres.cpp'),'-o',str(binary)],check=True)
                    for mode in modes:
                        output=temp/'stream.f32'
                        subprocess.run([str(binary),str(output),*([mode] if mode else [])],check=True)
                        streams[label,mode]=output.read_bytes()
                for mode in modes:
                    name=target+'/'+driver+'/'+(mode or 'default')
                    assert streams['parent',mode]==streams['candidate',mode],name+' differs from the parent'
                    report['streams'][name]={'float32_samples':len(streams['parent',mode])//4,
                                             'sha256':sha(streams['parent',mode])}
    report['status']='pass'
    text=json.dumps(report,indent=2)+'\n'
    if args.report:args.report.write_text(text)
    print(text,end='')
if __name__=='__main__':main()

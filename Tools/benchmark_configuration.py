#!/usr/bin/env python3
"""Run once, with no concurrent heavy build/render, and retain every timing row.
No host scheduling guarantee; all cases have one synth with active source/ring.
"""
import argparse, hashlib, json, platform, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='ceres-config-performance-') as d:
    binary=Path(d)/'probe';cmd=['c++','-std=c++17','-O3','-DNDEBUG','-DCERES_EMBEDDED_TARGET=1','-fno-exceptions','-I',str(ROOT/'Source/DSP'),str(ROOT/'Tools/benchmark_configuration.cpp'),str(ROOT/'Source/DSP/ceres.cpp'),'-o',str(binary)]
    subprocess.run(cmd,check=True)
    rows=json.loads(subprocess.check_output([str(binary)]))
    report={'status':'completed','scope':'One native embedded-ABI synth; not Rack, Double, target or host performance. No retry/removal of overruns.','platform':platform.platform(),'compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0],'compile_command':cmd,'binary_sha256':sha(binary),'measurements':rows,'inputs_sha256':{str(f.relative_to(ROOT)):sha(f) for f in [ROOT/'Source/DSP/ceres.cpp',ROOT/'Source/DSP/ceres.hpp',ROOT/'Tools/benchmark_configuration.cpp',Path(__file__).resolve()]}}
a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

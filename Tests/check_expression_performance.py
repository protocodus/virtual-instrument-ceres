#!/usr/bin/env python3
"""One bounded default embedded-DSP timing run; never retries for a pass."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
build=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'build'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
for path,digest in identity.items():assert sha(ROOT/path)==digest,path
binary=build/'ceres_expression_performance'
command=[os.environ.get('CXX','c++'),'-std=c++17','-O3','-DNDEBUG','-DCERES_EMBEDDED_TARGET=1',
         '-I',str(ROOT/'Source/DSP'),str(ROOT/'Tools/native_performance.cpp'),
         str(build/'libceres_embedded.a'),'-o',str(binary)]
subprocess.run(command,check=True)
report=json.loads(subprocess.check_output([str(binary)],text=True))
report.update({'compile_command':command,'binary_sha256':sha(binary),
               'embedded_library_sha256':sha(build/'libceres_embedded.a'),
               'compiled_source_sha256':identity,'performance_source_sha256':sha(ROOT/'Tools/native_performance.cpp'),
               'measurement_script_sha256':sha(Path(__file__))})
output=ROOT/'Tests/Reports/expression/performance-default.json'
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

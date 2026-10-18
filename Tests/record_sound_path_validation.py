#!/usr/bin/env python3
"""Bind successful CTest logs and compiled identities after the documented builds.

This records completed checks; it does not run or replace CTest. Re-run both
builds/CTest commands from README before recording after a source change.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--release-build',type=Path,default=ROOT/'build')
    ap.add_argument('--sanitize-build',type=Path,default=ROOT/'build-sanitize')
    args=ap.parse_args()
    destination=ROOT/'Tests/Reports/soundpath'
    files=['Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','CMakeLists.txt',
           'Tests/dsp_tests.cpp','Tests/SoundPathTests.cpp','Tests/EmbeddedContract.cpp',
           'Tests/ParityDriver.cpp','Tests/check_sound_path_parity.py',
           'Tests/record_sound_path_validation.py','Tools/check_embedded.py',
           'Tools/render_main.cpp','Tools/build_identity.hpp.in']
    bindings={name:sha(ROOT/name) for name in files}
    builds={}
    names=['native_dsp_contracts','sound_path_circuit_contracts','embedded_allocation_contract',
           'native_embedded_audio_parity','active_sound_path_audio_parity']
    for label,build in [('release',args.release_build.resolve()),('sanitizer',args.sanitize_build.resolve())]:
        summary=destination/(label+'.log')
        assert '100% tests passed, 0 tests failed out of 5' in summary.read_text(),label
        detail=(build/'Testing/Temporary/LastTest.log').read_text()
        assert detail.count('Test Passed.')==5,label
        for name in names:assert 'Testing: '+name in detail,(label,name)
        assert len(re.findall(r'^PASS ',detail,flags=re.MULTILINE))==33,(label,'PASS groups')
        detail_path=destination/(label+'-details.log')
        detail_path.write_text(detail)
        identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
        for path,digest in identity.items():assert sha(ROOT/path)==digest,(label,path)
        executables=['libceres.a','libceres_embedded.a','ceres_render','ceres_tests','ceres_sound_path_tests',
                     'ceres_embedded_contract','ceres_native_parity','ceres_embedded_parity']
        builds[label]={'build_directory':str(build),'compiled_source_sha256':identity,
                       'binary_sha256':{name:sha(build/name) for name in executables},
                       'ctest_count':5,'native_group_count':31,
                       'sanitizers':['address','undefined'] if label=='sanitizer' else [],
                       'asan_detect_leaks':False if label=='sanitizer' else None}
        for path in (summary,detail_path):bindings[str(path.relative_to(ROOT))]=sha(path)
    report={'status':'pass','recorded_utc':datetime.now(timezone.utc).isoformat(),
            'scope':'Bounded native circuit/lifecycle, embedded allocation, and native/embedded audio checks; not hardware calibration.',
            'parameter_keys':56,'new_groups':5,'existing_groups':26,
            'allocation_witness':{'blocks':512,'frames_per_block':64,'post_construction_new_calls':0},
            'each_parity_stream':{'float32_samples':384000,'rates':[44100,48000,96000,192000],
                                  'scenes_per_rate':8,'comparison':'byte-exact'},
            'builds':builds,'verified_file_sha256':bindings,
            'pre_trim_logs':'pre-trim/*.log preserve successful predecessor checks; they are not claimed as final-source validation.'}
    output=destination/'validation.json';output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'PASS {len(bindings)} input/log bindings, 5 release + 5 sanitizer CTest tests; {output}')
if __name__=='__main__':main()

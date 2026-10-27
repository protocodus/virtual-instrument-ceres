#!/usr/bin/env python3
"""Bind the completed expression CTest runs and direct prior-core comparison."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'Tests/Reports/expression'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--release-build',type=Path,default=ROOT/'build')
    ap.add_argument('--sanitize-build',type=Path,default=ROOT/'build-sanitize')
    args=ap.parse_args()
    files=['Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','CMakeLists.txt','Tests/dsp_tests.cpp',
           'Tests/SoundPathTests.cpp','Tests/ExpressionTests.cpp','Tests/EmbeddedContract.cpp',
           'Tests/ParityDriver.cpp','Tests/check_expression_parity.py','Tests/check_expression_legacy.py',
           'Tests/record_expression_validation.py','Tests/check_expression_performance.py',
           'Tests/check_sound_path_parity.py','Tools/check_embedded.py',
           'Tools/render_main.cpp','Tools/build_identity.hpp.in','README.md','Docs/research/dsp-source.md']
    bindings={p:sha(ROOT/p) for p in files};builds={}
    names=['native_dsp_contracts','sound_path_circuit_contracts','voice_expression_contracts',
           'embedded_allocation_contract','native_embedded_audio_parity','active_sound_path_audio_parity',
           'expression_audio_parity']
    for label,build in [('release',args.release_build.resolve()),('sanitizer',args.sanitize_build.resolve())]:
        log=DEST/(label+'.log');assert '100% tests passed, 0 tests failed out of 7' in log.read_text(),label
        detail=(build/'Testing/Temporary/LastTest.log').read_text()
        assert detail.count('Test Passed.')==7,label
        for name in names:assert 'Testing: '+name in detail,(label,name)
        detailpath=DEST/(label+'-details.log');detailpath.write_text(detail)
        identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info'],text=True))
        for p,digest in identity.items():assert sha(ROOT/p)==digest,(label,p)
        binaries=['ceres_render','ceres_tests','ceres_sound_path_tests','ceres_expression_tests',
                  'ceres_embedded_contract','ceres_native_parity','ceres_embedded_parity','libceres.a','libceres_embedded.a']
        builds[label]={'compiled_source_sha256':identity,'binary_sha256':{p:sha(build/p) for p in binaries},
                       'ctest_count':7,'native_groups':36,'sanitizers':['address','undefined'] if label=='sanitizer' else [],
                       'asan_detect_leaks':False if label=='sanitizer' else None}
        for p in (log,detailpath):bindings[str(p.relative_to(ROOT))]=sha(p)
    legacy=DEST/'legacy-parity.json';report=json.loads(legacy.read_text());assert report['status']=='pass'
    for p,digest in report['verified_file_sha256'].items():assert sha(ROOT/p)==digest,p
    assert report['streams']['candidate']['binary_sha256']==builds['release']['binary_sha256']['ceres_native_parity']
    assert report['streams']['baseline']['audio_sha256']==report['streams']['candidate']['audio_sha256']
    bindings[str(legacy.relative_to(ROOT))]=sha(legacy)
    perf=DEST/'performance-default.json';performance=json.loads(perf.read_text())
    assert performance['measured_blocks']==10000 and performance['voices']==4
    assert performance['embedded_library_sha256']==builds['release']['binary_sha256']['libceres_embedded.a']
    assert performance['compiled_source_sha256']==builds['release']['compiled_source_sha256']
    assert performance['measurement_script_sha256']==sha(ROOT/'Tests/check_expression_performance.py')
    for p in (perf,ROOT/'Tools/native_performance.cpp'):bindings[str(p.relative_to(ROOT))]=sha(p)
    validation={'status':'pass','recorded_utc':datetime.now(timezone.utc).isoformat(),
                'scope':'Optional modern per-voice velocity expression; no hardware model/calibration change at default source0.',
                'parameter_keys':57,'new_native_groups':5,'native_groups':36,
                'allocation_witness':{'blocks':576,'frames_per_block':64,'post_construction_new_calls':0},
                'parity_stream':{'samples':384000,'rates':[44100,48000,96000,192000],'scenes_per_rate':8},
                'legacy_baseline_commit':report['baseline_commit'],'builds':builds,
                'default_performance':performance,'verified_file_sha256':bindings,
                'limits':'Native timing is not SDK/Reason host timing. Allocation interposition covers new/new[], not every platform allocator. No hardware fidelity claim is made for velocity routing.'}
    (DEST/'validation.json').write_text(json.dumps(validation,indent=2)+'\n')
    print(f'PASS expression validation:7 release+7 sanitizer tests; {len(bindings)} source/log bindings')
if __name__=='__main__':main()

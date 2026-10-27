#!/usr/bin/env python3
"""Bind completed capacitor checks; does not rerun tests or create audio."""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Docs/capacitor-continuity'
PARENT='d5e0b2dfb21d4c7fa8350d2c5c9555f3bb711f43'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    identity=json.loads(subprocess.check_output([str(ROOT/'build/ceres_render'),'--build-info'],text=True))
    for path,digest in identity.items():assert sha(ROOT/path)==digest,path
    parity=json.loads((OUT/'parent-parity.json').read_text())
    assert parity['status']=='pass' and parity['parent_commit']==PARENT
    assert parity['compiled_source_sha256']==identity
    assert parity['total_samples_per_variant']==882688
    for path,digest in parity['verified_file_sha256'].items():assert sha(ROOT/path)==digest,path
    for name,count in [('release.log',9),('sanitize.log',3)]:
        text=(OUT/name).read_text()
        assert f'100% tests passed, 0 tests failed out of {count}' in text,name
    def parameter_table(source):return source.split('const Parameter parameters[] = {',1)[1].split('\n};',1)[0]
    parent=subprocess.check_output(['git','show',PARENT+':Source/DSP/ceres.cpp'],cwd=ROOT,text=True)
    current=(ROOT/'Source/DSP/ceres.cpp').read_text()
    assert parameter_table(parent)==parameter_table(current),'Parameter ABI changed'
    keys=re.findall(r'\{"([a-z_]+)",',parameter_table(current));assert len(keys)==57
    tracked=['CMakeLists.txt','Source/DSP/ceres.cpp','Source/DSP/ceres.hpp',
        'Tests/CapacitorContinuityTests.cpp','Tests/CapacitorStaticDriver.cpp',
        'Tests/ParityDriver.cpp','Tests/check_capacitor_parity.py',
        'Tests/check_capacitor_parent_parity.py','Tests/record_capacitor_validation.py']
    diff=subprocess.check_output(['git','diff','--cached','--binary','--',*tracked],cwd=ROOT)
    assert diff, 'Stage owned source/tests before recording the full source patch'
    (OUT/'source.patch').write_bytes(diff)
    research=Path('/tmp/ceres-cs50-1x8LGd/research')
    audit=Path('/Users/vojta/Dev/virtual-instrument-ceres-soundpath-20261018/Docs/accuracy/filter-eg-capacitor-continuity/source-audit.md')
    assert sha(audit)=='4febb599e7b01dfdfdf8276a4749c4454f58b0a044986ec71547f29807f84028'
    files=[ROOT/p for p in tracked]+[OUT/name for name in ['README.md','source.patch','release.log','build.log',
        'sanitize.log','sanitize-build.log','parent-parity.json','parent-parity.log']]
    files += [audit]+[research/name for name in ['service1.pdf','service2.pdf','yamaha-ic-guide.pdf','cs-servicing-guide.pdf']]
    binaries=[ROOT/'build'/name for name in ['ceres_render','libceres.a','libceres_embedded.a','ceres_capacitor_tests','ceres_native_parity','ceres_embedded_parity']]
    report={'status':'pass','created_utc':datetime.now(timezone.utc).isoformat(),'parent_commit':PARENT,
        'compiled_source_sha256':identity,'release_ctest_passed':9,'asan_ubsan_ctest_passed':3,
        'asan_options':'detect_leaks=0','embedded_keys':len(keys),'parameter_table_byte_identical':True,
        'parent_audio_exact_samples':882688,'live_native_embedded_exact_samples':384000,
        'scope':'Q-anchor and idle-EG continuity only; no oscillator/parameter/UI changes, public recording renders, or audio fitting.',
        'runtime_allocation_contract':'Existing embedded test passed with all57 keys and active controls; zero post-construction new/new[] allocations.',
        'binary_sha256':{str(p.relative_to(ROOT)):sha(p) for p in binaries},
        'verified_file_sha256':{str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):sha(p) for p in files}}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,digest in report['verified_file_sha256'].items():assert sha(ROOT/name)==digest,name
    print('PASS',len(report['verified_file_sha256']),'bindings;',len(keys),'unchanged keys; renderer',sha(ROOT/'build/ceres_render'))
if __name__=='__main__':main()

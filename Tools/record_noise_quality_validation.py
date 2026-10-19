#!/usr/bin/env python3
"""Run/bind audio-noise quality correction checks without touching old evidence.

--run builds the local standalone Release and targeted sanitizer paths, runs
the complete Release suite and the five relevant sanitizer tests, and repeats
immutable original4x parity. No recordings, fitting, audio retention or timing.
Without --run, verify existing logs and current compiled identities only.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
D=ROOT/'Docs/validation/noise-quality'
PARENT='652e3722eb40f67001f0467103f9f31a957ec363'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--run',action='store_true')
a=p.parse_args();D.mkdir(parents=True,exist_ok=True)
commands=[]
def run(name,cmd,env=None):
    import os
    cmd=[str(x) for x in cmd]
    commands.append({'log':name,'command':cmd,'environment_overrides':env or {}})
    print(name,flush=True)
    with (D/name).open('w') as log:
        completed=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                                 env={**os.environ,**(env or {})})
    if completed.returncode:
        raise RuntimeError(f'{name} exit{completed.returncode}; log retained')
if a.run:
    for build,kind,sanitize in [('build-dsp','Release','OFF'),('build-sanitize','RelWithDebInfo','ON')]:
        prefix='sanitizer' if sanitize=='ON' else 'release'
        run(prefix+'-configure.log',['cmake','-S','.','-B',build,
            '-DCERES_BUILD_PLUGIN=OFF','-DCERES_BUILD_UNIVERSAL=OFF',
            '-DCMAKE_BUILD_TYPE='+kind,'-DBUILD_TESTING=ON','-DCERES_SANITIZE='+sanitize])
        targets=[] if sanitize=='OFF' else ['--target','ceres_noise_quality_tests',
            'ceres_configuration_tests','ceres_tests','ceres_embedded_contract',
            'ceres_native_configuration_parity','ceres_embedded_configuration_parity']
        run(prefix+'-build.log',['cmake','--build',build,'--parallel','2']+targets)
        select=[] if sanitize=='OFF' else ['-R',
            '^Ceres\\.(configuration_contracts|native_dsp_contracts|noise_quality_contracts|embedded_allocation_contract|configuration_native_embedded_parity)$']
        run(prefix+'-ctest.log',['ctest','--test-dir',build,'--output-on-failure','-V']+select,
            {'ASAN_OPTIONS':'detect_leaks=0'} if sanitize=='ON' else None)
    run('legacy-parity.log',['python3','Tools/check_legacy_configuration.py',
        '--candidate','build-dsp/ceres_native_parity','--report',D/'legacy-parity.json'])
    (D/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')

for log,n in [('release-ctest.log',20),('sanitizer-ctest.log',5)]:
    assert f'100% tests passed, 0 tests failed out of {n}' in (D/log).read_text(),log
legacy=json.loads((D/'legacy-parity.json').read_text())
assert legacy['status']=='pass' and legacy['exact_sample_count']==768000
for path,h in legacy['inputs_sha256'].items():assert sha(ROOT/path)==h,path
identity=json.loads(subprocess.check_output([str(ROOT/'build-dsp/ceres_render'),'--build-info']))
for path,h in identity.items():assert sha(ROOT/path)==h,path
noise=[]
for line in (D/'release-ctest.log').read_text().splitlines():
    match=re.search(r'NOISE_DELTA_DB,(\d+),(\d+),(\d+),(.+)$',line)
    if match:noise.append(dict(zip(['host_hz','factor','band','db'],[int(match[1]),int(match[2]),int(match[3]),float(match[4])])))
assert len(noise)==12 and max(abs(row['db']) for row in noise)<.6
peer=json.loads((D/'peer/report.json').read_text())
assert peer['status']=='pass' and peer['audio_bit_identity_cases']==11 and peer['state_identity_cases']==15
assert peer['plan_sha256']==sha(D/'peer/plan.json')
assert peer['measurements_sha256']==sha(D/'peer/measurements.json')
plan=json.loads((D/'peer/plan.json').read_text())
for path,h in plan['candidate_original_source_sha256'].items():
    assert sha(ROOT/'Source/DSP'/Path(path).name)==h,path
paths=['CMakeLists.txt','Source/DSP/ceres.cpp','Source/DSP/ceres.hpp',
       'Tests/NoiseQualityTests.cpp','Tests/ConfigurationParityDriver.cpp','Tests/ConfigurationTests.cpp',
       'Tests/RingDepthContinuityTests.cpp','Tools/benchmark_configuration.cpp',
       'Tests/EmbeddedContract.cpp','Tools/record_noise_quality_validation.py',
       'Tools/check_legacy_configuration.py','Tests/ParityDriver.cpp',
       'README.md','Docs/DSP_SOURCE.md']
paths += [str(f.relative_to(ROOT)) for f in D.rglob('*') if f.is_file() and f.name!='validation.json']
report={'status':'pass','product_version':'0.2.0','previous_commit':PARENT,
    'scope':'Audio NO only; fixed-host-rate variance/frequency normalization, no hardware gain fit, SUB/drift unchanged. Prior configuration/CPU reports historical.',
    'release_tests_passed':20,'targeted_asan_ubsan_tests_passed':5,
    'legacy4x_exact_samples':768000,'configured_native_embedded_exact_samples':184320,
    'postconstruction_allocation_contract':'pass','band_hz':[[200,800],[800,2000],[2000,4000]],
    'samples_per_noise_case':262144,'warmup_seconds':1,'noise_band_deltas_db':noise,
    'maximum_absolute_band_delta_db':max(abs(row['db']) for row in noise),
    'independent_peer_audio_exact_cases':11,'independent_peer_state_exact_cases':15,
    'build_identity':identity,'renderer_sha256':sha(ROOT/'build-dsp/ceres_render'),
    'verified_file_sha256':{path:sha(ROOT/path) for path in sorted(set(paths))}}
(D/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

#!/usr/bin/env python3
"""Bind completed configuration checks to current source; never rerender audio.
The recorded logs are local executions, not independent CI/host approvals.
"""
import argparse, hashlib, json, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];D=ROOT/'Docs/validation/poly-quality'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=ROOT/'build-dsp');a=p.parse_args();build=a.build.resolve()
for log,count in [('release-ctest.log',18),('sanitizer-ctest.log',4)]:
    text=(D/log).read_text();assert f'100% tests passed, 0 tests failed out of {count}' in text,log
legacy=json.loads((D/'legacy-parity.json').read_text());assert legacy['status']=='pass' and legacy['exact_sample_count']==768000
for f,h in legacy['inputs_sha256'].items():assert sha(ROOT/f)==h,f
for name in ['configured-parity.json','cli.json']:
    assert json.loads((D/name).read_text())['status']=='pass'
performance=json.loads((D/'performance.json').read_text())
for f,h in performance['inputs_sha256'].items():assert sha(ROOT/f)==h,f
identity=json.loads(subprocess.check_output([str(build/'ceres_render'),'--build-info']))
for f,h in identity.items():assert sha(ROOT/f)==h,f
inputs=['CMakeLists.txt','Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','Tools/render_main.cpp',
 'Tests/ConfigurationTests.cpp','Tests/ConfigurationParityDriver.cpp','Tests/EmbeddedContract.cpp',
 'Tests/check_configuration_cli.py','Tests/check_configuration_parity.py',
 'Tools/check_legacy_configuration.py','Tools/benchmark_configuration.py','Tools/benchmark_configuration.cpp',
 'Tools/record_configuration_validation.py','README.md','Docs/DSP_SOURCE.md']
inputs += [str(f.relative_to(ROOT)) for f in D.iterdir() if f.is_file() and f.name!='validation.json']
report={'status':'pass','product_version':'0.2.0','parent_commit':'2ee6cf22deead7358d6b4ee7c294da955225af56',
 'release_ctest_passed':18,'targeted_asan_ubsan_passed':4,'legacy_exact_samples':768000,
 'configured_native_embedded_exact_samples':184320,'patch_key_count_unchanged':57,
 'postconstruction_allocations_observed':0,'configured_voices':[1,4,16],'configured_factors':[1,2,4],
 'timing_limit':'Native single-engine16voice4x has64/1000 thread-CPU overruns; all wall misses retained; no host/Double guarantee.',
 'build_identity':identity,'binary_sha256':{name:sha(build/name) for name in ['ceres_render','ceres_configuration_tests','ceres_embedded_contract','ceres_native_configuration_parity','ceres_embedded_configuration_parity']},
 'verified_file_sha256':{f:sha(ROOT/f) for f in sorted(set(inputs))},
 'scope':'Local native/C++17 embedded validation; no Rack SDK or host validation, no calibration/recording fits. JUCE scaffold unchanged.'}
(D/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

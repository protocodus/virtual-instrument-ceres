#!/usr/bin/env python3
"""Record the merged build checks without rerunning or weakening any contract.

The circuit source must match the reviewed integrated parent; the existing
JUCE scaffold, CI, release tools and legal files must match its main parent.
The pre-existing empty-preset audio failure is required and recorded as a
failure, not converted into a passing/disabled test. Logs contain the actual
completed local commands' output. This records local build integration only.
"""
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Docs/validation/main-integration-2026-10-19'
CIRCUIT='aec922d'
SCAFFOLD='813c004'
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    circuit=git('rev-parse',CIRCUIT).decode().strip();scaffold=git('rev-parse',SCAFFOLD).decode().strip()
    exact={}
    for name in ['Source/DSP/ceres.cpp','Source/DSP/ceres.hpp']:
        assert (ROOT/name).read_bytes()==git('show',circuit+':'+name),name
        exact[name]=sha(ROOT/name)
    protected=['Source/DSP/CeresEngine.cpp','Source/DSP/CeresEngine.h','Source/PluginProcessor.cpp',
        'Source/PluginProcessor.h','Source/PluginEditor.cpp','Source/PluginEditor.h',
        'Tools/PresetDemoRenderer.h','Tools/RenderPresetDemos.cpp','Tools/update_readme_blocks.py',
        'Tests/ParameterMatrix.h','Tests/FuzzHarness.h','LICENSE','THIRD_PARTY_NOTICES.md']
    protected+=git('ls-tree','-r','--name-only',scaffold,'.github','scripts','ThirdParty').decode().splitlines()
    for name in protected:
        assert (ROOT/name).read_bytes()==git('show',scaffold+':'+name),name
    for name,count in [('dsp-unit.log',13),('dsp-fuzz.log',1),('sanitize.log',7),('plugin-processor.log',1)]:
        assert f'100% tests passed, 0 tests failed out of {count}' in (OUT/name).read_text(),name
    audio=(OUT/'dsp-audio-existing-failure.log').read_text()
    assert 'the instrument declares no presets' in audio and '0% tests passed, 1 tests failed' in audio
    assert 'unable to find a build program' in (OUT/'multi-configure.log').read_text()
    for name in ['CeresPluginProcessorTests','CeresParameterMatrixTests','Ceres_VST3','Ceres_AU','Ceres_CLAP','Ceres_Standalone']:
        assert 'Built target '+name in (OUT/'plugin-build.log').read_text(),name
    identity=json.loads(subprocess.check_output([str(ROOT/'build-dsp/ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    ctest=json.loads(subprocess.check_output(['ctest','--test-dir',str(ROOT/'build-dsp'),'--show-only=json-v1'],text=True))
    assert len(ctest['tests'])==15
    for test in ctest['tests']:
        assert test['name'].startswith('Ceres.'),test['name']
        labels=[p['value'] for p in test['properties'] if p['name']=='LABELS']
        assert len(labels)==1 and len(labels[0])==1,test['name']
    db=json.loads((ROOT/'build-dsp/compile_commands.json').read_text())
    core=[r for r in db if r['file'].endswith('/Source/DSP/ceres.cpp')]
    assert len(core)==2 and all('-std=c++17' in r['command'] for r in core)
    embedded=[r for r in core if 'CERES_EMBEDDED_TARGET' in r['command']]
    assert len(embedded)==1 and '-fno-exceptions' in embedded[0]['command']
    plugin_paths={'VST3':'VST3/Ceres.vst3/Contents/MacOS/Ceres',
        'AU':'AU/Ceres.component/Contents/MacOS/Ceres','CLAP':'CLAP/Ceres.clap/Contents/MacOS/Ceres',
        'Standalone':'Standalone/Ceres.app/Contents/MacOS/Ceres'}
    binaries=[ROOT/'build-dsp'/p for p in ['ceres_render','ceres_native_parity','ceres_embedded_parity']]
    binaries += [ROOT/'build-plugin/CeresPluginProcessorTests']
    binaries += [ROOT/'build-plugin/Ceres_artefacts/Release'/p for p in plugin_paths.values()]
    modified=['CMakeLists.txt','CLAUDE.md','README.md','.gitignore','Tools/check_embedded.py',
        'Tests/check_sound_path_parity.py','Tests/check_expression_parity.py','Tests/check_capacitor_parity.py',
        'Tools/record_main_integration.py']
    files=[ROOT/p for p in modified+protected]+list(OUT.glob('*.log'))
    report={'status':'local_unit_integration_pass_known_audio_failure','created_utc':datetime.now(timezone.utc).isoformat(),
        'circuit_parent':circuit,'scaffold_parent':scaffold,'conflicts_resolved':['.gitignore','CMakeLists.txt','README.md'],
        'circuit_source_byte_identical':exact,'scaffold_protected_files_byte_identical':protected,
        'project_version':'0.1.0','compiled_source_sha256':identity,
        'local_checks':{'unit_passed':13,'fuzz_passed':1,'asan_ubsan_targeted_passed':7,
            'plugin_processor_passed':1,'audio_failed':1,
            'audio_failure':'Pre-existing scaffold declares no presets; source and shared renderer unchanged.'},
        'plugin_build':{'host_architecture':platform.machine(),'universal':False,
            'formats_built':list(plugin_paths),'juce_version':'8.0.14',
            'clap_extensions_commit':'c1a5ad025f95d01e03267857fa8276ebeed16500',
            'scope':'Local native build and processor test; no installation, signing/package audit, external plugin host validation or hardware recording render.'},
        'portability':{'circuit_standard':'C++17 explicitly verified in compile commands','scaffold_standard':'C++20 plus existing C++17 object check',
            'embedded_no_exceptions':True,'exact_cmake_target_paths_for_parity':True,
            'multi_config_runtime':'Not executed: local Ninja unavailable; active xcode-select uses CommandLineTools. Actual TARGET_FILE paths are used by CMake and .exe fallback preserved for direct script use.',
            'windows_execution':False,'sanitizer_options':['CERES_SANITIZE compatibility alias','CERES_ENABLE_SANITIZERS retained']},
        'known_limits':['Full suite is not green: existing AudioDemos failure remains. No test disabled or weakened.',
            'JUCE plugin remains a silent separate CeresEngine scaffold; circuit model not wired into it.',
            'Historical research evidence remains bound to its original source/build identities and may need external inputs.',
            'This local report does not assert GitHub CI status or authorize merging a PR.'],
        'binary_sha256':{str(p.relative_to(ROOT)):sha(p) for p in binaries},
        'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in files}}
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS local unit integration;',len(files),'bindings; known audio failure retained')
if __name__=='__main__':main()

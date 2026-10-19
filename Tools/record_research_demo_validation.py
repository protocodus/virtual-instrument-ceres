#!/usr/bin/env python3
"""Verify and bind the circuit-research demo integration after completed runs.

The shared audio helper, all circuit equations/presets and the separate JUCE
plugin must remain unchanged. This verifies the authored source MIDI and the
14 exported MP3s; it never adjusts a preset, renders the synth or chooses gain.
Local tests are reported separately from remote CI, which is not queried.
"""
from array import array
from datetime import datetime,timezone
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Docs/validation/research-demos-2026-10-19'
BASE='90c3b3e5c3ff8d6ce8f2a7def0effa360275a8a4'
MIDI_COMMIT='a0211e4b34b878efd252992760fe609844023e9b'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT)
def decode_midi(path):
    # Independent direct format0 decoder for these deliberately simple files.
    data=path.read_bytes();assert data[:4]==b'MThd' and struct.unpack('>IHHH',data[4:14])==(6,0,1,960)
    assert data[14:18]==b'MTrk' and struct.unpack('>I',data[18:22])[0]==len(data)-22
    at=22;tick=0;messages=[];tempo=None
    def vlq():
        nonlocal at
        value=0
        for _ in range(4):
            byte=data[at];at+=1;value=value*128+(byte&127)
            if not byte&128:return value
        raise AssertionError('Invalid VLQ')
    while at<len(data):
        tick+=vlq();kind=data[at];at+=1
        if kind==255:
            meta=data[at];at+=1;count=vlq();payload=data[at:at+count];at+=count
            if meta==81:tempo=int.from_bytes(payload,'big')
        else:
            assert kind in [0x80,0x90]
            note,velocity=data[at:at+2];at+=2;messages.append((tick,kind,note,velocity))
    assert tempo==500000 and len(messages)==16 and tick==18240
    expected=[]
    for on,off,note in [(480,2400,48),(2880,4800,52),(5280,7200,55),(7680,10560,60)]+[(11520,18240,k) for k in [48,55,60,64]]:
        expected.extend([(on,0x90,note,100),(off,0x80,note,0)])
    assert messages==sorted(expected,key=lambda x:x[0]),path.name
    return {'channel_events':len(messages),'last_event_seconds':tick/1920.,'sha256':sha(path)}
def main():
    for log,count in [('audio.log',1),('unit.log',13),('fuzz.log',1)]:
        assert f'100% tests passed, 0 tests failed out of {count}' in (OUT/log).read_text(),log
    prior=ROOT/'Docs/validation/main-integration-2026-10-19/report.json'
    assert sha(prior)=='1557dd11fcb1c635e9b643c7cedac9f2ee34755363fc958ec4f71854f5af1445'
    protected=['Source/DSP/ceres.cpp','Source/DSP/ceres.hpp','Source/DSP/CeresEngine.cpp','Source/DSP/CeresEngine.h',
        'Source/PluginProcessor.cpp','Source/PluginProcessor.h','Source/PluginEditor.cpp','Source/PluginEditor.h',
        'Tools/PresetDemoRenderer.h','Tools/update_readme_blocks.py']
    protected+=git('ls-tree','-r','--name-only',BASE,'.github','scripts').decode().splitlines()
    for name in protected:assert (ROOT/name).read_bytes()==git('show',BASE+':'+name),name
    identity=json.loads(subprocess.check_output([str(ROOT/'build-dsp/ceres_render'),'--build-info'],text=True))
    for name,digest in identity.items():assert sha(ROOT/name)==digest,name
    names=subprocess.check_output([str(ROOT/'build-dsp/ceres_render'),'--list-presets'],text=True).splitlines()
    manifest_path=ROOT/'build-dsp/demo-audio/demos.json';manifest=json.loads(manifest_path.read_text())
    assert len(names)==14 and [p['preset'] for p in manifest['demos']]==names
    assert manifest['sample_rate']==48000 and manifest['block_frames']==256 and manifest['seed']==20261017
    assert manifest['instrument']=='Ceres standalone circuit research presets'
    records=[];midi_paths=[]
    for number,(name,demo) in enumerate(zip(names,manifest['demos']),1):
        stem=f'{number:02}-{name.replace("_","-")}'
        path=ROOT/'Docs/audio/midi'/f'{stem}.mid';midi_paths.append(path)
        assert path.read_bytes()==git('show',MIDI_COMMIT+':'+str(path.relative_to(ROOT))),path
        decoded_midi=decode_midi(path)
        assert demo['number']==number and demo['file']==stem+'.mp3' and demo['seconds']==12
        assert -60<demo['peak_dbfs']<0
        mp3=manifest_path.parent/demo['file']
        command=['ffmpeg','-nostdin','-v','error','-i',str(mp3),'-f','f32le','-acodec','pcm_f32le','-ac','2','-ar','48000','pipe:1']
        decoded=subprocess.check_output(command);samples=array('f');samples.frombytes(decoded)
        if sys.byteorder!='little':samples.byteswap()
        assert samples and len(samples)%2==0 and all(math.isfinite(x) for x in samples)
        peak=max(abs(x) for x in samples);rms=math.sqrt(sum(x*x for x in samples)/len(samples))
        assert .001<peak<1 and rms>0
        records.append({'preset':name,'midi':str(path.relative_to(ROOT)),**decoded_midi,
            'raw_seconds':demo['seconds'],'raw_peak_dbfs':demo['peak_dbfs'],'mp3':str(mp3.relative_to(ROOT)),
            'mp3_sha256':sha(mp3),'decoded_stereo_frames':len(samples)//2,'decoded_peak':peak,'decoded_rms':rms,
            'decoded_channels_equal':all(samples[i]==samples[i+1] for i in range(0,len(samples),2))})
    assert len(list((ROOT/'Docs/audio/midi').glob('*.mid')))==14
    # Exercise the existing build-owned README generator in a temporary build
    # output, without hand-editing or prematurely committing its media block.
    preview=ROOT/'build-dsp/demo-preview-README.md';preview.write_bytes((ROOT/'README.md').read_bytes())
    subprocess.run([sys.executable,str(ROOT/'Tools/update_readme_blocks.py'),'demos','--readme',str(preview),
                    '--manifest',str(manifest_path)],check=True)
    files=[ROOT/p for p in protected]+[ROOT/p for p in ['CMakeLists.txt','README.md','CLAUDE.md',
        'Tools/RenderPresetDemos.cpp','Tools/make_research_demo_midi.py','Tools/record_research_demo_validation.py',
        'Docs/audio/midi/README.md']]+midi_paths+list(OUT.glob('*.log'))
    # Retain numerical metadata, not duplicate rebuildable MP3 payloads.
    (OUT/'demos.json').write_bytes(manifest_path.read_bytes());files.append(OUT/'demos.json')
    report={'status':'pass_local_dsp_audio_integration','created_utc':datetime.now(timezone.utc).isoformat(),
        'previous_integration_commit':BASE,'authored_midi_commit':MIDI_COMMIT,'compiled_source_sha256':identity,
        'tests':{'unit_passed':13,'fuzz_passed':1,'audio_tier_passed':1,'audio_internal_presets':14,
            'audio_repeat_render_count':28,'additional_export_render_count':14},
        'preset_changes':False,'normalization':False,'fixed_export_gain':1,'ceiling_linear':1,
        'raw_peak_dbfs_range':[min(r['raw_peak_dbfs'] for r in records),max(r['raw_peak_dbfs'] for r in records)],
        'superseded_failure':{'report':str(prior.relative_to(ROOT)),'sha256':sha(prior),
            'scope':'Only the empty demo-bank/tool failure is resolved. Historical report/log bytes remain unchanged; this is the new CMake/tool build identity.'},
        'protected_files_unchanged':protected,'records':records,
        'binary_sha256':{name:sha(ROOT/name) for name in ['build-dsp/ceres_render','build-dsp/RenderCeresPresetDemos']},
        'verified_file_sha256':{str(p.relative_to(ROOT)):sha(p) for p in files},
        'scope':'Original fixed MIDI through existing standalone circuit research presets. Separate JUCE plugin remains silent and unchanged; these are not plugin factory demos, recovered hardware performances, calibration fits or evidence of hardware similarity. Shared portfolio helper/CI and production DSP unchanged. MP3 outputs stay in ignored build directory; main CI owns committed media/README blocks.',
        'remote_ci':'Not queried. Local pass does not establish remote CI or review approval; historical remote failures/queued status must be reported from the current remote checks.'}
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS14 research demos; peaks',report['raw_peak_dbfs_range'],';',len(files),'bindings; no gain/preset changes')
if __name__=='__main__':main()

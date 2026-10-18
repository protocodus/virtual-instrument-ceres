#!/usr/bin/env python3
"""Fail closed on changed local source, synthetic MIDI/settings, or saved audio."""
from pathlib import Path
import json
import run_battery as b

def main():
 plan=b.verify_plan();checked={}
 for name,digest in plan['input_sha256'].items():checked[name]=digest
 errata=b.read(b.BASE/'metadata-errata.json')
 assert errata['plan_sha256']==b.sha(b.BASE/'plan.json')
 for name,digest in errata['input_sha256'].items():
  assert b.sha(b.ROOT/name)==digest;checked[name]=digest
 for case in plan['cases']:b.midi_check(b.ROOT/case['midi'],b.ROOT/case['events'])
 for path in sorted((b.BASE/'runs').glob('*/report.json')):
  report=b.read(path);assert report['plan_sha256']==b.sha(b.BASE/'plan.json')
  relocations=b.read(b.BASE/'identity-relocations.json')['reports'] if (b.BASE/'identity-relocations.json').exists() else {}
  relocation=relocations.get(b.rel(path))
  if relocation:
   assert b.sha(path)==relocation['report_sha256']
   binary=b.ROOT/relocation['binary']; source_root=b.ROOT/relocation['source_root']
  else: binary=Path(report['binary']); source_root=Path(report['source_root'])
  assert b.sha(binary)==report['binary_sha256']
  assert b.read_identity(binary,source_root)==report['compiled_source_sha256']
  for row in report['cases']:
   assert b.sha(b.ROOT/row['audio'])==row['audio_sha256'];checked[row['audio']]=row['audio_sha256']
 for path in sorted(b.BASE.glob('comparison-*.json')):
  c=b.read(path);assert c['plan_sha256']==b.sha(b.BASE/'plan.json')
  assert b.sha(b.ROOT/c['listening']['audio'])==c['listening']['sha256']
 print(json.dumps({'status':'pass','plan_sha256':b.sha(b.BASE/'plan.json'),'frozen_inputs_and_audio_checked':len(checked),'MIDI_schedules_checked':len(plan['cases'])},indent=2))
if __name__=='__main__':main()

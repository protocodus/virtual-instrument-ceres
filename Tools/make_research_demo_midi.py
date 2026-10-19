#!/usr/bin/env python3
"""Reproduce the authored, fixed research-preset demonstration MIDI sources.

This is an original phrase, not recovered hardware MIDI. All existing native
research presets receive the same notes, velocities and timing. No sound is
rendered and no engine parameter is adjusted by this tool. The shared demo
renderer adds its fixed2.5-second tail to the final9.5-second note-off.
Default operation checks bytes; --write deliberately replaces these sources.
"""
import argparse
import hashlib
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[1]
NAMES=['init','string1','string2','brass1','brass2','flute','electric_piano',
       'clavichord','harpsichord','guitar1','guitar2','funky1','funky2','funky3']
def vlq(number):
    result=[number&127];number>>=7
    while number:result.insert(0,128|(number&127));number>>=7
    return bytes(result)
def midi(name):
    events=[]
    def note(start,end,key):
        events.extend([(round(start*1920),bytes([0x90,key,100])),(round(end*1920),bytes([0x80,key,0]))])
    for start,end,key in [(.25,1.25,48),(1.5,2.5,52),(2.75,3.75,55),(4,5.5,60)]:note(start,end,key)
    for key in [48,55,60,64]:note(6,9.5,key)
    events.sort(key=lambda item:item[0])
    label=('Ceres circuit research: '+name).encode('ascii')
    track=b'\x00\xff\x03'+vlq(len(label))+label+b'\x00\xff\x51\x03\x07\xa1\x20'
    prior=0
    for tick,message in events:track+=vlq(tick-prior)+message;prior=tick
    track+=b'\x00\xff\x2f\x00'
    return b'MThd'+struct.pack('>IHHH',6,0,1,960)+b'MTrk'+struct.pack('>I',len(track))+track
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--write',action='store_true');args=ap.parse_args()
    folder=ROOT/'Docs/audio/midi';folder.mkdir(parents=True,exist_ok=True)
    expected=[]
    for index,name in enumerate(NAMES,1):
        path=folder/f'{index:02}-{name.replace("_","-")}.mid';data=midi(name);expected.append(path.name)
        if args.write:path.write_bytes(data)
        assert path.read_bytes()==data,str(path)
        print(path.name,hashlib.sha256(data).hexdigest())
    assert sorted(p.name for p in folder.glob('*.mid'))==sorted(expected),'Unexpected or missing MIDI source'
    print('PASS14 authored phrases,16 channel events each,final note-off9.5s')
if __name__=='__main__':main()

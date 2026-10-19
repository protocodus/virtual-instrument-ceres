// Analysis driver: discard Synth audio; retain only exact control/phase states.
// This file is not part of production DSP or the audio renderer.
#include "ceres.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
struct Event { unsigned long long frame; std::string kind; int note; double value; };
int main(int argc, char** argv) {
  try {
    if (argc!=7) throw std::runtime_error("preset patch events frames seed output.f64");
    auto p=ceres::preset(argv[1]);
    std::ifstream patch(argv[2]); if(!patch) throw std::runtime_error("patch");
    std::string line;
    while(std::getline(patch,line)) {
      line=line.substr(0,line.find('#'));
      if(line.find_first_not_of(" \t\r\n")==std::string::npos) continue;
      auto eq=line.find('='); if(eq==std::string::npos) throw std::runtime_error("patch assignment");
      auto key=line.substr(0,eq); auto a=key.find_first_not_of(" \t"), z=key.find_last_not_of(" \t");
      key=key.substr(a,z-a+1);
      if(!ceres::setParameter(p,key,std::stod(line.substr(eq+1)))) throw std::runtime_error("patch key");
    }
    std::ifstream events(argv[3]); if(!events) throw std::runtime_error("events");
    std::vector<Event> queue;
    while(std::getline(events,line)) {
      line=line.substr(0,line.find('#'));
      if(line.find_first_not_of(" \t\r\n")==std::string::npos) continue;
      Event e; std::istringstream row(line);
      if(!(row>>e.frame>>e.kind>>e.note>>e.value)) throw std::runtime_error("event row");
      queue.push_back(e);
    }
    const auto frames=std::stoull(argv[4]), seed=std::stoull(argv[5]);
    ceres::Synth synth(48000,seed); synth.setPatch(p);
    std::ofstream out(argv[6],std::ios::binary); if(!out) throw std::runtime_error("trace output");
    std::size_t index=0, records=0;
    for(unsigned long long frame=0;frame<frames;++frame) {
      while(index<queue.size() && queue[index].frame==frame) {
        const auto& e=queue[index++];
        if(e.kind=="on") synth.noteOn(e.note,e.value);
        else if(e.kind=="off") synth.noteOff(e.note);
        else if(e.kind=="bend") synth.pitchBend(e.value);
        else if(e.kind=="pressure") synth.pressure(e.value);
        else if(e.kind=="sustain") synth.sustain(e.value>=.5);
        else if(e.kind=="alloff") synth.allNotesOff();
        else if(e.kind=="panic") synth.allSoundOff();
        else if(e.kind=="reset") synth.reset();
        else throw std::runtime_error("unknown event");
      }
      (void)synth.process(); // No audio value is stored, exported, or measured.
      if((frame+1)%3) continue; // 16 kHz exact state grid; coarse check uses 8 kHz.
      auto sound=synth.soundPathState(); auto mod=synth.modulationState();
      std::vector<double> row{double(frame+1)/48000.,sound.commonVcaGain,
                              sound.hpResonance,sound.lpResonance,mod.sub};
      for(const auto& v:synth.voiceState()) {
        for(double value:{double(v.note),double(v.keyNote),double(v.held),double(v.pedalHeld),
            v.ampEnvelope,v.filterEnvelope,v.oscillatorPhase,v.oscillatorFrequency,
            v.hpFrequency,v.lpFrequency}) row.push_back(value);
      }
      for(auto value:row) if(!std::isfinite(value)) throw std::runtime_error("nonfinite state");
      out.write(reinterpret_cast<const char*>(row.data()),row.size()*sizeof(double)); ++records;
    }
    out.close(); if(!out || index!=queue.size()) throw std::runtime_error("trace incomplete");
    std::cout<<std::setprecision(17)<<"{\"records\":"<<records<<",\"columns\":45,\"rate\":16000,\"frames\":"<<frames
      <<",\"audio_exported\":false,\"parameters\":{";
    bool first=true;
#define FIELD(name) if(!first)std::cout<<',';first=false;std::cout<<"\"" #name "\":"<<p.name;
    FIELD(saw) FIELD(pulse) FIELD(sine) FIELD(noise) FIELD(pwmDepth) FIELD(pwmRate)
    FIELD(pulseWidth) FIELD(hpCutoff) FIELD(lpCutoff) FIELD(hpResonance) FIELD(lpResonance)
    FIELD(hpEnvelopeScale) FIELD(lpEnvelopeScale) FIELD(keyTrack) FIELD(brilliance)
    FIELD(filterLevel) FIELD(ampLevel) FIELD(volume) FIELD(outputTrimDb) FIELD(octave)
    FIELD(tuneCents) FIELD(voiceSpreadCents) FIELD(driftCents) FIELD(vibratoCents)
    FIELD(filterLfo) FIELD(lfoRate) FIELD(lfoWaveform) FIELD(tremolo) FIELD(ringMix)
    FIELD(pressureFilter) FIELD(pressureVolume) FIELD(pressureVibrato)
    FIELD(velocitySensitivity) FIELD(resetMicroseconds) FIELD(drive)
#undef FIELD
    std::cout<<"}}\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

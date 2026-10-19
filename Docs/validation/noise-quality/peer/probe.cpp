#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc,char** argv){
 if(argc!=4)return 2;const char* profile=argv[1];const unsigned factor=std::strtoul(argv[2],nullptr,10),frames=std::strtoul(argv[3],nullptr,10);
 ceres::Synth s(48000,50);if(!s.configure(4,factor))return 3;
 ceres::Patch p;p.filterInitial=p.filterPeak=0;p.keyTrack=0;p.hpCutoff=40;p.lpCutoff=3000;p.ampAttack=.001;p.ampSustain=1;p.volume=.25;
 if(!std::strcmp(profile,"noise_only")){p.saw=p.pulse=p.sine=0;p.noise=.05;}
 else if(!std::strcmp(profile,"mixed_noise")){p.saw=.5;p.pulse=.25;p.sine=.1;p.noise=.25;p.driftCents=2;p.lfoWaveform=4;p.filterLfo=.2;}
 else if(!std::strcmp(profile,"sub_noise0")){p.noise=0;p.saw=.5;p.lfoWaveform=4;p.filterLfo=.7;p.vibratoCents=20;p.tremolo=.3;}
 else if(!std::strcmp(profile,"drift_noise0")){p.noise=0;p.saw=.5;p.driftCents=7;p.voiceSpreadCents=5;}
 else if(!std::strcmp(profile,"saw_noise0")){p.noise=0;p.saw=.5;p.ringMix=.4;p.ringDepth=2;}
 else return 4;
 s.setPatch(p);s.noteOn(60,.8);if(!std::strcmp(profile,"mixed_noise")||!std::strcmp(profile,"drift_noise0")){s.noteOn(64,.7);s.noteOn(67,.9);s.noteOn(72,1);}
 for(unsigned i=0;i<frames;++i){const float audio=s.process();const auto m=s.modulationState();const auto v=s.voiceState()[0];const double state[]={m.sub,v.oscillatorFrequency,v.oscillatorPhase};if(!std::isfinite(audio))return 5;
  if(std::fwrite(&audio,sizeof(audio),1,stdout)!=1||std::fwrite(state,sizeof(double),3,stdout)!=3)return 6;
 }
 return 0;
}

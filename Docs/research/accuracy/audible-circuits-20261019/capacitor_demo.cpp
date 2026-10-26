#include "ceres.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void u16(std::ostream& o,uint16_t x){o.put(char(x));o.put(char(x>>8));}
void u32(std::ostream& o,uint32_t x){u16(o,uint16_t(x));u16(o,uint16_t(x>>16));}
void wavHeader(std::ostream& o,uint32_t frames){o.write("RIFF",4);u32(o,48+4*frames);o.write("WAVEfmt ",8);u32(o,16);u16(o,3);u16(o,1);u32(o,48000);u32(o,192000);u16(o,4);u16(o,32);o.write("fact",4);u32(o,4);u32(o,frames);o.write("data",4);u32(o,4*frames);}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: capacitor_demo SCENE output.wav");
    const std::string scene=argv[1];
    const bool q=scene=="lp_q_step"||scene=="lp_q_static"||scene=="hp_q_step";
    if(!q&&scene!="idle_il"&&scene!="il_static")throw std::runtime_error("unknown scene");
    const bool hp=scene=="hp_q_step";
    ceres::Patch p;
    p.saw=1;p.pulse=p.sine=p.noise=0;p.drive=1;p.volume=.55;
    p.keyTrack=0;p.hpCutoff=hp?261.6255653005986:30;p.lpCutoff=q?(hp?8000:4*261.6255653005986):3000;
    p.hpResonance=hp?.2:0;p.lpResonance=q?(hp?0:(scene=="lp_q_static"?.8:.2)):.2;
    p.hpEnvelopeScale=0;p.lpEnvelopeScale=q?0:.65*39/47;
    p.filterInitial=scene=="il_static"?-5:0;p.filterPeak=0;
    p.filterAttack=q?.01:.25;p.filterDecay=.35;p.filterRelease=q?.2:1;
    p.ampAttack=.01;p.ampDecay=.3;p.ampSustain=1;p.ampRelease=.2;
    p.resetMicroseconds=6; // Freeze oscillator for a Q/EG-only comparison.
    const uint32_t frames=q?360000:96000;
    ceres::Synth synth(48000,1);synth.setPatch(p);
    std::ofstream o(argv[2],std::ios::binary);if(!o)throw std::runtime_error("cannot open output");wavHeader(o,frames);
    double peak=0,energy=0;
    for(uint32_t i=0;i<frames;++i){
        if(q){
            if(i==0)synth.noteOn(60,1);
            if(i==48000&&scene!="lp_q_static"){if(hp)p.hpResonance=.8;else p.lpResonance=.8;synth.setPatch(p);}
            if(i==312000)synth.noteOff(60);
        }else{
            if(i==24000&&scene=="idle_il"){p.filterInitial=-5;synth.setPatch(p);}
            if(i==26400)synth.noteOn(60,1);
            if(i==57600)synth.noteOff(60);
        }
        float x=synth.process();if(!std::isfinite(x))throw std::runtime_error("nonfinite audio");
        peak=std::max(peak,std::abs(double(x)));energy+=double(x)*x;
        uint32_t bits;std::memcpy(&bits,&x,4);u32(o,bits);
    }
    o.close();if(!o)throw std::runtime_error("WAV write failed");
    std::cout<<"{\"scene\":\""<<scene<<"\",\"frames\":"<<frames<<",\"peak\":"<<peak<<",\"rms\":"<<std::sqrt(energy/frames)<<",\"normalization\":\"none\"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

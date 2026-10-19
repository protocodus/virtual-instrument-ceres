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
    if(argc!=3)throw std::runtime_error("usage: oscillator_demo WAVE output.wav");
    const std::string wave=argv[1];
    if(wave!="saw"&&wave!="sine"&&wave!="pulse")throw std::runtime_error("unknown wave");
    ceres::Patch p;
    p.saw=wave=="saw";p.sine=wave=="sine";p.pulse=wave=="pulse";p.noise=0;
    p.hpCutoff=5;p.lpCutoff=20000;p.hpResonance=p.lpResonance=0;p.keyTrack=0;
    p.hpEnvelopeScale=p.lpEnvelopeScale=0;p.filterInitial=p.filterPeak=0;
    p.ampAttack=.01;p.ampDecay=.3;p.ampSustain=1;p.ampRelease=.08;
    p.drive=.05;p.volume=.25;
    // Reset intentionally uses each model's default: old6us, source-timed59.7286us.
    const uint32_t frames=192000;const int notes[]={69,81,93,105};
    ceres::Synth synth(48000,1);synth.setPatch(p);
    std::ofstream o(argv[2],std::ios::binary);if(!o)throw std::runtime_error("cannot open output");wavHeader(o,frames);
    double peak=0,energy=0;
    for(uint32_t i=0;i<frames;++i){
        for(unsigned j=0;j<4;++j){if(i==j*43200)synth.noteOn(notes[j],1);if(i==j*43200+31200)synth.noteOff(notes[j]);}
        float x=synth.process();if(!std::isfinite(x))throw std::runtime_error("nonfinite audio");
        peak=std::max(peak,std::abs(double(x)));energy+=double(x)*x;
        uint32_t bits;std::memcpy(&bits,&x,4);u32(o,bits);
    }
    o.close();if(!o)throw std::runtime_error("WAV write failed");
    std::cout<<"{\"wave\":\""<<wave<<"\",\"frames\":"<<frames<<",\"reset_us\":"<<p.resetMicroseconds<<",\"peak\":"<<peak<<",\"rms\":"<<std::sqrt(energy/frames)<<",\"normalization\":\"none\"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

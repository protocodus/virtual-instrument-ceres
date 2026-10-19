#include "ceres.hpp"
#include "build_identity.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Event { uint64_t frame; std::string kind; int note; double value; };
void u16(std::ostream& o, uint16_t n) { o.put(char(n)); o.put(char(n >> 8)); }
void u32(std::ostream& o, uint32_t n) { u16(o, uint16_t(n)); u16(o, uint16_t(n >> 16)); }
std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
uint64_t uintArgument(const std::string& value) {
    if(value.empty() || value[0]=='-') throw std::runtime_error("expected unsigned integer");
    size_t end=0; auto n=std::stoull(value,&end);
    if(end!=value.size()) throw std::runtime_error("invalid integer: "+value);
    return n;
}
void loadPatch(ceres::Patch& patch,const std::string& path) {
    std::ifstream in(path); if(!in) throw std::runtime_error("cannot read patch: "+path);
    std::string line; unsigned number=0;
    while(std::getline(in,line)) {
        ++number; line=trim(line.substr(0,line.find('#'))); if(line.empty()) continue;
        auto equal=line.find('=');
        if(equal==std::string::npos) throw std::runtime_error("patch expected key=value at line "+std::to_string(number));
        auto key=trim(line.substr(0,equal)), str=trim(line.substr(equal+1));
        size_t end=0; double value=std::stod(str,&end);
        if(end!=str.size() || !std::isfinite(value) || !ceres::setParameter(patch,key,value))
            throw std::runtime_error("unknown/invalid patch parameter: "+key);
    }
}
std::vector<Event> loadEvents(const std::string& path,uint64_t frames) {
    std::ifstream in(path); if(!in) throw std::runtime_error("cannot read events: "+path);
    std::vector<Event> events; std::string line;
    while(std::getline(in,line)) {
        line=trim(line.substr(0,line.find('#'))); if(line.empty()) continue;
        std::istringstream row(line); std::string frame,extra; Event e{};
        if(!(row>>frame>>e.kind>>e.note>>e.value) || (row>>extra)) throw std::runtime_error("invalid event row: "+line);
        e.frame=uintArgument(frame);
        if(e.frame>=frames || !std::isfinite(e.value) || e.note<0 || e.note>127)
            throw std::runtime_error("event outside render bounds: "+line);
        if(e.kind!="on"&&e.kind!="off"&&e.kind!="pressure"&&e.kind!="bend"&&e.kind!="sustain"&&e.kind!="alloff"&&e.kind!="panic"&&e.kind!="reset")
            throw std::runtime_error("unknown event type: "+e.kind);
        if((e.kind=="on"||e.kind=="pressure"||e.kind=="sustain") && (e.value<0||e.value>1))
            throw std::runtime_error("event value outside 0..1");
        if(e.kind=="bend" && std::abs(e.value)>24) throw std::runtime_error("pitch bend exceeds 24 semitones");
        if(!events.empty() && e.frame<events.back().frame) throw std::runtime_error("events must be sorted");
        events.push_back(e);
    }
    return events;
}
void dispatch(ceres::Synth& synth,const Event& e) {
    if(e.kind=="on") synth.noteOn(e.note,e.value);
    else if(e.kind=="off") synth.noteOff(e.note);
    else if(e.kind=="pressure") synth.pressure(e.value);
    else if(e.kind=="bend") synth.pitchBend(e.value);
    else if(e.kind=="sustain") synth.sustain(e.value>=.5);
    else if(e.kind=="alloff") synth.allNotesOff();
    else if(e.kind=="panic") synth.allSoundOff();
    else if(e.kind=="reset") synth.reset();
}
}

int main(int argc,char** argv) {
 try {
    std::string eventPath,outputPath,patchPath,preset="brass_1";
    uint64_t frames=0,seed=50; unsigned rate=48000;
    for(int i=1;i<argc;++i) {
        std::string option=argv[i];
        if(option=="--build-info") { std::cout<<ceresCompiledSources<<'\n'; return 0; }
        if(option=="--list-presets") { for(auto& p:ceres::presetNames()) std::cout<<p<<'\n'; return 0; }
        if(i+1>=argc) throw std::runtime_error("missing argument for "+option);
        std::string value=argv[++i];
        if(option=="--events") eventPath=value;
        else if(option=="--output") outputPath=value;
        else if(option=="--patch") patchPath=value;
        else if(option=="--preset") preset=value;
        else if(option=="--rate") { auto n=uintArgument(value); if(n<8000||n>192000) throw std::runtime_error("sample rate must be 8000..192000"); rate=unsigned(n); }
        else if(option=="--frames") frames=uintArgument(value);
        else if(option=="--seed") seed=uintArgument(value);
        else throw std::runtime_error("unknown option: "+option);
    }
    if(eventPath.empty()||outputPath.empty()||frames==0||frames>(std::numeric_limits<uint32_t>::max()-48)/4)
        throw std::runtime_error("usage: ceres_render --events events.tsv --output output.wav --frames N [--preset NAME --patch file.patch --rate 48000 --seed 50]");
    auto patch=ceres::preset(preset); if(!patchPath.empty()) loadPatch(patch,patchPath);
    auto events=loadEvents(eventPath,frames);
    ceres::Synth synth(rate,seed); synth.setPatch(patch);
    std::ofstream out(outputPath,std::ios::binary); if(!out) throw std::runtime_error("cannot write WAV");
    // IEEE 32-bit float, mono; fixed gain and no clip normalization.
    out.write("RIFF",4); u32(out,uint32_t(48+frames*4)); out.write("WAVEfmt ",8);
    u32(out,16); u16(out,3); u16(out,1); u32(out,rate); u32(out,rate*4); u16(out,4); u16(out,32);
    out.write("fact",4); u32(out,4); u32(out,uint32_t(frames)); out.write("data",4); u32(out,uint32_t(frames*4));
    size_t index=0; double peak=0,energy=0; uint64_t above=0;
    const auto started=std::chrono::steady_clock::now();
    for(uint64_t frame=0;frame<frames;++frame) {
        while(index<events.size()&&events[index].frame==frame) dispatch(synth,events[index++]);
        float value=synth.process();
        if(!std::isfinite(value)) throw std::runtime_error("nonfinite sample at "+std::to_string(frame));
        peak=std::max(peak,std::abs(double(value))); energy+=double(value)*value; above+=(std::abs(value)>1);
        uint32_t bits; static_assert(sizeof(value)==sizeof(bits),"float must be 32 bit"); std::memcpy(&bits,&value,4); u32(out,bits);
    }
    out.close(); if(!out) throw std::runtime_error("WAV write failed");
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    std::cout<<std::setprecision(12)<<"{\"sample_rate\":"<<rate<<",\"frames\":"<<frames<<",\"channels\":1,\"sample_format\":\"float32\",\"seed\":"<<seed
       <<",\"preset\":\""<<preset<<"\",\"peak\":"<<peak<<",\"rms\":"<<std::sqrt(energy/frames)<<",\"samples_above_unity\":"<<above
       <<",\"render_seconds\":"<<elapsed<<",\"audio_seconds\":"<<double(frames)/rate<<",\"realtime_factor\":"<<(double(frames)/rate)/elapsed
       <<",\"normalization\":\"none\",\"timing_scope\":\"native offline DSP plus WAV I/O; not host deadline performance\"}\n";
    return 0;
 } catch(const std::exception& e) { std::cerr<<"ceres_render: "<<e.what()<<'\n'; return 1; }
}

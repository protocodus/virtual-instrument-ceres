#include "ceres.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

ceres::Patch screenshot() {
    ceres::Patch p;
    p.saw=1; p.pulse=.54; p.sine=.84; p.noise=0;
    p.pulseWidth=.58; p.hpCutoff=28; p.hpResonance=0;
    p.lpCutoff=1750; p.lpResonance=.2; p.keyTrack=1;
    p.filterInitial=-2.5; p.filterPeak=1.4;
    p.filterAttack=1.4; p.filterDecay=3.5; p.filterRelease=2.5;
    p.ampAttack=1; p.ampDecay=2; p.ampSustain=.86; p.ampRelease=2.8;
    p.lfoWaveform=0; p.lfoRate=.22; p.vibratoCents=0; p.filterLfo=.18; p.tremolo=0;
    p.ringMix=.73; p.ringRate=1.26; p.ringDepth=8; p.ringAttack=.0087; p.ringDecay=.05;
    p.pwmRate=1.48; p.pwmDepth=.31; p.expressionSource=1;
    p.pressureVolume=.45; p.pressureFilter=1; p.pressureVibrato=0;
    p.globalRelease=0; p.sustainMode=1; p.filterLevel=1; p.ampLevel=1;
    p.volume=.42; p.octave=0; p.tuneCents=-10;
    return p;
}
ceres::Patch stock() {
    auto p=screenshot(); p.saw=.45; p.pulse=.55; p.sine=0;
    p.ringMix=0; p.ringRate=30; p.ringDepth=0; p.ringAttack=.01; p.ringDecay=.35;
    p.pwmRate=.35; p.pwmDepth=.12; return p;
}
void save(const std::string& path,const std::vector<float>& data) {
    std::ofstream f(path,std::ios::binary); f.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(float));
}
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const std::string root=argv[1];
    // A steal on the same pitch isolates assignment discontinuities from a
    // normal pitch waveform change. Four boards are already at full sound.
    for(unsigned factor: {1u,4u})for(const std::string kind:{"screenshot","no-ring","equal-velocity","no-expression","stock"}) {
        auto p=kind=="stock"?stock():screenshot();
        if(kind=="no-ring")p.ringMix=0;
        if(kind=="no-expression"){p.pressureVolume=0;p.pressureFilter=0;}
        for(bool released: {false,true})for(bool changePitch: {false,true}) {
            ceres::Synth event(48000,50),control(48000,50);
            event.configure(4,factor);control.configure(4,factor);
            event.setPatch(p);control.setPatch(p);
            for(auto* s:{&event,&control})for(int n=0;n<4;++n)s->noteOn(60,.2);
            std::vector<float> a,b;
            for(int n=0;n<48000;++n){a.push_back(event.process());b.push_back(control.process());}
            if(released){event.noteOff(60);control.noteOff(60);for(int n=0;n<2400;++n){a.push_back(event.process());b.push_back(control.process());}}
            const auto before=event.allVoiceState()[0];
            event.noteOn(changePitch?84:60,kind=="equal-velocity"?.2:1);
            const auto after=event.allVoiceState()[0];
            std::printf("%s/%u/%d/%d key %d->%d amp %.17g->%.17g filter %.17g->%.17g phase %.17g->%.17g\n",kind.c_str(),factor,int(released),int(changePitch),before.keyNote,after.keyNote,before.ampEnvelope,after.ampEnvelope,before.filterEnvelope,after.filterEnvelope,before.oscillatorPhase,after.oscillatorPhase);
            for(int n=0;n<9600;++n){a.push_back(event.process());b.push_back(control.process());}
            const auto stem=root+"/"+kind+"-"+std::to_string(factor)+"x-"+(released?"released":"held")+"-"+(changePitch?"pitch":"same");
            save(stem+"-event.f32",a);save(stem+"-control.f32",b);
        }
    }
}

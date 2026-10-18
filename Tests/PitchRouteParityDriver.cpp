// Uses the parent API except the explicitly compiled candidate-only selector.
// Produces synthetic raw float samples, with no recording-linked inputs.
#include "ceres.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc<2)return 2;
    std::ofstream out(argv[1],std::ios::binary);
    for(double rate:{44100.,48000.,96000.,192000.})for(int mode:{0,1,2}) {
        auto p=ceres::preset(mode==1?"flute":"brass1");
        p.calibrationNetwork=mode;p.calibrationAlpha=1.2;p.calibrationHpBias=2;p.calibrationLpBias=-1;
        p.filterLfo=.12;p.noise=.04;p.tremolo=.3;p.ringMix=.2;p.ringDepth=1.4;
        p.keyTrack=.5;p.tuneCents=17;p.pressureVolume=.4;p.pressureVibrato=19;
        p.pressureFilter=mode==0?.5:0;p.sustainMode=2;
        p.ampAttack=.005;p.filterAttack=.007;p.ampRelease=.035;p.filterRelease=.06;
        ceres::Synth s(rate,82);s.setPatch(p);
#if defined(CERES_PITCH_ROUTE_PARITY_API)
        if(argc>2)s.setResearchPitchRoute(ceres::ResearchPitchRoute::Shared);
#else
        (void)argc;
#endif
        for(unsigned f=0;f<8000;++f) {
            if(f==0){s.noteOn(60,.3);s.noteOn(67,.9);s.pitchBend(-.7);s.pressure(.2);}
            if(f==200)s.noteOn(72,.6);
            if(f%137==0){s.pitchBend(.7*std::sin(double(f)*.01));s.pressure(double(f%101)/100);}
            if(f==500)s.sustain(true);
            if(f==700)s.noteOff(60);
            if(f==800)s.noteOn(81,.7);
            if(f==1000){s.noteOn(76,.8);s.noteOn(76,.2);}
            if(f==1200)s.noteOff(76);
            if(f==1500)s.sustain(false);
            if(f==2000)s.allNotesOff();
            if(f==3000)s.allSoundOff();
            if(f==3300)s.noteOn(55,.8);
            if(f==4000)s.reset();
            if(f==4300){s.noteOn(72,.9);s.pitchBend(.4);}
            if(f==5500)s.noteOff(72);
            const float value=s.process();
            if(!std::isfinite(value))throw std::runtime_error("nonfinite synthetic sample");
            out.write(reinterpret_cast<const char*>(&value),sizeof(value));
        }
    }
    return out?0:1;
}

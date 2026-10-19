#include "ceres.hpp"
#include <cmath>
#include <fstream>
// Factory mappings are native-only; the unchanged numeric Patch path is also
// compared against the embedded build by the separate live-edit driver.
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ofstream out(argv[1],std::ios::binary);if(!out)return 3;
    for(double fs:{8000.,48000.})for(const auto& name:ceres::presetNames()) {
        ceres::Synth synth(fs,50);const auto patch=ceres::preset(name);synth.setPatch(patch);
        for(unsigned frame=0;frame<4096;++frame) {
            if(frame==0)synth.noteOn(60,.8);
            if(frame==59)synth.noteOn(64,.6);
            if(frame==113)synth.noteOn(67,1);
            if(frame==491)synth.noteOn(72,.5);
            if(frame==911){synth.sustain(true);synth.noteOff(60);}
            if(frame==1301)synth.sustain(false);
            if(frame==1703){synth.pitchBend(.21);synth.pressure(.6);}
            if(frame==2001)synth.allNotesOff();
            if(frame==2401)synth.allSoundOff();
            if(frame==2451)synth.noteOn(55,.7);
            if(frame==3001){synth.reset();synth.noteOn(76,.4);}
            const auto sample=synth.process();if(!std::isfinite(sample))return 4;
            out.write(reinterpret_cast<const char*>(&sample),sizeof(sample));
        }
    }
    return out?0:5;
}

#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    FILE* file=std::fopen(argv[1],"wb");if(!file)return 3;
    for(double rate:{44100.,48000.,88200.,96000.,192000.})
    for(unsigned voices:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}){
        ceres::Synth s(rate,50);if(!s.configure(voices,factor))return 4;
        ceres::Patch p;p.saw=.7;p.pulse=.4;p.noise=.05;p.sine=.13;p.pwmDepth=.22;
        p.filterAttack=.003;p.filterDecay=.08;p.filterRelease=.02;
        p.ampAttack=.002;p.ampDecay=.03;p.ampRelease=.04;p.ampSustain=.6;
        p.drive=2;p.filterLfo=.4;p.tremolo=.6;p.ringMix=.8;p.ringRate=30;p.ringDepth=2;
        p.resonanceOffset=-3;p.hpResonance=.7;p.lpResonance=.6;
        p.voiceSpreadCents=.3;p.driftCents=1;p.expressionSource=1;
        p.pressureVolume=.7;p.pressureFilter=.8;p.pressureVibrato=14;
        p.sustainMode=2;p.outputTrimDb=-6;s.setPatch(p);
        for(unsigned f=0;f<4096;++f){
            if(f==0)for(unsigned i=0;i<voices;++i)s.noteOn(40+int(i)*3,.1+.9*i/voices);
            if(f==111){s.pitchBend(.37);s.pressure(.65);}
            if(f==201)s.noteOn(78,.7);
            // Overflow a repeated-pitch allocation, then consume the stolen
            // presses before the surviving owners, with velocity retargets.
            if(f==230)for(unsigned i=0;i<voices+2;++i)s.noteOn(60,.15+.7*i/(voices+2));
            if(f==240 || f==245)s.noteOff(60);
            if(f==250){s.sustain(true);s.noteOff(60);}
            if(f==260){s.noteOn(60,.95);s.noteOn(60,.2);}
            if(f==270)s.sustain(false);
            if(f==400){s.sustain(true);s.allNotesOff();}
            if(f==500){p.lpResonance=.2;p.hpResonance=.1;s.setPatch(p);}
            if(f==600)s.sustain(false);
            if(f==1100 || f==1150 || f==1400 || f==2200) {
                p.ringDepth=f==1400?0:(f==2200?6:8);s.setPatch(p);
            }
            if(f==1600)s.allSoundOff();
            if(f==1650)s.noteOn(60,.2);
            if(f==2000){s.reset();s.noteOn(72,.9);}
            if(f==2600)s.noteOff(72);
            if(f==3000){p.filterInitial=-5;s.setPatch(p);}
            if(f==3500)s.noteOn(67,.5);
            const float y=s.process();if(!std::isfinite(y))return 5;
            if(std::fwrite(&y,sizeof y,1,file)!=1)return 6;
        }
    }
    return std::fclose(file)==0?0:7;
}

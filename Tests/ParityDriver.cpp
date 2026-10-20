#include "ceres.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

// Identical harness for the original experiment, current native and embedded
// variants. Deliberately uses only the shared numeric Patch / Synth API.
int main(int argc,char** argv) {
    if(argc<2 || argc>3)return 2;
    const bool panelControls=argc==3;
    const bool capacitorControls=argc==3 && std::strcmp(argv[2],"capacitors")==0;
    const bool expressionControls=argc==3 && std::strcmp(argv[2],"expression")==0;
    const bool soundPathControls=argc==3 && (std::strcmp(argv[2],"soundpath")==0 || expressionControls);
    std::ofstream output(argv[1],std::ios::binary);
    if(!output)return 3;
    for(double rate:{44100.,48000.,96000.,192000.})for(unsigned scene=0;scene<8;++scene) {
        ceres::Patch patch;
        patch.saw=scene==0||scene==5||scene==7;
        patch.pulse=scene==1||scene==5;
        patch.sine=scene==2 ? 1 : .17;
        patch.noise=scene==3||scene==5 ? .35 : 0;
        patch.hpCutoff=scene==7 ? 22.575169232892954 : 80;
        patch.lpCutoff=scene==7 ? 19000 : 3200;
        patch.hpResonance=.71;patch.lpResonance=.88;
        patch.pwmDepth=.2;patch.pwmRate=.85;patch.pwmPhase=.41;
        patch.pulseWidth=.63;patch.resetMicroseconds=scene==7 ? 50 : 6;
        patch.filterInitial=-2.7;patch.filterPeak=3.4;
        patch.hpEnvelopeScale=.124090909090909;patch.lpEnvelopeScale=.5393617021276597;
        patch.filterAttack=.011;patch.filterDecay=.22;patch.filterRelease=.13;
        patch.ampAttack=.003;patch.ampDecay=.08;patch.ampSustain=.73;patch.ampRelease=.21;
        patch.filterAttackHeadroom=scene==4 ? 100000 : .006783654906304231;
        patch.ampAttackHeadroom=scene==4 ? .4 : .01;
        patch.filterAttackSaturation=scene==6 ? .49 : 0;
        patch.ampAttackSaturation=scene==6 ? .001 : 0;
        patch.globalRelease=scene==7 ? .4 : 0;
        patch.lfoRate=6.669;patch.vibratoCents=13;patch.tremolo=.71;patch.filterLfo=.2;
        // With the intentionally corrected unequal SUB->VCF route inactive,
        // the new controls' neutral defaults must preserve the old engine.
        if(argc==3 && std::strcmp(argv[2],"neutral")==0)patch.filterLfo=0;
        patch.pressureVolume=.2;patch.pressureFilter=.3;patch.pressureVibrato=4;
        patch.voiceSpreadCents=2;patch.driftCents=3;patch.velocitySensitivity=.4;
        patch.octave=scene==2 ? -1 : 0;patch.tuneCents=31.793;patch.drive=scene==3 ? .05 : 1;
        if(panelControls) {
            // String keys keep this default-path driver compilable against
            // the frozen pre-controls core for exact legacy comparisons.
            if(!ceres::setParameter(patch,"ring_mix",scene/7.) ||
               !ceres::setParameter(patch,"ring_rate",7+scene*30) ||
               !ceres::setParameter(patch,"ring_depth",scene) ||
               !ceres::setParameter(patch,"ring_attack",.007+scene*.003) ||
               !ceres::setParameter(patch,"ring_decay",.08+scene*.05) ||
               !ceres::setParameter(patch,"lfo_waveform",scene%5) ||
               !ceres::setParameter(patch,"sustain_mode",1+scene%2))return 6;
        }
        if(argc==3 && std::strcmp(argv[2],"ring-bypass")==0)patch.ringMix=0;
        if(soundPathControls) {
            if(!ceres::setParameter(patch,"brilliance",-15+scene*30./7) ||
               !ceres::setParameter(patch,"resonance_offset",-10+scene*20./7) ||
               !ceres::setParameter(patch,"amp_level",scene/7.) ||
               !ceres::setParameter(patch,"output_trim_db",-24+scene*48./7))return 7;
        }
        if(expressionControls && !ceres::setParameter(patch,"expression_source",1))return 8;
        // Glide and touch-wah ride with the expression scenes: both are
        // per-board key/touch paths shared by the native and embedded cores.
        if(expressionControls && (!ceres::setParameter(patch,"glide_mode",scene%3) ||
           !ceres::setParameter(patch,"glide_rate",10+scene*9) ||
           !ceres::setParameter(patch,"pressure_wah",.3)))return 9;
        ceres::Synth synth(rate,50+scene);synth.setPatch(patch);
        for(unsigned frame=0;frame<12000;++frame) {
            if(frame==0)synth.noteOn(60,.8);
            if(frame==73)synth.noteOn(64,.5);
            if(frame==149)synth.noteOn(67,1);
            if(frame==997)synth.noteOn(72,.7);
            if(frame==1499)synth.noteOn(60,.3); // Repeated pitch beyond four held voices.
            if(frame==1500)synth.sustain(true);
            if(frame==1777){synth.noteOff(60);synth.noteOff(64);}
            if(frame==2048){ceres::setParameter(patch,"lp_cutoff",870.);synth.setPatch(patch);}
            if(frame==2223){synth.pitchBend(-.37);synth.pressure(.72);}
            if(frame==3000)synth.sustain(false);
            if(frame==3600)synth.allNotesOff();
            if(frame==4001){patch.globalRelease=.63;synth.setPatch(patch);}
            if(soundPathControls && frame==4501) {
                ceres::setParameter(patch,"resonance_offset",0);
                ceres::setParameter(patch,"amp_level",.73);synth.setPatch(patch);
            }
            if(frame==6000)synth.allSoundOff();
            if(capacitorControls && (frame==2048 || frame==4501 || frame==6001)) {
                patch.hpResonance=frame==4501 ? .1:.8;
                patch.lpResonance=frame==4501 ? .9:.2;
                patch.filterInitial=frame==6001 ? -5:-.1;
                patch.resonanceOffset=frame==4501 ? -4:6;
                synth.setPatch(patch);
            }
            if(expressionControls && (frame==5000 || frame==5500)) {
                ceres::setParameter(patch,"expression_source",frame==5500 ? 1:0);synth.setPatch(patch);
            }
            if(frame==6111)synth.noteOn(48,.9);
            if(frame==8192){synth.reset();synth.noteOn(76,.6);}
            if(frame==9100)synth.noteOff(76);
            const float sample=synth.process();
            if(!std::isfinite(sample))return 4;
            output.write(reinterpret_cast<const char*>(&sample),sizeof(sample));
        }
    }
    return output ? 0 : 5;
}

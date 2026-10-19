#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Uses only the original four-board API so this identical source compiles
// against the installed b11 DSP (2ee6cf2) as well as the current candidate.
// No sound payload or fitted reference waveform is used by these witnesses.
int main(int argc,char** argv) {
    const bool requireFixed=argc==2&&std::strcmp(argv[1],"--require-fixed")==0;
    if(argc>2||(argc==2&&!requireFixed))return 1;
    constexpr double fs=48000,pi=3.14159265358979323846;
    constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
    auto patch=ceres::Patch{};
    patch.saw=0;patch.sine=1;patch.hpCutoff=100;patch.keyTrack=0;
    patch.filterInitial=patch.filterPeak=0;patch.expressionSource=1;
    patch.pressureVolume=.7;patch.pressureFilter=1;
    patch.ampAttack=.001;patch.ampSustain=1;patch.ampRelease=2.8;
    patch.volume=.42;
    ceres::Synth synth(fs,50);synth.setPatch(patch);
    for(unsigned i=0;i<4;++i)synth.noteOn(60,.2);
    for(unsigned i=0;i<960;++i)synth.process();
    const auto before=synth.voiceState()[0];
    synth.noteOn(72,.9);
    const auto assigned=synth.voiceState()[0];
    const bool stateContinuous=before.ampEnvelope==assigned.ampEnvelope &&
        before.filterEnvelope==assigned.filterEnvelope && before.oscillatorPhase==assigned.oscillatorPhase;
    synth.process();
    const double cv=std::log2((synth.voiceState()[0].hpFrequency-floorHz)/(100-floorHz));
    const double expected=.9+(.2-.9)*std::exp(-1/(fs*.005));
    // All four original MIDI60 presses now have one stolen owner. Its first
    // eventual note-off must leave the three surviving MIDI60 owners held.
    synth.noteOff(60);
    unsigned surviving=0;for(const auto& v:synth.voiceState())if(v.held&&v.note==60)++surviving;
    // Show the intentional difference between the shortest exposed attack and
    // a 1ms attack. Both retain their exact requested RC time and charge.
    auto fast=patch;fast.ampAttack=.0001;
    const double first01=ceres::envelopeTrace(fs,fast,false,1,{{0,true}})[0];
    fast.ampAttack=.001;
    const double first1=ceres::envelopeTrace(fs,fast,false,1,{{0,true}})[0];
    std::printf("{\n  \"sample_rate\":48000,\n  \"voices\":4,\n  \"factor\":4,\n"
                "  \"assignment_preserves_oscillator_and_eg_charge\":%s,\n"
                "  \"first_host_sample_velocity_cv\":%.17g,\n"
                "  \"expected_continuous_velocity_cv\":%.17g,\n"
                "  \"continuous_velocity_error\":%.17g,\n"
                "  \"remaining_same_pitch_held_owners_after_stale_off\":%u,\n"
                "  \"expected_remaining_same_pitch_held_owners\":3,\n"
                "  \"attack_first_48k_sample_0_1ms\":%.17g,\n"
                "  \"attack_first_48k_sample_1ms\":%.17g\n}\n",
                stateContinuous?"true":"false",cv,expected,std::abs(cv-expected),surviving,first01,first1);
    return requireFixed&&(!stateContinuous||std::abs(cv-expected)>1e-12||surviving!=3)?2:0;
}

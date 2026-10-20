#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>
namespace {
constexpr double pi=3.14159265358979323846;
void require(bool b,const char* what){if(!b){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
void near(double a,double b,double tolerance,const char* what){require(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance,what);}
void advance(ceres::Synth& s,unsigned n){for(unsigned i=0;i<n;++i)require(std::isfinite(s.process()),"finite output");}
void configuration(){
    ceres::Synth s(48000,50), reference(48000,50);
    require(s.polyphony()==4&&s.oversamplingFactor()==4,"legacy defaults");
    for(unsigned n:{0u,17u,1000u})require(!s.configure(n,4),"invalid voices rejected");
    for(unsigned f:{0u,3u,8u})require(!s.configure(4,f),"invalid factor rejected");
    s.noteOn(60,.7);reference.noteOn(60,.7);
    require(!s.configure(16,1),"preclock assigned voice blocks mutation");
    for(unsigned i=0;i<1000;++i)require(s.process()==reference.process(),"rejected configure preserves every sample");
    require(s.configure(4,4),"equal configuration live no-op");
    require(!s.configure(16,2),"live reconfiguration rejected");
    s.allSoundOff();require(!s.configure(16,2),"panic does not authorize clock reset");
    s.reset();require(s.configure(16,2),"reset permits configuration");
    s.noteOn(60);advance(s,1);s.reset();
    require(s.polyphony()==16&&s.oversamplingFactor()==2,"reset retains configuration");
    require(s.activeVoices()==0,"reset clears voices");
    for(unsigned n=1;n<=16;++n){s.reset();require(s.configure(n,1)&&s.polyphony()==n,"every voice-count value accepted");}
}
void voices(){
    for(unsigned n:{1u,4u,16u})for(unsigned factor:{1u,2u,4u}){
        ceres::Synth s(48000,17);require(s.configure(n,factor),"configure polyphony");
        ceres::Patch p;p.ampAttack=.001;p.ampRelease=.01;p.sustainMode=2;s.setPatch(p);
        auto before=s.allVoiceState();
        for(unsigned i=0;i<n;++i)s.noteOn(40+int(i),.2+.8*i/n);
        require(s.activeVoices()==n,"all configured voices allocated");advance(s,1000);
        auto state=s.allVoiceState();
        for(unsigned i=0;i<n;++i)require(state[i].held&&state[i].note==40+int(i),"distinct initial allocation");
        for(unsigned i=n;i<16;++i)require(!state[i].held&&state[i].oscillatorPhase==before[i].oscillatorPhase,"unconfigured slots unclocked");
        s.noteOn(90,.8);state=s.allVoiceState();
        for(unsigned i=0;i<n;++i)require(state[i].held&&state[i].note==40+int(i),"key beyond held polyphony is not stored");
        s.noteOff(90);
        s.sustain(true);s.noteOff(40);state=s.allVoiceState();require(state[0].pedalHeld&&!state[0].held,"pedal gate preserved");
        const double phase=state[0].oscillatorPhase;s.noteOn(91,.5);state=s.allVoiceState();
        require(state[0].note==91&&state[0].oscillatorPhase==phase,"pedal-held board is re-keyed without a phase reset");
        s.allNotesOff();advance(s,200);require(s.activeVoices()==n,"all-notes-off honors pedal");
        s.sustain(false);advance(s,3000);require(s.activeVoices()==0,"pedal-up releases all configured boards");
        s.allSoundOff();for(unsigned i=0;i<100;++i)near(s.process(),0,0,"panic exact silence");
        // Repeated-note ownership uses FIFO, independently of polyphony.
        s.reset();for(unsigned i=0;i<n;++i)s.noteOn(60,.5);s.noteOff(60);
        state=s.allVoiceState();require(!state[0].held,"repeated note FIFO oldest off");
        for(unsigned i=1;i<n;++i)require(state[i].held,"repeated note others stay held");
    }
}
void clockAndCoefficients(){
    for(double fs:{8000.,44100.,48000.,96000.,192000.,384000.})for(unsigned factor:{1u,2u,4u}){
        ceres::Synth s(fs,50);require(s.configure(1,factor),"rate configuration");
        ceres::Patch p;p.saw=0;p.sine=1;p.filterInitial=-2;p.filterAttack=.001;p.filterDecay=.3;
        p.ampAttack=.001;p.ampDecay=.3;p.ampSustain=.4;p.ringMix=1;p.ringRate=220;
        p.boardPitchTolerance=0; // the clock check needs the nominal key frequency
        s.setPatch(p);
        const auto before=s.allVoiceState()[0];s.noteOn(60);advance(s,100);
        const auto after=s.allVoiceState()[0];
        const double f=440*std::exp2((60.-69)/12);
        double expected=before.oscillatorPhase;
        for(unsigned i=0;i<100*factor;++i){expected+=f/(fs*factor);expected-=std::floor(expected);}
        near(after.oscillatorPhase,expected,2e-13,"actual quality sample clock");
        p.resonanceOffset=8;p.lpResonance=.9;s.setPatch(p);advance(s,128);
        const auto q=s.soundPathState();
        near(q.resonanceVoltageOffset,4*(1-std::exp(-128/(fs*1.1))),3e-12,"Q tau independent of quality");
        near(q.lpResonanceAnchor,.9*(1-std::exp(-128/(fs*1.1))),3e-12,"Q anchor rate conversion");
        s.noteOff(60);advance(s,128);
    }
}
void outputFilters(){
    for(unsigned factor:{1u,2u,4u}){
        const unsigned length=factor==1?32:62*factor+1;
        std::vector<double> h(length,0);
        for(unsigned phase=0;phase<factor;++phase){
            auto out=ceres::outputRateImpulse(factor,70,phase);
            for(unsigned n=0;n<out.size();++n){
                const unsigned k=n*factor+factor-1-phase;
                if(k<length)h[k]=out[n];else near(out[n],0,0,"finite resampler support");
            }
        }
        double sum=0,moment=0;
        for(unsigned k=0;k<length;++k){sum+=h[k];moment+=k*h[k];}
        near(sum,1,2e-14,"output DC gain unity");near(moment/sum/factor,31,2e-12,"31 host sample group delay");
        if(factor==1){for(unsigned k=0;k<length;++k)near(h[k],k==31?1:0,0,"1x is exact delay only");}
        else {
            for(unsigned k=0;k<length;++k)near(h[k],h[length-1-k],1e-16,"linear phase symmetry");
            auto response=[&](double outputCycles){std::complex<double> z{};for(unsigned k=0;k<length;++k)z+=h[k]*std::polar(1.,-2*pi*outputCycles*k/factor);return std::abs(z);};
            near(response(.2),1,1e-5,"passband retained");
            require(response(.65)<1e-5,"images rejected at .65 output cycles");
        }
    }
}
}
int main(){configuration();voices();clockAndCoefficients();outputFilters();std::puts("PASS configuration: 1/4/16 voices, real1/2/4x clocks, lifecycle and independent output-filter delay/response");}

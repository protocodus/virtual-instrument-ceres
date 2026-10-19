#include "ceres.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
namespace {
constexpr double pi=3.14159265358979323846;
void require(bool v,const char* text){if(!v){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
void near(double a,double b,double e,const char* text){if(!(std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=e)){std::fprintf(stderr,"actual %.17g expected %.17g error %.17g tolerance %.17g\n",a,b,std::abs(a-b),e);require(false,text);}}
void startupAndClock(){
 for(double fs:{44100.,48000.,96000.,192000.})for(unsigned factor:{1u,2u,4u}){
  std::fprintf(stderr,"rate %.0f factor %u\n",fs,factor);
  ceres::Synth s(fs,50);require(s.configure(4,factor),"configure quality");ceres::Patch p;p.ringDepth=3;s.setPatch(p);
  near(s.modulationState().ringDepth,3,0,"unclocked saved depth precharged");s.noteOn(60);s.process();
  p.ringDepth=8;s.setPatch(p);near(s.modulationState().ringDepth,3,0,"live write never jumps actual depth");
  const unsigned frames=static_cast<unsigned>(std::ceil(fs*.020));
  // At most one rounded addition per internal sample, with |depth|<=8.
  // Bound cumulative rounding explicitly; the final endpoint remains exact.
  const double roundoff=8.*frames*factor*std::numeric_limits<double>::epsilon();
  for(unsigned n=1;n<=frames;++n){
   // Identical-target patch notifications and unrelated controls must not
   // restart the transition. Closed-form elapsed time is the independent oracle.
   if(n%11==0){p.volume=.3+.1*(n%3);s.setPatch(p);}
   s.process();near(s.modulationState().ringDepth,3+5.*n/frames,roundoff,"20ms depth interpolation at every quality/rate");
  }
  near(s.modulationState().ringDepth,8,0,"finite exact endpoint");s.process();near(s.modulationState().ringDepth,8,0,"endpoint stays exact");
  p.ringDepth=0;s.setPatch(p);for(unsigned n=0;n<frames/2;++n)s.process();
  const double before=s.modulationState().ringDepth;p.ringDepth=6;s.setPatch(p);
  near(s.modulationState().ringDepth,before,0,"reversal has no coordinate jump");
  for(unsigned n=1;n<=frames;++n){s.process();near(s.modulationState().ringDepth,before+(6-before)*n/frames,roundoff,"reversal retargets current value");}
  near(s.modulationState().ringDepth,6,0,"reversal finite endpoint");
  p.ringDepth=2;s.setPatch(p);s.process();const double current=s.modulationState().ringDepth;s.allSoundOff();
  near(s.modulationState().ringDepth,current,0,"panic retains depth interpolation state");s.process();require(s.modulationState().ringDepth<current,"panic keeps smoothing clock running");
  s.reset();near(s.modulationState().ringDepth,2,0,"explicit reset precharges target");
 }
}
void phaseAndEnvelope(){
 for(unsigned factor:{1u,2u,4u}){
  const double fs=48000.*factor;ceres::Synth moving(fs,50),unchanged(fs,50);
  require(moving.configure(4,1)&&unchanged.configure(4,1),"internal-step probes");
  ceres::Patch p;p.ringRate=30;p.ringDepth=0;p.ringAttack=.2;p.ringDecay=3;p.ringMix=1;
  moving.setPatch(p);unchanged.setPatch(p);moving.noteOn(69);unchanged.noteOn(69);
  double phase=0,previous=30,maxStep=0;const unsigned edge=static_cast<unsigned>(fs*.1),count=static_cast<unsigned>(fs*.15);
  for(unsigned i=0;i<count;++i){
   if(i==edge){p.ringDepth=8;moving.setPatch(p);}
   moving.process();unchanged.process();const auto m=moving.modulationState(),u=unchanged.modulationState();
   near(m.ringEnvelope,u.ringEnvelope,0,"depth automation never changes or restarts ring EG");
   const double expected=std::max(.2,std::min(220.,30*std::exp2(m.ringDepth*m.ringEnvelope)));
   near(m.ringFrequency,expected,0,"existing exponential map and .2..220Hz cap retained");
   near(m.ringCarrier,std::sin(2*pi*phase),2e-12,"free carrier phase follows integrated frequency, never resets");
   phase=std::fmod(phase+m.ringFrequency/fs,1.);
   if(i>=edge)maxStep=std::max(maxStep,std::abs(m.ringFrequency-previous));previous=m.ringFrequency;
  }
  require(maxStep<1.3/factor,"native internal-sample ramp removes the190Hz single-step control jump");
 }
}
}
int main(){startupAndClock();phaseAndEnvelope();std::puts("PASS ring Depth:12 rate/quality paths,20ms closed-form ramp/reversal, identical target immunity, startup/reset/panic, unchanged EG/free phase/cap, reduced discrete frequency jump");}

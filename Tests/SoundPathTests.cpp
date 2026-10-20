#include "ceres.hpp"
#include <algorithm>
#include <complex>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
constexpr double floorHz=1/(2*pi*1.5e-9*4.7e6);
void require(bool good,const char* why) {if(!good)throw std::runtime_error(why);}
void near(double actual,double expected,double tolerance,const char* why) {
    if(!std::isfinite(actual) || !std::isfinite(expected) || std::abs(actual-expected)>tolerance) {
        std::cerr<<"actual="<<actual<<" expected="<<expected<<'\n';
        throw std::runtime_error(why);
    }
}
double advance(ceres::Synth& synth,unsigned frames) {
    double energy=0;
    for(unsigned i=0;i<frames;++i) {const auto x=synth.process();require(std::isfinite(x),"Finite output");energy+=x*x;}
    return energy;
}

void controlRanges() {
    ceres::Patch p;
    require(p.brilliance==0 && p.resonanceOffset==0 && p.ampLevel==1,"Neutral defaults");
    for(const char* name:{"brilliance","resonance_offset","amp_level","output_trim_db"})
        require(!ceres::setParameter(p,name,std::numeric_limits<double>::infinity()),"Nonfinite control rejected");
    require(ceres::setParameter(p,"brilliance",-100) && p.brilliance==-15,"BR lower endpoint");
    require(ceres::setParameter(p,"brilliance",100) && p.brilliance==15,"BR upper endpoint");
    require(ceres::setParameter(p,"resonance_offset",-100) && p.resonanceOffset==-10,"RES delta lower endpoint");
    require(ceres::setParameter(p,"resonance_offset",100) && p.resonanceOffset==10,"RES delta upper endpoint");
    require(ceres::setParameter(p,"amp_level",-1) && p.ampLevel==0,"VCA lower endpoint");
    require(ceres::setParameter(p,"amp_level",2) && p.ampLevel==1,"VCA upper endpoint");
    require(ceres::setParameter(p,"output_trim_db",-100) && p.outputTrimDb==-24,"Output trim lower endpoint");
    require(ceres::setParameter(p,"output_trim_db",100) && p.outputTrimDb==24,"Output trim upper endpoint");
    p.brilliance=p.resonanceOffset=p.ampLevel=p.outputTrimDb=std::numeric_limits<double>::quiet_NaN();
    ceres::Synth invalid(8000,3),defaults(8000,3);invalid.setPatch(p);
    invalid.noteOn(60);defaults.noteOn(60);
    for(unsigned i=0;i<500;++i)require(invalid.process()==defaults.process(),"Direct invalid new values use neutral defaults");
}

void brillianceAndSubKcl() {
    // Independently solve the grounded cutoff-control summing node with
    // arbitrary additional shunts. Ratios to the manual-source derivative
    // cancel those shunts; this is the stated stiff-source coordinate.
    for(double extra:{0.,1/3000.,1/1800.,.1}) {
        for(bool hp:{false,true}) {
            const double rm=39000,rb=hp?680000:220000,rs=hp?150000:47000;
            // Both levers: the printed IG00156 law, 1 oct/V at the 39k lever
            // (guide p.34), replacing the provisional .7/.65 on 2026-10-20.
            const double slope=1;
            const double total=1/rm+1/rb+1/rs+extra;
            const double duManual=(1/rm)/total;
            near(((1/rb)/total)/duManual*slope,slope*rm/rb,1e-14,"BR nodal ratio");
            near(((1/rs)/total)/duManual*slope,slope*rm/rs,1e-14,"SUB nodal ratio");
        }
    }
    for(double br:{-15.,0.,15.})for(double tracking:{0.,.4,1.}) {
        ceres::Patch p;p.filterInitial=p.filterPeak=0;p.hpCutoff=100;p.lpCutoff=900;
        p.hpEnvelopeScale=.21;p.lpEnvelopeScale=2.1;p.filterLfo=1.3;p.lfoRate=7;
        p.keyTrack=tracking;p.tuneCents=230;p.brilliance=br;
        ceres::Synth s(8000,1);s.setPatch(p);s.noteOn(67);s.pitchBend(.3);
        for(unsigned n=0;n<250;++n) {
            s.process();const auto v=s.voiceState()[0];const double lfo=s.modulationState().sub;
            const double key=(67-60+.3)*tracking/12+230./1200;
            const double hpOct=std::log2((v.hpFrequency-floorHz)/(p.hpCutoff-floorHz))-key;
            const double lpOct=std::log2((v.lpFrequency-floorHz)/(p.lpCutoff-floorHz))-key;
            near(hpOct,br*39000/680000+lfo*1.3*(47000./150000),2e-14,"HP actual summed control currents");
            near(lpOct,br*39000/220000+lfo*1.3,2e-14,"LP actual summed control currents");
        }
        // At zero variable gm, neither BR nor SUB may scale the fixed feed.
        p.hpCutoff=p.lpCutoff=5;s.setPatch(p);s.process();
        near(s.voiceState()[0].hpFrequency,floorHz,0,"BR fixed HP resistor floor");
        near(s.voiceState()[0].lpFrequency,floorHz,0,"BR fixed LP resistor floor");
    }
    // Full audio equivalence to the same physical static gm shift, not only
    // a disconnected diagnostic variable. No frequency boundary is active.
    ceres::Patch p;p.hpCutoff=150;p.lpCutoff=1800;p.brilliance=-8;
    auto equivalent=p;equivalent.brilliance=0;
    equivalent.hpCutoff=floorHz+(p.hpCutoff-floorHz)*std::exp2(-8*39000./680000);
    equivalent.lpCutoff=floorHz+(p.lpCutoff-floorHz)*std::exp2(-8*39000./220000);
    ceres::Synth a(8000,71),b(8000,71);a.setPatch(p);b.setPatch(equivalent);a.noteOn(60);b.noteOn(60);
    for(unsigned i=0;i<2000;++i)near(a.process(),b.process(),2e-7,"BR must affect actual filter audio at correct gm position");
}

void resonanceControlCircuit() {
    constexpr double rPreset=220000,rShared=220000,c=10e-6;
    const double conductance=1/rPreset+1/rShared;
    const double tau=c/conductance;
    for(double fs:{8000.,44100.,192000.}) {
        ceres::Patch p;p.hpResonance=.8;p.lpResonance=.55;p.resonanceOffset=8;
        ceres::Synth s(fs,9);s.setPatch(p);s.noteOn(60);
        std::array<double,8> nodal{};
        // Eight independent physical capacitor KCL equations, solved by RK4
        // at HOST rate, independent of production's scalar exact RC update.
        auto derivative=[&](double q,double source){return ((0-q)/rPreset+(source-q)/rShared)/c;};
        const unsigned count=static_cast<unsigned>(fs*.12);
        for(unsigned n=0;n<count;++n) {
            for(double& q:nodal) {
                const double h=1/fs,k1=derivative(q,8),k2=derivative(q+h*k1/2,8);
                const double k3=derivative(q+h*k2/2,8),k4=derivative(q+h*k3,8);
                q+=h*(k1+2*k2+2*k3+k4)/6;
            }
            s.process();const auto state=s.soundPathState();
            for(double q:nodal)near(state.resonanceVoltageOffset,q,3e-12,"Eight pin7 capacitor KCL versus exact production reduction");
            near(state.hpResonance,.8-nodal[0]/10,3e-13,"HP provisional d(Vq) slope/sign");
            near(state.lpResonance,.55-nodal[4]/10,3e-13,"LP independent baseline with same global delta");
        }
        const double before=s.soundPathState().resonanceVoltageOffset;
        near(before,(8/rShared)/conductance*(1-std::exp(-(count/fs)/tau)),3e-12,"Physical RC closed form across rates");
        s.noteOn(64);s.noteOff(60);s.sustain(true);s.allNotesOff();s.allSoundOff();
        near(s.soundPathState().resonanceVoltageOffset,before,0,"Notes, stealing/pedal/panic must not reset Q capacitor");
        p.resonanceOffset=0;p.hpResonance=.2;s.setPatch(p);
        near(s.soundPathState().resonanceVoltageOffset,before,0,"Patch edit preserves capacitor charge");
        advance(s,count);
        near(s.soundPathState().resonanceVoltageOffset,before*std::exp(-(count/fs)/tau),3e-12,"Offset-zero tail cannot bypass smoothing");
        p.resonanceOffset=-10;s.setPatch(p);advance(s,count);
        require(s.soundPathState().resonanceVoltageOffset<0,"Negative delta reverses current while preserving state");
        s.reset();near(s.soundPathState().resonanceVoltageOffset,0,0,"Explicit reset clears control charge");
    }
    // Prove it is wired into audio, not solely the observable control state.
    ceres::Patch p;p.hpCutoff=200;p.lpCutoff=900;p.hpResonance=.65;p.lpResonance=.7;
    ceres::Synth a(8000,11),b(8000,11);a.setPatch(p);p.resonanceOffset=10;b.setPatch(p);
    a.noteOn(60);b.noteOn(60);double difference=0;
    for(unsigned n=0;n<4000;++n){const double delta=a.process()-b.process();difference+=delta*delta;}
    require(difference>1e-5,"Dynamic global resonance must change filtered audio");
}

void commonVcaPlacement() {
    ceres::Patch p;p.saw=0;p.sine=1;p.filterLevel=0;p.ampAttack=.001;p.ampSustain=1;p.ampLevel=.4;
    auto scaled=p;scaled.sine=.4;scaled.ampLevel=1;
    ceres::Synth a(8000,21),b(8000,21);a.setPatch(p);b.setPatch(scaled);a.noteOn(60);b.noteOn(60);
    for(unsigned i=0;i<2000;++i)near(a.process(),b.process(),2e-8,"Common LEVEL includes bypass sine before linear voice coupling");
    p.ampLevel=0;ceres::Synth muted(8000,21);muted.setPatch(p);muted.noteOn(60);
    require(advance(muted,2000)==0,"Common LEVEL closes both filtered and sine paths");
    // Additive LC control, including its zero-gain boundary, differs from a
    // LEVEL*tremolo product. Square SUB makes the two halves unambiguous.
    // R7 LC: SUB lever through 120k against LEVEL's 10V through 270k.
    const double subScale=(1.5/120e3)/(10/270e3);
    p.ampLevel=.2;p.tremolo=1;p.lfoWaveform=3;p.lfoRate=2;
    ceres::Synth additive(8000,21);additive.setPatch(p);additive.noteOn(60);
    require(advance(additive,1800)>1e-5,"Positive square SUB opens the additive common VCA");
    near(additive.soundPathState().commonVcaGain,.2+subScale,1e-15,"R7 SUB current adds to LEVEL");
    require(advance(additive,400)>0,"Closing the common VCA retains downstream capacitor tails");
    require(additive.soundPathState().commonVcaGain==0,"Negative square half closes LC gain");
    additive.allSoundOff();
    require(advance(additive,1200)==0,"Panic clears audio tails while SUB stays in its closed half");
    additive.noteOn(60);
    require(advance(additive,1000)>1e-5,"Next positive half reopens common VCA without restarting SUB");
    near(additive.soundPathState().commonVcaGain,.2+subScale,1e-15,"Positive SUB returns to the same open level");
    // Dynamic LEVEL is upstream of the bus and global ring. Changing master
    // volume at the same instant is not the same circuit state trajectory.
    p.ampLevel=1;p.tremolo=0;p.ringMix=1;p.ringRate=47;
    ceres::Synth level(8000,22),master(8000,22);level.setPatch(p);master.setPatch(p);
    level.noteOn(60);master.noteOn(60);
    for(unsigned n=0;n<2500;++n)require(level.process()==master.process(),"Pre-edit common signal");
    auto finalMute=p;finalMute.volume=0;p.ampLevel=0;level.setPatch(p);master.setPatch(finalMute);
    double deltaEnergy=0;
    for(unsigned n=0;n<1000;++n){const double delta=level.process()-master.process();deltaEnergy+=delta*delta;}
    require(deltaEnergy>1e-7,"Common level acts before bus/ring charge while master is later");
    // The guide's final-column voltages are ideal divider values; the T-board
    // pin they reach sits a 1S1555 drop below, solved here by bisection on
    // the node equation rather than by the engine's fixed-point pass.
    const std::array<double,13> ideal={10,8.47,10,8.333,10,10,6.99,10,8.33,6.41,5,6.80,5.95};
    const auto pin=[](double v) {
        const double vt=1.75*25.85e-3;
        double low=0,high=v;
        for(int i=0;i<200;++i) {
            const double mid=(low+high)/2;
            // 10V source, series resistor set by the ideal ratio, 10k load.
            ((10-vt*std::log(mid/(10e3*2.52e-9)))*v/10>mid ? low : high)=mid;
        }
        return (low+high)/2;
    };
    const auto names=ceres::presetNames();
    for(unsigned i=0;i<ideal.size();++i) {
        const auto factory=ceres::preset(names[i+1]);
        near(factory.ampLevel,pin(ideal[i])/10,1e-12,"Factory final CV maps to common VCA LEVEL");
        near(factory.volume,.55,0,"Factory LEVEL must not be folded into final master volume");
    }
}
void sharedSubPolarity() {
    // PN3 gives both depth pots the same SUO/ground endpoints. R7's
    // positive conductances and the positive LI-to-gain slope imply a
    // positive gain/cutoff relation regardless of the unknown depth scale.
    // Check all five real production sources, including filtered noise.
    for(unsigned waveform=0;waveform<5;++waveform) {
        ceres::Patch p;p.lfoWaveform=waveform;p.lfoRate=7;
        p.ampLevel=.8;p.tremolo=.4;p.filterLfo=.4;
        p.filterInitial=p.filterPeak=0;p.keyTrack=0;
        p.hpCutoff=100;p.lpCutoff=1200;
        ceres::Synth active(8000,91),clock(8000,91);
        active.setPatch(p);clock.setPatch(p);
        unsigned positive=0,negative=0;
        for(unsigned n=0;n<4000;++n) {
            if(n==0)active.noteOn(60);
            if(n==113)active.noteOn(64);
            if(n==277)active.noteOff(60);
            if(n==401)active.sustain(true);
            if(n==503)active.noteOff(64);
            if(n==607)active.sustain(false);
            if(n==809)active.allNotesOff();
            if(n==997)active.allSoundOff();
            if(n==1201)active.noteOn(67);
            active.process();clock.process();
            const double source=active.modulationState().sub;
            require(source==clock.modulationState().sub,"Notes, pedal and panic cannot reset the shared SUB source");
            const double gain=active.soundPathState().commonVcaGain;
            const auto v=active.voiceState()[0];
            const double hpOct=std::log2((v.hpFrequency-floorHz)/(p.hpCutoff-floorHz));
            const double lpOct=std::log2((v.lpFrequency-floorHz)/(p.lpCutoff-floorHz));
            // The .4 lever delivers .4/(1+.4*.6/3) of SUO into its 30k load.
            const double wiper=.4/(1+.4*.6*(10e3/30e3));
            near(gain,.8+wiper*(1.5/120e3)/(10/270e3)*source,2e-16,"Common gain swings around LEVEL by the loaded R7 120k/270k ratio");
            near(lpOct,.4*source,2e-14,"Selected SUB feeds actual LP variable-gm control");
            near(hpOct,.4*source*(47./150.),2e-14,"Selected SUB feeds actual HP variable-gm control");
            if(source>.05) {
                ++positive;require(gain>.8 && hpOct>0 && lpOct>0,"Positive shared SUB raises gain and both cutoffs");
            }
            if(source<-.05) {
                ++negative;require(gain<.8 && hpOct<0 && lpOct<0,"Negative shared SUB lowers gain and both cutoffs");
            }
        }
        require(positive>100 && negative>100,"Both SUB polarities were exercised for every waveform");
        active.reset();ceres::Synth fresh(8000,91);fresh.setPatch(p);
        active.noteOn(60);fresh.noteOn(60);
        for(unsigned n=0;n<120;++n) {
            require(active.process()==fresh.process(),"Explicit reset reproduces fresh circuit/audio state");
            require(active.modulationState().sub==fresh.modulationState().sub,"Explicit reset initializes every SUB waveform deterministically");
        }
    }
}
// The expression pedal is the PRA photo-coupler network, not a gain. A fully
// open pedal leaves the signal path untouched bit for bit; a closed one
// follows the guide's printed level curve at midband and, because the leg
// that then carries the signal is a 68k-2.2k/0.33uF divider into 0.0033uF,
// lifts the bottom and rolls off the top around it.
void expressionNetwork() {
    constexpr double fs=48000;
    {   // Open: every sample identical to a patch without the field.
        auto bare=ceres::preset("flute"); auto open=bare; open.expression=1;
        ceres::Synth a(fs,7),b(fs,7);
        require(a.configure(4,4)&&b.configure(4,4),"Configure expression witnesses");
        a.setPatch(bare); b.setPatch(open); a.noteOn(60); b.noteOn(60);
        for(unsigned n=0;n<4096;++n)
            require(a.process()==b.process(),"A fully open pedal is a bare wire");
    }
    // Closed: the network's own transfer function, built here from the
    // printed component values with the cell solved from the printed curve.
    const double arm=68e3,shunt=2.2e3,shuntCap=.33e-6,outCap=.0033e-6,load=10e3+330;
    const double g=1/arm,gs=1/shunt,gl=1/load;
    const auto response=[&](double hz,double decibels) {
        const double pass=g*g/(2*g+gs);
        const double gain=std::pow(10,decibels/20);
        const double cell=(gain*(g-pass+gl)-pass)/(1-gain);
        const std::complex<double> s(0,2*pi*hz);
        const std::complex<double> ys=s*shuntCap/(1.0+s*shunt*shuntCap),d=2*g+ys;
        return std::abs((cell+g*g/d)/(cell+g+gl+s*outCap-g*g/d));
    };
    // Travel .3, .5 and 1 of the graph's 28 degrees, and its levels there.
    for(auto band:{std::make_pair(.7,-7.2),std::make_pair(.5,-16.5),std::make_pair(0.,-39.8)}) {
        std::array<double,3> magnitude{};
        for(unsigned i=0;i<3;++i) {
            const int note=36+24*int(i);
            const double hz=440*std::exp2((note-69)/12.);
            ceres::Synth s(fs,7); require(s.configure(4,4),"Configure closed witness");
            auto p=ceres::preset("flute"); p.expression=band.first; s.setPatch(p); s.noteOn(note);
            std::complex<double> sum=0; double n=0;
            for(unsigned k=0;k<unsigned(fs);++k) {
                const double y=s.process();
                if(k>=unsigned(fs*.3)) {sum+=y*std::exp(std::complex<double>(0,-2*pi*hz*k/fs)); n+=1;}
            }
            // Against the open pedal at the same pitch, so only the network
            // is measured; the output preamp is linear at this level.
            ceres::Synth o(fs,7); require(o.configure(4,4),"Configure open witness");
            auto q=ceres::preset("flute"); o.setPatch(q); o.noteOn(note);
            std::complex<double> reference=0;
            for(unsigned k=0;k<unsigned(fs);++k) {
                const double y=o.process();
                if(k>=unsigned(fs*.3)) reference+=y*std::exp(std::complex<double>(0,-2*pi*hz*k/fs));
            }
            magnitude[i]=std::abs(sum)/std::abs(reference);
            near(magnitude[i],response(hz,band.second),response(hz,band.second)*.01,
                 "A closed pedal follows the printed curve through the network");
        }
        if(band.second<-30)
            require(magnitude[0]/magnitude[2]>1.6,"A closed pedal leaves 65Hz well above 1kHz");
    }
}

void postCircuitOutputTrim() {
    for(double db:{-24.,-6.020599913279624,0.,6.020599913279624,24.}) {
        ceres::Patch p;p.sine=.3;p.drive=2;p.ringMix=.73;p.ringRate=53;
        p.brilliance=-8;p.resonanceOffset=-3;p.filterLfo=1;
        ceres::Synth base(8000,88),trimmed(8000,88);base.setPatch(p);
        p.outputTrimDb=db;trimmed.setPatch(p);base.noteOn(60);trimmed.noteOn(60);
        const double gain=std::pow(10,db/20);
        double energy=0;
        for(unsigned n=0;n<=2000;++n) {
            if(n==1000){base.noteOff(60);trimmed.noteOff(60);}
            const double x=base.process(),y=trimmed.process();energy+=x*x;
            near(y,x*gain,2e-7*std::max(1e-20,std::abs(x*gain)),"Trim is linear after every circuit element and FIR");
            if(db==0)require(x==y,"Neutral trim preserves exact samples");
            const auto a=base.voiceState()[0],b=trimmed.voiceState()[0];
            require(a.oscillatorPhase==b.oscillatorPhase && a.filterEnvelope==b.filterEnvelope &&
                a.hpFrequency==b.hpFrequency && a.lpFrequency==b.lpFrequency,
                "Utility trim cannot change internal circuit state");
        }
        require(energy>1e-6,"Trim witness has nonzero audio");
        p.outputTrimDb=0;trimmed.setPatch(p);
        // The switch occurs after sample2000, so all future samples
        // must immediately match, with no gain smoothing or added latency.
        for(unsigned n=0;n<400;++n)require(base.process()==trimmed.process(),"Immediate post-FIR neutral trim has no delayed history");
    }
}
}
int main() {
    try {
        controlRanges();std::cout<<"PASS new control ranges/neutral/invalid-input contract\n";
        brillianceAndSubKcl();std::cout<<"PASS BR/SUB nodal ratios, shared key modulation, fixed-gm floor and audio equivalence\n";
        resonanceControlCircuit();std::cout<<"PASS eight-node Q capacitor KCL, exact RC dynamics, lifecycle and audio routing\n";
        commonVcaPlacement();std::cout<<"PASS additive common VCA, sine path, downstream bus/ring state and factory LEVEL\n";
        sharedSubPolarity();std::cout<<"PASS all five shared SUB polarities, continuous clock, lifecycle and reset\n";
        postCircuitOutputTrim();std::cout<<"PASS post-FIR utility trim linearity, neutral bypass and state/latency independence\n";
        expressionNetwork();std::cout<<"PASS expression photo-coupler bypass, printed level curve and its tone shaping\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
constexpr double vt=1.380649e-23*298.15/1.602176634e-19;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why){
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance){
        std::cerr<<why<<": "<<a<<" versus "<<b<<'\n';throw std::runtime_error(why);
    }
}
// Ratio of two BJT collector currents with matched Is and a common emitter.
// This uses exponential currents, not the production tanh identity.
double currentRatio(double volts){
    const double first=std::exp(volts/(2*vt)),second=std::exp(-volts/(2*vt));
    return (first-second)/(first+second);
}
double independentBias(bool baseCurrents=false){
    // Full DC nodal solve of the PRA bias network around the uA796. Nodes:
    // 0 rail S (390 from +15), 1 carrier-bias b (pin 8), 2 audio-bias a,
    // 3 pin 1, 4 pin 4, 5 pin 7 (100 ohm to b; its 33k is capacitor-coupled).
    // The IC is the uA796 sheet's model: I5=(V5-.75)/500 with pin 5 fed from
    // S through 10k, i.e. a 10.5k branch to .75V; the quad's 2*I5 collector
    // current leaves S through the two 2.7k loads. Optionally the base
    // currents I5/beta (each signal base, and pins 7 and 8) are drawn too.
    const double beta=((8-.75)/7300.)/12e-6;
    double i5=1e-3;
    std::array<double,6> x{};
    for(unsigned pass=0;pass<60;++pass){
        std::array<std::array<double,6>,6> a{};std::array<double,6> b{};
        auto ground=[&](unsigned i,double r){a[i][i]+=1/r;};
        auto resistor=[&](unsigned i,unsigned j,double r){
            a[i][i]+=1/r;a[j][j]+=1/r;a[i][j]-=1/r;a[j][i]-=1/r;
        };
        ground(0,390);b[0]+=15/390.;ground(0,10500);b[0]+=.75/10500.;b[0]-=2*i5;
        resistor(0,1,1200);resistor(1,2,820);ground(2,1000);
        resistor(2,3,470);resistor(2,4,470);ground(3,125000);ground(4,125000);
        resistor(1,5,100);
        if(baseCurrents){b[3]-=i5/beta;b[4]-=i5/beta;b[5]-=i5/beta;b[1]-=i5/beta;}
        const auto original=a;const auto originalB=b;
        for(unsigned col=0;col<6;++col){
            unsigned pivot=col;for(unsigned i=col+1;i<6;++i)if(std::abs(a[i][col])>std::abs(a[pivot][col]))pivot=i;
            std::swap(a[col],a[pivot]);std::swap(b[col],b[pivot]);
            for(unsigned row=col+1;row<6;++row){
                const double ratio=a[row][col]/a[col][col];
                for(unsigned j=col;j<6;++j)a[row][j]-=ratio*a[col][j];b[row]-=ratio*b[col];
            }
        }
        for(int row=5;row>=0;--row){double sum=b[row];for(unsigned j=row+1;j<6;++j)sum-=a[row][j]*x[j];x[row]=sum/a[row][row];}
        for(unsigned row=0;row<6;++row){double error=-originalB[row];for(unsigned j=0;j<6;++j)error+=original[row][j]*x[j];near(error,0,1e-15,"Full DC nodal KCL");}
        i5=(x[0]-.75)/10500.;
    }
    near(x[0],12.151998210544821,baseCurrents?.02:1e-12,"Local rail S");
    if(!baseCurrents){
        // Without base currents pin 7 sits exactly on pin 8; the offset is the
        // pin-7 base current through the 100 ohm, applied explicitly.
        near(x[5]-x[1],0,1e-15,"No DC through the capacitor-coupled 33k");
        return -100*i5/beta;
    }
    return x[5]-x[1];
}
void physicalCurrentLaw(){
    for(double volts:{-1.5,-.5,-.1,-2*vt,-vt,-.01,-.001,0.,.001,.01,vt,2*vt,.1,.5,1.5})
        near(ceres::ringSwitchingResponse(volts),currentRatio(volts),3e-16,"Independent matched BJT current ratio in physical volts");
    near(ceres::ringSwitchingResponse(0),0,0,"Balanced pair zero differential");
    const double epsilon=1e-7;
    near((ceres::ringSwitchingResponse(epsilon)-ceres::ringSwitchingResponse(-epsilon))/(2*epsilon),1/(2*vt),3e-11,"Small-signal slope has physical factor2VT");
    near(ceres::ringSwitchingResponse(1e300),1,0,"Finite large positive limit");
    near(ceres::ringSwitchingResponse(-1e300),-1,0,"Finite large negative limit");
    for(double value:{std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        bool threw=false;try{ceres::ringSwitchingResponse(value);}catch(const std::invalid_argument&){threw=true;}
        require(threw,"Nonfinite physical differential rejected by offline probe");
    }
}
void productionConnectionAndLifecycle(){
    const double dc=independentBias();
    near(dc,-.0013120723063956995,1e-15,"Typical-part carrier-port offset, -1.31mV");
    // Drawing every base current as well moves the offset by under 2%.
    near(independentBias(true)/dc,1,.02,"Neglected base currents are second order");
    ceres::Patch p;p.ringDepth=0;p.ringRate=.2;
    ceres::Synth dry(8000,19),wet(8000,19);dry.setPatch(p);p.ringMix=1;wet.setPatch(p);
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Constructor includes physical nominal DC feedthrough");
    near(dry.modulationState().ringSwitchGain,-.025528523672276603,3e-14,"No hidden zero-subtraction");
    for(unsigned i=0;i<13000;++i){
        if(i==11)wet.noteOn(64);
        if(i==191)wet.noteOff(64);
        if(i==911){p.ringRate=220;dry.setPatch(p);wet.setPatch(p);}
        if(i==1317){p.ringRate=6.669;p.ringMix=0;dry.setPatch(p);p.ringMix=1;wet.setPatch(p);}
        require(std::isfinite(dry.process())&&std::isfinite(wet.process()),"Finite production audio");
        const auto a=dry.modulationState(),b=wet.modulationState();
        near(a.ringSwitchGain,currentRatio(dc+1.6*a.ringCoupledCarrier),4e-14,"Actual production wiring uses the 3.20Vpp sine after passive coupling");
        near(a.ringSwitchGain,b.ringSwitchGain,0,"Notes and mix do not reset or suspend steering history");
    }
    const auto before=dry.modulationState();dry.allSoundOff();
    near(dry.modulationState().ringCarrier,before.ringCarrier,0,"Software panic preserves free carrier phase");
    near(dry.modulationState().ringCoupledCarrier,0,0,"Panic clears existing passive state convention");
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Panic returns steering to nominal DC not artificialzero");
    dry.process();const auto after=dry.modulationState();
    near(after.ringSwitchGain,currentRatio(dc+1.6*after.ringCoupledCarrier),4e-14,"Post-panic state uses same physical law");
    dry.reset();near(dry.modulationState().ringCarrier,0,0,"Explicit reset resets freeclock");
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Reset initializes steering to nominal DC");
}
void slowCarrierBoundary(){
    // With the 33k in series (not to ground) even the slowest .2Hz carrier
    // swings pins 7-8 by +/-4.7mV against a 1.31mV offset, so the steering
    // reverses every cycle and the product is a true four-quadrant one. Its
    // mean is the offset's feedthrough, small and negative, not -.43.
    ceres::Patch p;p.ringRate=.2;p.ringDepth=0;ceres::Synth s(8000,7);s.setPatch(p);
    double lo=1,hi=-1,mean=0;
    for(unsigned i=0;i<80000;++i){s.process();if(i>=40000){const double f=s.modulationState().ringSwitchGain;lo=std::min(lo,f);hi=std::max(hi,f);mean+=f/40000.;}}
    require(lo<0&&hi>0,"Slow carrier reverses the steering");
    require(hi<.1&&lo>-.12,"Linear region: steering stays near +/-.09, never switches");
    require(mean<0&&mean>-.05,"Offset feedthrough is small and keeps its sign");
    std::cout<<"slow0.2Hz steering min="<<lo<<" max="<<hi<<" mean="<<mean<<'\n';
}
}
int main(){try{
    physicalCurrentLaw();std::cout<<"PASS physical BJT current law,units,factor2VT,limits\n";
    productionConnectionAndLifecycle();std::cout<<"PASS independent DC operating point,production wiring/lifecycle/no DC removal\n";
    slowCarrierBoundary();std::cout<<"PASS slow carrier crosses zero in the linear region\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

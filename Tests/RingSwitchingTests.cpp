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
double independentBias(){
    // Full six-node DC KCL including the two 470+125k returns. Node order is
    // carrier7, carrier8, audio-bias, local-supply, audio1, audio4. All IC
    // currents are omitted, exactly the declared uncalibrated loading limit.
    std::array<std::array<double,6>,6> a{};std::array<double,6> b{};
    auto ground=[&](unsigned i,double r){a[i][i]+=1/r;};
    auto resistor=[&](unsigned i,unsigned j,double r){
        a[i][i]+=1/r;a[j][j]+=1/r;a[i][j]-=1/r;a[j][i]-=1/r;
    };
    ground(0,33000);resistor(0,1,100);resistor(1,2,820);resistor(1,3,1200);
    ground(2,1000);ground(3,390);b[3]=15/390.;
    resistor(2,4,470);resistor(2,5,470);ground(4,125000);ground(5,125000);
    const auto original=a;const auto originalB=b;
    for(unsigned col=0;col<6;++col){
        unsigned pivot=col;for(unsigned i=col+1;i<6;++i)if(std::abs(a[i][col])>std::abs(a[pivot][col]))pivot=i;
        std::swap(a[col],a[pivot]);std::swap(b[col],b[pivot]);
        for(unsigned row=col+1;row<6;++row){
            const double ratio=a[row][col]/a[col][col];
            for(unsigned j=col;j<6;++j)a[row][j]-=ratio*a[col][j];b[row]-=ratio*b[col];
        }
    }
    std::array<double,6> x{};
    for(int row=5;row>=0;--row){double sum=b[row];for(unsigned j=row+1;j<6;++j)sum-=a[row][j]*x[j];x[row]=sum/a[row][row];}
    for(unsigned row=0;row<6;++row){double error=-originalB[row];for(unsigned j=0;j<6;++j)error+=original[row][j]*x[j];near(error,0,3e-17,"Full DC nodal KCL");}
    const double dc=x[0]-x[1];near(dc,-.023489437180096218,2e-14,"Source-only nominal DC solution");return dc;
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
    const double dc=independentBias();ceres::Patch p;p.ringDepth=0;p.ringRate=.2;
    ceres::Synth dry(8000,19),wet(8000,19);dry.setPatch(p);p.ringMix=1;wet.setPatch(p);
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Constructor includes physical nominal DC feedthrough");
    near(dry.modulationState().ringSwitchGain,-.4277380604918782,3e-14,"No hidden zero-subtraction");
    for(unsigned i=0;i<13000;++i){
        if(i==11)wet.noteOn(64);
        if(i==191)wet.noteOff(64);
        if(i==911){p.ringRate=220;dry.setPatch(p);wet.setPatch(p);}
        if(i==1317){p.ringRate=6.669;p.ringMix=0;dry.setPatch(p);p.ringMix=1;wet.setPatch(p);}
        require(std::isfinite(dry.process())&&std::isfinite(wet.process()),"Finite production audio");
        const auto a=dry.modulationState(),b=wet.modulationState();
        near(a.ringSwitchGain,currentRatio(dc+1.5*a.ringCoupledCarrier),4e-14,"Actual production wiring uses3Vpp after passive coupling");
        near(a.ringSwitchGain,b.ringSwitchGain,0,"Notes and mix do not reset or suspend steering history");
    }
    const auto before=dry.modulationState();dry.allSoundOff();
    near(dry.modulationState().ringCarrier,before.ringCarrier,0,"Software panic preserves free carrier phase");
    near(dry.modulationState().ringCoupledCarrier,0,0,"Panic clears existing passive state convention");
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Panic returns steering to nominal DC not artificialzero");
    dry.process();const auto after=dry.modulationState();
    near(after.ringSwitchGain,currentRatio(dc+1.5*after.ringCoupledCarrier),4e-14,"Post-panic state uses same physical law");
    dry.reset();near(dry.modulationState().ringCarrier,0,0,"Explicit reset resets freeclock");
    near(dry.modulationState().ringSwitchGain,currentRatio(dc),3e-14,"Reset initializes steering to nominal DC");
}
void slowCarrierBoundary(){
    // An intentionally diagnostic consequence of THIS fixed zero-load DC
    // model: at0.2Hz its passive AC swing is less than the nominal bias, so
    // the upper pair never reverses. This is not measured hardware leakage.
    ceres::Patch p;p.ringRate=.2;p.ringDepth=0;ceres::Synth s(8000,7);s.setPatch(p);
    double lo=1,hi=-1,mean=0;
    for(unsigned i=0;i<80000;++i){s.process();if(i>=40000){const double f=s.modulationState().ringSwitchGain;lo=std::min(lo,f);hi=std::max(hi,f);mean+=f/40000.;}}
    require(lo<hi&&hi<0,"Slow nominal carrier keeps signed DC steering without gain normalization");
    require(mean<-.3,"Slow nominal carrier preserves nonzero mean feedthrough");
    std::cout<<"slow0.2Hz steering min="<<lo<<" max="<<hi<<" mean="<<mean<<'\n';
}
}
int main(){try{
    physicalCurrentLaw();std::cout<<"PASS physical BJT current law,units,factor2VT,limits\n";
    productionConnectionAndLifecycle();std::cout<<"PASS independent6node DC,production wiring/lifecycle/no DC removal\n";
    slowCarrierBoundary();std::cout<<"PASS nominal slow-rate no-crossing boundary\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

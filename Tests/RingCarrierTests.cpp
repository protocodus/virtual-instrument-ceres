#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace {
constexpr double pi=3.141592653589793238462643383279502884;
using Complex=std::complex<double>;
template<class T>using Matrix=std::array<std::array<T,4>,4>;
template<class T>using Vector=std::array<T,4>;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void near(double a,double b,double tolerance,const char* why){
    if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>tolerance){
        std::cerr<<why<<": "<<a<<" versus "<<b<<'\n';throw std::runtime_error(why);
    }
}
template<class T>Vector<T> solve(Matrix<T> a,Vector<T> b){
    // Independent elimination of the physical nodal equations; no reuse of
    // production's precomputed inverse or companion-state implementation.
    for(unsigned col=0;col<4;++col){
        unsigned pivot=col;for(unsigned i=col+1;i<4;++i)if(std::abs(a[i][col])>std::abs(a[pivot][col]))pivot=i;
        std::swap(a[col],a[pivot]);std::swap(b[col],b[pivot]);
        require(std::abs(a[col][col])>1e-20,"Nonsingular nodal system");
        for(unsigned row=col+1;row<4;++row){
            const T ratio=a[row][col]/a[col][col];
            for(unsigned j=col;j<4;++j)a[row][j]-=ratio*a[col][j];b[row]-=ratio*b[col];
        }
    }
    Vector<T> x{};for(int row=3;row>=0;--row){T sum=b[row];for(unsigned j=row+1;j<4;++j)sum-=a[row][j]*x[j];x[row]=sum/a[row][row];}
    return x;
}
// Pin7-pin8 small-signal shunt, rebuilt from the uA796 sheet: carrier-port
// bias current 12uA in its Fig.1 (I5=(8-.75)/7.3k) gives beta; the PRA's own
// I5 follows from the 390-fed rail (10.5k pin-5 branch, two 2.7k collector
// loads, 1.2k into 820+(1k||2*(470+125k))). Each upper pair is 2*rpi at
// Ic=I5/2; two pairs in parallel are rpi, across the 100 ohm.
double carrierShunt(){
    const double vt=1.380649e-23*298.15/1.602176634e-19;
    const double ra=1/(1/1000.+2/125470.),divider=1200+820+ra;
    // Rail S: (15-S)/390 = 3*(S-.75)/10500 + S/divider.
    const double s=(15/390.+3*.75/10500.)/(1/390.+3/10500.+1/divider);
    const double i5=(s-.75)/10500.,beta=((8-.75)/7300.)/12e-6;
    const double rpi=beta*vt/(i5/2);
    return 100*rpi/(100+rpi);
}
const double shunt=carrierShunt(),share=shunt/(33000+shunt);
Matrix<double> conductance(){
    // Explicit independent KCL rows m,b,a,s: m is the 100uF/33k junction and
    // the 33k runs on to pin 7, which meets pin 8 through the shunt above.
    const double g=1/(33000+shunt);
    return {{{g,-g,0,0},
        {-g,g+1/820.+1/1200.,-1/820.,-1/1200.},
        {0,-1/820.,1/820.+1/1000.,0},
        {0,-1/1200.,0,1/1200.+1/390.}}};
}
Complex response(double frequency,double fs){
    const auto g=conductance();Matrix<Complex> a{};Vector<Complex> b{};
    const Complex sc(0,2*fs*std::tan(pi*frequency/fs)*100e-6);
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)a[i][j]=g[i][j]+(i==j?sc:Complex(0));
    b[0]=sc;const auto v=solve(a,b);return (v[0]-v[1])*share;
}
void frequencyResponse(){
    // The coupling capacitor now works into 33k, a 3.3s pole: the impulse
    // needs 110s, not 8s, to settle below 1e-14, so the two probe rates are
    // 8k and 32k rather than 8k and 192k (memory, not accuracy).
    for(double fs:{8000.,32000.}){
        const std::size_t count=static_cast<std::size_t>(fs*110);
        std::vector<double> input(count,0);input[0]=1;
        const auto output=ceres::ringCarrierResponse(fs,input);
        require(std::all_of(output.begin(),output.end(),[](double v){return std::isfinite(v);}),"Finite impulse");
        near(output.back(),0,1e-14,"Impulse tail settled before Fourier comparison");
        for(double f:{.2,1.,6.6692178847,30.,220.}){
            Complex measured=0,z=1;const auto step=std::polar(1.,-2*pi*f/fs);
            for(double value:output){measured+=value*z;z*=step;}
            const auto expected=response(f,fs);
            near(std::abs(measured-expected)/share,0,2e-10,"Actual native complex response versus physical four-node KCL");
            near(std::arg(measured/expected),0,2e-8,"Actual native carrier phase");
            require(std::abs(measured)<1,"No frequency-dependent gain normalization");
            std::cout<<"response fs="<<fs<<" f="<<f<<" dB="<<20*std::log10(std::abs(measured))
                <<" phase="<<std::arg(measured)*180/pi<<'\n';
        }
    }
}
void independentTransient(){
    const auto g=conductance();
    for(double fs:{32000.,192000.}){
        std::vector<double> input(1200);
        for(unsigned i=0;i<input.size();++i)input[i]=i<211?1:(i<509?-.7:std::sin(i*.031));
        const auto actual=ceres::ringCarrierResponse(fs,input);
        Vector<double> previous{};double previousInput=0;
        for(unsigned n=0;n<input.size();++n){
            Matrix<double> a{};Vector<double> rhs{};
            // C/h*(q_n-q_prev)+G/2*(q_n+q_prev)=B/2*(u_n+u_prev)
            // q=(x-u,b,a,s), B=-G*e0; independently retain physical q and u.
            for(unsigned i=0;i<4;++i){
                rhs[i]=100e-6*fs*previous[i]-.5*g[i][0]*(input[n]+previousInput);
                for(unsigned j=0;j<4;++j){a[i][j]=.5*g[i][j]+(i==j?100e-6*fs:0);rhs[i]-=.5*g[i][j]*previous[j];}
            }
            const auto q=solve(a,rhs);
            near(actual[n]/share,input[n]+q[0]-q[1],5e-13,"Production companion versus simultaneous physical-state trapezoids");
            for(unsigned i=0;i<4;++i){
                double residual=100e-6*fs*(q[i]-previous[i])+.5*g[i][0]*(input[n]+previousInput);
                for(unsigned j=0;j<4;++j)residual+=.5*g[i][j]*(q[j]+previous[j]);
                near(residual,0,8e-15,"Independent transient KCL residual");
            }
            previous=q;previousInput=input[n];
        }
    }
}
void productionLifecycle(){
    constexpr double fs=8000,internal=4*fs;
    ceres::Patch p;p.ringMix=0;p.ringRate=6.669;p.ringDepth=0;
    ceres::Synth dry(fs,21),wet(fs,21);dry.setPatch(p);p.ringMix=1;wet.setPatch(p);
    double phase=0;
    for(unsigned segment=0;segment<2;++segment){
        std::vector<double> input(4*900);
        for(unsigned frame=0;frame<900;++frame)for(unsigned sub=0;sub<4;++sub){
            const double rate=frame<333?6.669:30;
            input[4*frame+sub]=std::sin(2*pi*phase);phase+=rate*(1/internal);phase-=std::floor(phase);
        }
        const auto expected=ceres::ringCarrierResponse(internal,input);
        for(unsigned frame=0;frame<900;++frame){
            if(frame==0){p.ringRate=6.669;p.ringMix=0;dry.setPatch(p);p.ringMix=1;wet.setPatch(p);}
            if(frame==7)wet.noteOn(60);
            if(frame==53)wet.noteOn(64);
            if(frame==131)wet.noteOff(60);
            if(frame==219)wet.noteOff(64);
            if(frame==333){p.ringRate=30;p.ringMix=0;dry.setPatch(p);p.ringMix=1;wet.setPatch(p);}
            dry.process();wet.process();const auto d=dry.modulationState(),w=wet.modulationState();
            near(d.ringCarrier,input[frame*4+3],0,"Raw oscillator arithmetic unchanged");
            near(w.ringCarrier,d.ringCarrier,0,"Notes and mix cannot reset carrier phase");
            near(d.ringCoupledCarrier,expected[frame*4+3],0,"Real engine uses exact probe network even at mix0");
            near(w.ringCoupledCarrier,d.ringCoupledCarrier,0,"Wet/dry carrier history identical across note/patch events");
        }
        const double raw=dry.modulationState().ringCarrier;
        dry.allSoundOff();wet.allSoundOff();
        near(dry.modulationState().ringCarrier,raw,0,"Panic keeps carrier phase convention");
        near(dry.modulationState().ringCoupledCarrier,0,0,"Explicit software panic clears coupling with wet path");
    }
    dry.reset();wet.reset();p.ringRate=6.669;p.ringMix=0;dry.setPatch(p);wet.setPatch(p);
    ceres::Synth fresh(fs,21);fresh.setPatch(p);
    for(unsigned n=0;n<100;++n){
        require(dry.process()==fresh.process(),"Reset restores deterministic dry audio");wet.process();
        near(dry.modulationState().ringCoupledCarrier,wet.modulationState().ringCoupledCarrier,0,"Reset reinitializes carrier network and phase");
    }
}
void dividerAcrossBand(){
    // Through the speed range the 100uF is all but a short against 33k (.46dB at .2Hz): the carrier
    // reaching pins 7-8 is the flat 97.5/33097.5 divider, not a high-pass.
    for(double f:{.2,1.,6.6692178847,30.,220.}){
        const double db=20*std::log10(std::abs(response(f,192000.))/share);
        require(db>-.5&&db<.001,"Carrier divider within .5dB (.46dB at .2Hz) across the speed range");
    }
    near(share*1.6,.0047138370,2e-9,"3.20Vpp source gives 4.71mV peak at pins 7-8");
    require(share*1.6<.025,"Inside the uA796 sheet's 25mV-peak linear carrier region");
}
void leverLaws(){
    // CS-60 PRA alignment (same board): 150ms period at VC1=5V, 5ms at 10V.
    near(ceres::ringSpeedHz(5),1/.150,1e-12,"SPEED aligned at 150ms period, VC1=5V");
    near(ceres::ringSpeedHz(10),1/.005,1e-10,"SPEED aligned at 5ms period, VC1=10V");
    near(ceres::ringSpeedHz(0),(1/.150)*(1/.150)/200,1e-12,"Exponential continues to .222Hz at 0V");
    near(ceres::ringSpeedOctavesPerVolt,std::log2(200*.150)/5,1e-15,"Speed octaves per volt");
    // VC2 through 180k against VC1's 100k into the same node; EO peak 10V.
    near(ceres::ringDepthOctavesPerLever,std::log2(30.)/5*100/180*10,1e-14,"Full DEPTH lever at EG peak");
    require(ceres::ringDepthOctavesPerLever>5.45&&ceres::ringDepthOctavesPerLever<5.46,"5.45 octaves");
    // ATTACK/DECAY TIME: CS-60 PRA alignment I, 31ms to the EO peak at
    // AT=DT=10V; the M-board alignment's printed points (4ms at 8V, 125ms at
    // 3V, CS-60 manual p.38) scaled by that 31x must hold to their 1%.
    near(ceres::ringAttackSeconds(10),.031,1e-15,"PRA attack aligned at 31ms, AT=10V");
    near(ceres::ringAttackSeconds(0),31,1e-12,"Three decades to 31s at 0V");
    for(const auto& point:{std::pair<double,double>{8,.004},{3,.125}})
        require(std::abs(ceres::ringAttackSeconds(point.first)/(31*point.second)-1)<.01,
                "M-board aligned attack points, 31 times slower");
    for(double v=0;v<=10;v+=.5)
        near(ceres::ringDecaySeconds(v)/ceres::ringAttackSeconds(v),10*std::log(1000.)/5,1e-13,
             "Decay is ten attack durations, as five-RC T60");
    near(ceres::ringDecaySeconds(10),.031*10*std::log(1000.)/5,1e-15,"Shortest decay .428s T60");
    near(ceres::ringReleaseVolts,15*3300./10100,1e-15,"2DT divider 4.901V");
    near(ceres::ringDecaySeconds(ceres::ringReleaseVolts),14.502123632169237,1e-12,"Fixed release 14.50s T60");
}
void invalidProbe(){
    for(double rate:{0.,std::numeric_limits<double>::quiet_NaN(),1600000.}){
        bool threw=false;try{ceres::ringCarrierResponse(rate,{0});}catch(const std::invalid_argument&){threw=true;}
        require(threw,"Invalid probe rate rejected");
    }
    bool threw=false;try{ceres::ringCarrierResponse(48000,{std::numeric_limits<double>::infinity()});}catch(const std::invalid_argument&){threw=true;}
    require(threw,"Nonfinite probe signal rejected");
}
}
int main(){try{
    frequencyResponse();std::cout<<"PASS complex carrier frequency response\n";
    independentTransient();std::cout<<"PASS independent physical-state trapezoids/KCL\n";
    productionLifecycle();std::cout<<"PASS native connection, mix bypass history, notes/live edits/panic/reset\n";
    dividerAcrossBand();std::cout<<"PASS flat 33k/100 carrier divider in the linear region\n";
    leverLaws();std::cout<<"PASS SPEED/DEPTH/ATTACK/DECAY lever laws and fixed release\n";
    invalidProbe();std::cout<<"PASS native diagnostic validation\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

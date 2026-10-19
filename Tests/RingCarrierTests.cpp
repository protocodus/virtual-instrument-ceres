#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <stdexcept>
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
Matrix<double> conductance(){
    // Explicit independent KCL rows x,b,a,s; six physical resistors.
    return {{{1/33000.+1/100.,-1/100.,0,0},
        {-1/100.,1/100.+1/820.+1/1200.,-1/820.,-1/1200.},
        {0,-1/820.,1/820.+1/1000.,0},
        {0,-1/1200.,0,1/1200.+1/390.}}};
}
Complex response(double frequency,double fs){
    const auto g=conductance();Matrix<Complex> a{};Vector<Complex> b{};
    const Complex sc(0,2*fs*std::tan(pi*frequency/fs)*100e-6);
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)a[i][j]=g[i][j]+(i==j?sc:Complex(0));
    b[0]=sc;const auto v=solve(a,b);return v[0]-v[1];
}
void frequencyResponse(){
    for(double fs:{8000.,192000.}){
        const std::size_t count=static_cast<std::size_t>(fs*8);
        std::vector<double> input(count,0);input[0]=1;
        const auto output=ceres::ringCarrierResponse(fs,input);
        require(std::all_of(output.begin(),output.end(),[](double v){return std::isfinite(v);}),"Finite impulse");
        near(output.back(),0,1e-14,"Impulse tail settled before Fourier comparison");
        for(double f:{.2,1.,6.6692178847,30.,220.}){
            Complex measured=0,z=1;const auto step=std::polar(1.,-2*pi*f/fs);
            for(double value:output){measured+=value*z;z*=step;}
            const auto expected=response(f,fs);
            near(std::abs(measured-expected),0,2e-10,"Actual native complex response versus physical four-node KCL");
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
            near(actual[n],input[n]+q[0]-q[1],5e-13,"Production companion versus simultaneous physical-state trapezoids");
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
    invalidProbe();std::cout<<"PASS native diagnostic validation\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

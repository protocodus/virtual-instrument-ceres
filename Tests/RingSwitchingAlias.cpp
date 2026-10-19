// Small offline witness. Samples the actual production steering kernel;
// no audio file or synth/recording optimization is involved.
#include "ceres.hpp"
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const long rate=std::strtol(argv[1],nullptr,10);if(rate!=192000&&rate!=3072000)return 3;
    constexpr double pi=3.141592653589793238462643383279502884,frequency=220.;
    // Same analog steady physical input at both sample rates: this isolates
    // sampling of the new nonlinear stage, not RC discretization differences.
    const std::complex<double> sc(0,2*pi*frequency*100e-6);
    const auto ya=1/1000.+sc,ys=1/390.+sc;
    const auto zb=1./(sc+ya/(1.+820.*ya)+ys/(1.+1200.*ys));
    const auto ac=sc/(sc+1/33000.+1./(100.+zb))*100./(100.+zb);
    constexpr double dc=-.023489437180096218;
    constexpr double vt=1.380649e-23*298.15/1.602176634e-19;
    for(long n=0;n<rate/20;++n){
        const double volts=dc+1.5*std::abs(ac)*std::sin(2*pi*frequency*(double(n)/rate)+std::arg(ac));
        const double forward=std::exp(volts/(2*vt)),reverse=std::exp(-volts/(2*vt));
        const double values[]={ceres::ringSwitchingResponse(volts),(forward-reverse)/(forward+reverse)};
        if(std::fwrite(values,sizeof(double),2,stdout)!=2)return 4;
    }
    return 0;
}

#include "ceres.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

// A fixed host rate is intentional: retain legacy4x white-noise PSD, not
// infer the hardware source voltage or promise cross-host-rate calibration.
// Independent radix2 DFT/Parseval band powers on long, stationary, low-drive
// full-engine output catch a source-variance change without measuring peaks.
namespace {
constexpr double pi=3.14159265358979323846;
constexpr unsigned fftSize=4096, count=262144;
void require(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void fft(std::array<std::complex<double>,fftSize>& a){
    for(unsigned i=1,j=0;i<fftSize;++i){
        unsigned bit=fftSize>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;if(i<j)std::swap(a[i],a[j]);
    }
    for(unsigned size=2;size<=fftSize;size<<=1){
        const auto step=std::polar(1.0,-2*pi/size);
        for(unsigned start=0;start<fftSize;start+=size){
            std::complex<double> w=1;
            for(unsigned j=0;j<size/2;++j){
                const auto even=a[start+j],odd=w*a[start+j+size/2];
                a[start+j]=even+odd;a[start+j+size/2]=even-odd;w*=step;
            }
        }
    }
}
std::array<double,3> power(const std::vector<double>& samples,double fs){
    std::array<double,fftSize> window{};double sumSquares=0;
    for(unsigned i=0;i<fftSize;++i){window[i]=.5-.5*std::cos(2*pi*i/(fftSize-1));sumSquares+=window[i]*window[i];}
    std::array<double,3> result{};unsigned frames=0;
    for(unsigned start=0;start+fftSize<=samples.size();start+=fftSize/2){
        std::array<std::complex<double>,fftSize> bins{};
        for(unsigned i=0;i<fftSize;++i)bins[i]=samples[start+i]*window[i];
        fft(bins);++frames;
        for(unsigned k=1;k<fftSize/2;++k){
            const double f=k*fs/fftSize;
            const double p=2*std::norm(bins[k])/(fftSize*sumSquares);
            if(f>=200&&f<800)result[0]+=p;
            if(f>=800&&f<2000)result[1]+=p;
            if(f>=2000&&f<4000)result[2]+=p;
        }
    }
    for(auto& p:result)p/=frames;
    return result;
}
void estimator(){
    std::vector<double> tone(count);
    // An integer-bin sine: 750Hz lies in the first band at48k.
    for(unsigned i=0;i<count;++i)tone[i]=.2*std::sin(2*pi*750*i/48000);
    const auto p=power(tone,48000);
    require(std::abs(p[0]-.02)<1e-8,"independent spectral estimator Parseval normalization");
    require(p[1]+p[2]<1e-10,"estimator band separation");
}
}
int main(){
    estimator();
    for(double fs:{48000.,96000.}){
        std::array<std::array<double,3>,3> levels{};
        for(unsigned index=0;index<3;++index){
            const unsigned factor=1u<<index;
            ceres::Synth s(fs,50);require(s.configure(1,factor),"configure");
            ceres::Patch p;p.saw=0;p.pulse=0;p.sine=0;p.noise=.5;
            p.drive=.05;p.volume=.1;p.hpCutoff=60;p.lpCutoff=6000;
            p.hpEnvelopeScale=0;p.lpEnvelopeScale=0;p.keyTrack=0;
            p.ampAttack=.001;p.ampSustain=1;s.setPatch(p);s.noteOn(60,1);
            for(unsigned i=0;i<static_cast<unsigned>(fs);++i)require(std::isfinite(s.process()),"warmup finite");
            std::vector<double> output(count);double energy=0,peak=0;
            for(auto& y:output){y=s.process();require(std::isfinite(y),"noise output finite");energy+=y*y;peak=std::max(peak,std::abs(y));}
            require(energy>1e-8&&peak<.1,"stationary audible noise, safely low drive");
            levels[index]=power(output,fs);
            for(unsigned band=0;band<3;++band){
                require(levels[index][band]>0&&std::isfinite(levels[index][band]),"finite positive band power");
                std::printf("NOISE_POWER,%.0f,%u,%u,%.17g\n",fs,factor,band,levels[index][band]);
            }
        }
        // White variance1/3 at4fs means two-sided density1/(12fs).
        // At lower factors variance must be factor/12 for the same density.
        // Different sequences/filter discretizations are not byte-identical;
        // .6dB allows finite-sample scatter and small-signal response error,
        // while decisively rejecting the original +3/+6dB variance defect.
        for(unsigned index=0;index<2;++index)for(unsigned band=0;band<3;++band){
            const double db=10*std::log10(levels[index][band]/levels[2][band]);
            std::printf("NOISE_DELTA_DB,%.0f,%u,%u,%.17g\n",fs,1u<<index,band,db);
            require(std::isfinite(db)&&std::abs(db)<.6,"quality preserves low-band audio-noise density within .6dB");
        }
    }
    std::puts("PASS noise quality: full-engine stationary band powers at48/96k, independent FFT normalization");
}

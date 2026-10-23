// Direct double/state parity for filter CPU optimizations. Compile this SAME
// driver against each frozen core, with identical compiler and flags:
// c++ -std=c++17 -O3 -DNDEBUG -I <core-directory> \
//     Tests/FilterCpuParityDriver.cpp -o <driver>
// Including the implementation is intentional: compare all electrical state,
// not only rounded float audio, without exposing test-only production APIs.
#include "ceres.cpp"
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
struct Output {
    std::FILE* file;
    std::uint64_t frames=0;
    explicit Output(const char* name): file(std::fopen(name,"wb")) {
        require(file!=nullptr,"cannot open filter parity output");
        static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,
                      "filter parity requires IEEE float64");
    }
    void write(double value,const ceres::OtaSvf& f) {
        const double frame[]={value,f.sb,f.sl,f.sz,f.sy,f.previousB};
        for (double x:frame) require(std::isfinite(x),"nonfinite filter output/state");
        require(std::fwrite(frame,sizeof(double),6,file)==6,"cannot write filter state");
        ++frames;
    }
    void finish() {
        require(std::fclose(file)==0,"cannot close filter parity output");
        std::fprintf(stderr,"filter parity: %llu frames, six doubles per frame\n",
                     static_cast<unsigned long long>(frames));
    }
};
void stationary(Output& out) {
    for (double rate:{8000.,44100.,48000.,96000.,192000.,1536000.})
    for (double source:{1e-12,1/150e3,4/150e3,.1})
    for (bool high:{false,true})
    for (double cutoff:{1.,ceres::filterFrequencyFloor,
                       std::nextafter(ceres::filterFrequencyFloor,100.),
                       2000.,.22*rate,rate})
    for (double resonance:{0.,.5,1.}) {
        ceres::OtaSvf filter;
        ceres::OtaNetwork network;network.configure(rate,source,3/150e3);
        network.setResonance(resonance);
        for (unsigned i=0;i<128;++i) {
            // Zero initialization, positive and negative impulses, then decay.
            const double input=i==8?1.:i==32?-8.:i==64?64.:0.;
            out.write(filter.process(input,cutoff,high,network),filter);
        }
    }
}
void moving(Output& out) {
    for (double rate:{8000.,44100.,48000.,96000.,192000.,1536000.})
    for (double source:{1e-12,1/150e3,4/150e3,.1})
    for (bool high:{false,true}) {
        ceres::OtaSvf filter;
        ceres::OtaNetwork network;network.configure(rate,source,3/150e3);
        for (unsigned i=0;i<2048;++i) {
            if (i==512 || i==1536) filter={};
            // Change the rate and mixer coefficients while charge is retained.
            if (i==768) network.configure(rate*2,source*2,1/150e3);
            if (i==1280) network.configure(rate,source,3/150e3);
            network.setResonance((i%127)/126.);
            const double cutoff=i%256<16?1.:i%256<32?network.fs:
                25+(.22*network.fs-25)*(i%251)/250.;
            const double input=i<32?0.:(i%67==0?-64.:8*std::sin(i*.173));
            out.write(filter.process(input,cutoff,high,network),filter);
        }
    }
}
}
int main(int argc,char** argv) {
    require(argc==2,"Usage: filter-parity output.f64");
    Output out(argv[1]);stationary(out);moving(out);out.finish();
}

// Actual CCeres callback, driven by the SAME test MOM shim for both builds.
// Including the existing wrapper suite also keeps its mock/ABI assumptions in
// sync. Its original main is retained but not called by this rendering driver.
#define main ceresUnusedWrapperContractMain
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#include "test_wrapper.cpp"
#pragma clang diagnostic pop
#undef main
#include <fstream>
#include <string>
#include <vector>

namespace {
using StoredPatch = std::array<double, parameterCount>;
struct BankPatch { std::string name; StoredPatch values; };
struct Change { unsigned frame, index; double value; bool isNote = false; };

class Stream {
public:
    Stream(const char* audio, const char* scenes) : audio_(audio, std::ios::binary), scenes_(scenes) {
        require(bool(audio_) && bool(scenes_), "open parity outputs");
    }
    void begin(const std::string& name) { name_ = name; start_ = samples_; peak_ = 0; stereo_ = false; }
    void block() {
        for (unsigned i = 0; i < 64; ++i) {
            require(std::isfinite(left[i]) && std::isfinite(right[i]), "finite parity stereo");
            const float pair[] = {left[i], right[i]};
            audio_.write(reinterpret_cast<const char*>(pair), sizeof(pair));
            samples_ += 2;
            peak_ = std::max(peak_, std::max(std::abs(double(left[i])), std::abs(double(right[i]))));
            stereo_ = stereo_ || left[i] != right[i];
        }
    }
    void end(bool audible = true) {
        require(!audible || peak_ > 1e-8, "scene activation witness");
        scenes_ << name_ << '\t' << start_ << '\t' << samples_ - start_
                << '\t' << peak_ << '\t' << stereo_ << '\n';
    }
    void finish() { audio_.close(); scenes_.close(); require(bool(audio_) && bool(scenes_), "complete parity outputs"); }
private:
    std::ofstream audio_, scenes_;
    std::string name_;
    std::uint64_t samples_ = 0, start_ = 0;
    double peak_ = 0;
    bool stereo_ = false;
};

void load(const StoredPatch& patch, unsigned quality) {
    defaults();
    std::copy(patch.begin(), patch.end(), mom.begin());
    mom[ceres_re::idx::oversampling] = quality;
    mom[expressionPedalIndex] = 1;
}

void setPhysical(const char* key, double value) {
    const unsigned index = parameterIndex(key);
    mom[index] = storedFor(index, value);
}

void run(Stream& output, CCeres& device, unsigned frames, double rate,
         std::vector<Change> changes = {}, unsigned resetAt = ~0u) {
    std::stable_sort(changes.begin(), changes.end(), [](const Change& a, const Change& b) { return a.frame < b.frame; });
    std::size_t next = 0;
    for (unsigned frame = 0; frame < frames; frame += 64) {
        if (frame == resetAt) ++resetCounter;
        std::vector<TJBox_PropertyDiff> diffs;
        while (next < changes.size() && changes[next].frame < frame + 64) {
            const auto& c = changes[next++];
            if (c.isNote) diffs.push_back(note(c.index, c.value, c.frame - frame));
            else {
                diffs.push_back(control(c.index, mom[c.index], c.value, c.frame - frame));
                mom[c.index] = c.value;
            }
        }
        for (unsigned i = 0; i < 64; ++i)
            extSignal[i] = float(.7 * std::sin((frame + i) * 2 * 3.14159265358979323846 * 137 / rate));
        device.RenderBatch(diffs.data(), unsigned(diffs.size()));
        output.block();
    }
    require(next == changes.size(), "all parity events consumed");
}

std::vector<Change> musicalEvents() {
    using namespace ceres_re::idx;
    return {
        // E2-G2-H2 (scientific E2-G2-B2): same-frame and split-frame attacks.
        {17, 40, 100, true}, {31, 43, 100, true}, {31, 47, 100, true},
        {1025, 40, 65, true}, {1307, 40, 0, true}, {1800, 52, 127, true},
        {2049, output_mode, 1}, {2301, double_detune, 1},
        {2607, double_pan, .31}, {3071, output_mode, 0},
        {3203, output_mode, 1}, // reverse the 20 ms ramp before it completes
        {4355, pedalIndex, 1}, {4401, 43, 0, true}, {4420, 47, 0, true},
        {4445, 52, 0, true}, {4607, bendIndex, .73}, {4703, pressureIndex, .8},
        {5001, pedalIndex, 0}, {5607, output_mode, 0},
        {6203, 64, 83, true}, {7001, 64, 0, true},
    };
}

void bankScenes(Stream& output, const std::vector<BankPatch>& bank) {
    for (const auto& p : bank) for (unsigned quality = 0; quality < 3; ++quality) {
        load(p.values, quality);
        CCeres device(48000);
        output.begin("bank/" + p.name + "/q" + std::to_string(1u << quality));
        run(output, device, 16384, 48000, musicalEvents());
        output.end();
    }
}

void controlScenes(Stream& output, const StoredPatch& init) {
    for (unsigned index = 0; index < parameterCount; ++index) {
        load(init, 0);
        // Keep every dependency represented: oscillator/noise, PWM, ring,
        // SUB, pressure, pedal, drift, portamento, and independent Double.
        setPhysical("saw", .7); setPhysical("pulse", .4); setPhysical("sine", .3);
        setPhysical("noise", .03); setPhysical("amp_attack", .001);
        setPhysical("amp_sustain", .9); setPhysical("amp_release", .01);
        setPhysical("pwm_depth", .2); setPhysical("ring_mix", .4);
        setPhysical("ring_depth", 2); setPhysical("filter_lfo", .3);
        setPhysical("tremolo", .2); setPhysical("vibrato_cents", 9);
        setPhysical("pressure_volume", .5); setPhysical("pressure_filter", .4);
        setPhysical("pressure_vibrato", 8); setPhysical("pressure_wah", .4);
        setPhysical("expression_source", 0); setPhysical("pedal_input", 1);
        setPhysical("glide_mode", 1); setPhysical("output_mode", 1);
        mom[pressureIndex] = .6; mom[expressionPedalIndex] = .7;
        extConnected = true;
        const auto& p = ceres_re::parameters[index];
        std::vector<Change> events {{17, 48, 100, true}, {31, 55, 80, true},
                                    {600, 48, 0, true}, {601, 60, 110, true}};
        const unsigned values = p.steps ? p.steps : 3;
        for (unsigned v = 0; v < values; ++v)
            events.push_back({1024 + v * 1024 + 17, index, p.steps ? double(v) : double(v) / 2});
        const unsigned tail = 2048 + values * 1024;
        events.push_back({tail + 17, 55, 0, true});
        events.push_back({tail + 31, 60, 0, true});
        CCeres device(48000);
        output.begin(std::string("control/") + p.key);
        // Reset after the selector sweep also applies every deferred voice/
        // quality selection. Fresh notes prove the resulting configuration.
        events.push_back({tail + 2048 + 17, 67, 100, true});
        events.push_back({tail + 3072 + 31, 67, 0, true});
        run(output, device, tail + 4096, 48000, events, tail + 2048);
        output.end();
    }
}

void resetScenes(Stream& output, const StoredPatch& init) {
    for (double rate : {44100., 48000., 88200., 96000., 192000.})
    for (unsigned quality = 0; quality < 3; ++quality) {
        defaults();
        StoredPatch resetDefaults{};
        std::copy_n(mom.begin(), parameterCount, resetDefaults.begin());
        for (unsigned kind = 0; kind < 2; ++kind) {
            load(kind ? init : resetDefaults, quality);
            CCeres device(rate);
            auto events = musicalEvents();
            events.push_back({7409, ceres_re::idx::voice_count, 15});
            events.push_back({7439, ceres_re::idx::oversampling, double((quality + 1) % 3)});
            events.push_back({8209, 36, 100, true});
            events.push_back({8277, 48, 80, true});
            events.push_back({10301, 36, 0, true});
            events.push_back({10331, 48, 0, true});
            output.begin(std::string(kind ? "init-reset/" : "motherboard-reset/") +
                         std::to_string(unsigned(rate)) + "/q" + std::to_string(1u << quality));
            run(output, device, 12288, rate, events, 8192);
            require(CCeresTestAccess::polyphony(device) == 16 &&
                    CCeresTestAccess::factor(device) == (1u << ((quality + 1) % 3)),
                    "audio reset applies requested voice/quality state");
            output.end();
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    require(argc == 4, "bank.txt audio.f32 scenes.tsv");
    std::ifstream file(argv[1]);
    unsigned patches = 0; file >> patches;
    require(patches == 23, "the approved Rack bank has 23 patches");
    std::vector<BankPatch> bank;
    StoredPatch init{}; bool hasInit = false;
    for (unsigned n = 0; n < patches; ++n) {
        BankPatch p; file >> p.name;
        for (unsigned i = 0; i < parameterCount; ++i) {
            std::string key; file >> key >> p.values[i];
            require(bool(file) && key == ceres_re::parameters[i].key &&
                    std::isfinite(p.values[i]), "complete ordered saved properties");
        }
        if (p.name == "Init.repatch") { init = p.values; hasInit = true; }
        bank.push_back(p);
    }
    std::string extra;
    require(!(file >> extra) && hasInit, "exact bank inventory including Init");
    Stream output(argv[2], argv[3]);
    bankScenes(output, bank);
    controlScenes(output, init);
    resetScenes(output, init);
    output.finish();
}

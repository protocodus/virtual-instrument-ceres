#pragma once

#include <JuceHeader.h>

#include "DSP/ceres.hpp"

#if CERES_CLAP_EXTENSIONS
 #include <clap-juce-extensions/clap-juce-extensions.h>
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// JUCE front end over the canonical circuit engine, ceres::Synth. It owns
// parameter mapping, MIDI, programs, state, voice and quality settings, the
// EXT IN bus and latency; synthesis stays in Source/DSP. Parameters are the
// engine's own keys (ceres::parameterInfo), presented through
// Source/PluginControls.h.
class CeresAudioProcessor final : public juce::AudioProcessor
#if CERES_CLAP_EXTENSIONS
    , public clap_juce_extensions::clap_juce_audio_processor_capabilities
#endif
{
public:
    // Every instance is the same unit: the seed fixes each board's
    // tolerances, oscillator phases and noise. It is the README demos' seed,
    // so a program is the instrument its demo plays (parameters hold floats,
    // so a program's values match the preset's to float precision).
    static constexpr std::uint64_t kSeed = 20261017u;
    // MIDI pitch bend reaches the engine as +/-2 semitones, as in the demos.
    static constexpr double kBendSemitones = 2.0;
    // The circuit's slowest coupling time constant, the output bus: the four
    // voice branches' 4.7 uF and 1k into the PRA's 47k load, 4.7 uF x (1k +
    // 4 x 47k) (outputCouplingTau in ceres.cpp). After a release it is what
    // still sounds longest.
    static constexpr double kBusCouplingSeconds = 4.7e-6 * (1.0e3 + 4 * 47.0e3);

    CeresAudioProcessor();
    ~CeresAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // EXT IN, the rear jack FUNCTION EXT listens to, is a sidechain rather
    // than a main input: VST3 sees an auxiliary bus, CLAP a non-main port.
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Extensions; }
#if CERES_CLAP_EXTENSIONS
    bool isInputMain (int) override { return false; }
#endif

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return static_cast<int> (programs.size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }

    // True while a Voices or Oversampling change waits for every key, the
    // sustain pedal and every envelope to come to rest.
    bool configurationPending() const noexcept { return pendingConfiguration.load(); }
    // The voices and oversampling factor the engine runs with now.
    int appliedVoices() const noexcept { return appliedVoiceCount.load(); }
    int appliedOversampling() const noexcept { return appliedFactor.load(); }
    // True once prepared at a sample rate the engine runs at.
    bool engineRunning() const noexcept { return running.load(); }
    // True when the host's sample rate is outside the engine's 8-384 kHz;
    // the plug-in is then silent.
    bool sampleRateUnsupported() const noexcept { return unsupportedRate.load(); }

    // The program list: the engine's research presets in its own order.
    // Programs leave these keys alone: the expression pedal is a performance
    // control, EXT LEVEL belongs to what is plugged into EXT IN, and the
    // output trim is the plug-in's level, not part of a sound.
    static bool programSetsKey (const char* key) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Binding
    {
        const char* key = nullptr;          // static storage, from the engine
        std::atomic<float>* raw = nullptr;  // the parameter's plain value
        double offset = 0.0;                // a choice's index i means minimum + i
        float last = 0.0f;
    };

    // Reads every parameter into the patch; true when the patch changed.
    bool readControls (bool force) noexcept;
    void applyExpression() noexcept;
    void startEngine (double sampleRate);
    void restartEngine() noexcept;
    void restoreControllers() noexcept;
    void handleMidi (const juce::uint8* data, int size) noexcept;
    // ext is EXT IN's first channel, or nullptr when the bus is off. It may
    // be the same memory as out: each sample is read before it is written.
    void render (float* out, const float* ext, int begin, int end) noexcept;
    void advanceConfiguration() noexcept;
    bool idle() const noexcept;
    void publishConfiguration() noexcept;

    struct Vst3Extensions final : juce::VST3ClientExtensions
    {
        bool getPluginHasMainInput() const override { return false; }
    } vst3Extensions;

    juce::AudioProcessorValueTreeState parameters;
    std::vector<Binding> bindings;
    std::size_t expressionBinding = ~std::size_t { 0 };
    std::atomic<float>* voiceCountValue = nullptr;
    std::atomic<float>* oversamplingValue = nullptr;
    std::atomic<float>* ampReleaseValue = nullptr;
    std::atomic<float>* globalReleaseValue = nullptr;

    struct Program
    {
        juce::String name;
        ceres::Patch patch;
    };
    std::vector<Program> programs;
    std::atomic<int> currentProgram { 0 };

    std::unique_ptr<ceres::Synth> engine;
    ceres::Patch patch {};
    double expressionLever = 1.0; // the parameter
    int expressionController = 127; // MIDI CC11, 127 until a pedal moves

    // MIDI controller positions survive a reset; keys and pedal do not.
    double bend = 0.0, touch = 0.0;
    bool pedal = false;
    std::array<std::uint32_t, 128> heldKeys {};
    std::uint32_t heldCount = 0;

    // Voices and oversampling reconfigure the engine only when it is idle:
    // the output fades for 20 ms, the engine resets and reconfigures, then
    // fades back in. A key or pedal during the fade-out cancels it.
    unsigned requestedVoices = 4, requestedFactor = 4;
    enum class Fade { none, out, in };
    Fade fade = Fade::none;
    double fadeGain = 1.0, fadeStep = 0.0;
    int fadeRemaining = 0, fadeFrames = 1;

    std::atomic<bool> pendingConfiguration { false }, running { false }, unsupportedRate { false };
    std::atomic<int> appliedVoiceCount { 4 }, appliedFactor { 4 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CeresAudioProcessor)
};

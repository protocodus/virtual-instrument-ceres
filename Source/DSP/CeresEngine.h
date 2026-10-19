#pragma once

#include <cstddef>

namespace ceres
{
// The scaffold engine: it takes the calls every front end makes and renders
// silence. What Ceres is has not been decided yet; the instrument's model
// replaces the body of process() and adds its controls and presets.
class CeresEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void allNotesOff();
    void process (float* left, float* right, std::size_t frames);

    double sampleRate() const { return rate; }
    int latencySamples() const { return 0; }

private:
    double rate = 48000.0;
};
} // namespace ceres

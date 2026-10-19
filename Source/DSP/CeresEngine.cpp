#include "CeresEngine.h"

#include <algorithm>

namespace ceres
{
void CeresEngine::prepare (double sampleRate)
{
    rate = sampleRate;
    reset();
}

void CeresEngine::reset() {}

void CeresEngine::noteOn (int, int) {}

void CeresEngine::noteOff (int) {}

void CeresEngine::allNotesOff() {}

void CeresEngine::process (float* left, float* right, std::size_t frames)
{
    std::fill (left, left + frames, 0.0f);
    std::fill (right, right + frames, 0.0f);
}
} // namespace ceres

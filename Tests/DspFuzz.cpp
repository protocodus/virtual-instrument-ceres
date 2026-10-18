// U4 input fuzzing of the JUCE-free engine through the shared portfolio
// harness (Tests/FuzzHarness.h is a byte-identical copy of the master; change
// it in reason-rack-extensions). Every input starts from a fresh engine, so a
// saved crash replays alone; commands and rendered frames are bounded
// whatever the input length.
#include "DSP/CeresEngine.h"
#include "FuzzHarness.h"

extern "C" int LLVMFuzzerTestOneInput (const std::uint8_t* data, std::size_t size)
{
    using namespace protocodus::fuzz;
    Input input { data, size };

    ceres::CeresEngine engine;
    engine.prepare (input.Rate());
    const auto anyInt = [&] { return static_cast<int> (input.Word()); };

    constexpr std::size_t maxFrames = 512;
    Sentinels<maxFrames> block;
    for (int command = 0; command < 64 && ! input.Empty(); ++command)
    {
        switch (input.Pick (5))
        {
            case 0: engine.noteOn (anyInt() % 300, anyInt() % 300); break;
            case 1: engine.noteOff (anyInt() % 300); break;
            case 2: engine.allNotesOff(); break;
            case 3: engine.reset(); break;
            default: engine.prepare (input.Rate()); break;
        }
        const std::size_t frames = 1 + input.Word() % maxFrames;
        block.Arm();
        engine.process (block.Left(), block.Right(), frames);
        block.Check (frames);
    }
    return 0;
}

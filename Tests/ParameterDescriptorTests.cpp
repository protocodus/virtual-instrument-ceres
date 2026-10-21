// Parameter-descriptor contract: parameterCount/parameterInfo/getParameter
// describe exactly what setParameter accepts, without allocating, in both the
// native and the embedded build. Front ends (the JUCE plug-in) build their
// parameter lists from these descriptors, so a mismatch here would expose a
// range, default or switch the engine does not apply.
//
// Each property is checked against setParameter's observable behaviour, not
// against the table the descriptors read: clamping at both ends, the stored
// default, rounding of switch keys, and (embedded build, where Patch is plain
// numbers) that the descriptors reach every Patch member exactly once.

#include "ceres.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <string>

namespace {
bool watch = false;
std::size_t allocations = 0;
int failures = 0;

void* allocate(std::size_t size) {
    if (watch) ++allocations;
    if (void* p = std::malloc(size ? size : 1)) return p;
    std::abort();
}

void check(bool good, const std::string& what) {
    if (!good) {
        ++failures;
        std::fprintf(stderr, "FAIL %s\n", what.c_str());
    }
}

double stored(const ceres::Patch& patch, const char* key) {
    double value = std::numeric_limits<double>::quiet_NaN();
    ceres::getParameter(patch, key, value);
    return value;
}

// One descriptor against setParameter: clamping, default and rounding.
void describe(const ceres::ParameterInfo& info) {
    const std::string key = info.key;
    const double low = info.minimum, high = info.maximum, span = high - low;
    check(std::isfinite(low) && std::isfinite(high) && low < high, key + " has a finite, ordered range");
    check(info.defaultValue >= low && info.defaultValue <= high, key + " default lies in its range");

    const ceres::Patch defaults{};
    check(stored(defaults, info.key) == info.defaultValue, key + " default is a default Patch's value");

    ceres::Patch patch{};
    check(ceres::setParameter(patch, info.key, high + 1 + std::abs(high)), key + " is a setParameter key");
    check(stored(patch, info.key) == high, key + " clamps to its maximum");
    check(ceres::setParameter(patch, info.key, low - 1 - std::abs(low)) && stored(patch, info.key) == low,
          key + " clamps to its minimum");

    if (info.integer) {
        check(low == std::round(low) && high == std::round(high) && info.defaultValue == std::round(info.defaultValue),
              key + " switch has whole-number limits and default");
        for (double position = low; position <= high; position += 1) {
            ceres::setParameter(patch, info.key, position);
            check(stored(patch, info.key) == position, key + " stores every switch position");
            ceres::setParameter(patch, info.key, position + (position < high ? .3 : -.3));
            check(stored(patch, info.key) == position, key + " rounds to the nearest switch position");
        }
    } else {
        // Interior points away from the ends (the attack-saturation keys
        // raise values below .001 to .001, a documented engine rule).
        for (const double fraction : { .25, .5, .75, .9 }) {
            const double value = low + span * fraction;
            ceres::setParameter(patch, info.key, value);
            check(stored(patch, info.key) == value, key + " stores interior values unchanged");
        }
    }
}

#if defined(CERES_EMBEDDED_TARGET)
// The embedded Patch is plain doubles: each descriptor must own exactly one
// member, and together they must cover all of them.
void coverage() {
    constexpr std::size_t slots = sizeof(ceres::Patch) / sizeof(double);
    check(sizeof(ceres::Patch) == slots * sizeof(double), "embedded Patch holds only doubles");
    std::set<std::size_t> owned;
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i) {
        const auto info = ceres::parameterInfo(i);
        const ceres::Patch defaults{};
        ceres::Patch patch{};
        ceres::setParameter(patch, info.key, info.defaultValue != info.maximum ? info.maximum : info.minimum);
        std::size_t changed = 0, slot = 0;
        for (std::size_t s = 0; s < slots; ++s)
            if (std::memcmp(reinterpret_cast<const char*>(&patch) + s * sizeof(double),
                            reinterpret_cast<const char*>(&defaults) + s * sizeof(double), sizeof(double)) != 0) {
                ++changed;
                slot = s;
            }
        check(changed == 1, std::string(info.key) + " writes exactly one Patch member");
        check(owned.insert(slot).second, std::string(info.key) + " owns its own Patch member");
    }
    check(owned.size() == slots, "the descriptors reach every Patch member");
}
#else
// Every research preset survives being rebuilt from the descriptors alone:
// the same values, and the same samples from a fresh engine.
void presets() {
    for (const auto& name : ceres::presetNames()) {
        const ceres::Patch original = ceres::preset(name);
        ceres::Patch rebuilt{};
        for (std::size_t i = 0; i < ceres::parameterCount(); ++i) {
            const auto info = ceres::parameterInfo(i);
            double value = 0;
            check(ceres::getParameter(original, info.key, value), name + " reads " + info.key);
            ceres::setParameter(rebuilt, info.key, value);
            check(stored(rebuilt, info.key) == value, name + " " + info.key + " round-trips");
        }
        ceres::Synth a(48000, 7), b(48000, 7);
        a.setPatch(original);
        b.setPatch(rebuilt);
        bool same = true;
        for (int frame = 0; frame < 9600; ++frame) {
            if (frame == 0) { a.noteOn(48, .8); b.noteOn(48, .8); a.noteOn(64, .5); b.noteOn(64, .5); }
            if (frame == 4800) { a.noteOff(48); b.noteOff(48); a.noteOff(64); b.noteOff(64); }
            same = same && a.process() == b.process();
        }
        check(same, name + " renders identically when rebuilt from the descriptors");
    }
}
#endif
} // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
    const std::size_t count = ceres::parameterCount();
    check(count > 0, "the engine describes its parameters");

    // Descriptor reads allocate nothing, in either build.
    ceres::Patch patch{};
    watch = true;
    std::size_t keyBytes = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto info = ceres::parameterInfo(i);
        double value = 0;
        if (info.key && ceres::getParameter(patch, info.key, value)) keyBytes += std::strlen(info.key);
    }
    double untouched = 42;
    const bool unknown = ceres::getParameter(patch, "not_a_key", untouched) || ceres::getParameter(patch, nullptr, untouched);
    const auto past = ceres::parameterInfo(count);
    watch = false;
    check(allocations == 0, "parameterInfo and getParameter allocate nothing");
    check(keyBytes > 0, "every descriptor key reads back");
    check(!unknown && untouched == 42, "unknown and null keys are rejected and leave the value unchanged");
    check(past.key == nullptr, "a descriptor past the end has no key");

    std::set<std::string> keys;
    for (std::size_t i = 0; i < count; ++i) {
        const auto info = ceres::parameterInfo(i);
        check(info.key != nullptr && keys.insert(info.key).second, "descriptor keys are present and unique");
        if (info.key) describe(info);
    }

    // A nonfinite member reads back as the engine applies it: its default.
    ceres::Patch broken{};
    broken.volume = std::numeric_limits<double>::infinity();
    check(stored(broken, "volume") == ceres::Patch{}.volume, "a nonfinite member reads back as its default");

#if defined(CERES_EMBEDDED_TARGET)
    coverage();
#else
    presets();
#endif
    std::printf("Ceres parameter descriptors (%zu keys): %d failure(s)\n", count, failures);
    return failures == 0 ? 0 : 1;
}

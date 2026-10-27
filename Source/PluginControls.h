#pragma once

// How the plug-in presents the engine's parameters: the CS-50 panel section
// each key sits in (Yamaha CS Series Servicing Guide, printed pp.2-7, "Panel
// construction" and "Function of panel lever"), its host name, its panel
// caption, how its value reads and how a lever spans its range.
//
// Keys, ranges, defaults and which keys are switches are NOT here: they come
// from the engine (ceres::parameterInfo). An engine key without an entry
// below still becomes a parameter, in the Engine group under its own key, so
// the plug-in always exposes exactly the engine's parameter list.

#include <cctype>
#include <cstring>
#include <string>

namespace ceres_plugin
{
// The program name a host lists for an engine preset key, which the README
// demos use too: "string1" -> "String 1", "electric_piano" -> "Electric Piano".
inline std::string programName (const std::string& key)
{
    std::string name;
    bool wordStart = true;
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        const auto c = static_cast<unsigned char> (key[i]);
        if (c == '_' || c == '-')
        {
            name += ' ';
            wordStart = true;
            continue;
        }
        if (std::isdigit (c) && i > 0 && ! std::isdigit (static_cast<unsigned char> (key[i - 1])))
            name += ' ';
        name += static_cast<char> (wordStart ? std::toupper (c) : c);
        wordStart = false;
    }
    return name;
}

enum class Unit
{
    Plain,       // the engine's own number
    Fraction,    // 0..1 lever travel or level, shown in percent
    Duty,        // pulse width, 0.5..0.9 of the cycle
    DutySwing,   // PWM depth, +/- fraction of the cycle
    Hertz,
    Seconds,
    Cents,
    Decibels,
    Octaves,
    EgUnits,     // VCF EG level, octaves at an EG depth of 1
    Volts,       // loaded bus volts (BRILLIANCE, RESONANCE)
    Degrees,     // a phase given in cycles
    Microseconds,
    SemitonesPerSecond,
    Ratio,
    Kelvin       // a temperature change
};

enum class Scale
{
    Linear,
    Logarithmic, // equal lever travel per ratio; the range must be positive
    Centred      // power skew placing `centre` at mid-travel; for 0-based spans
};

struct Group
{
    const char* id;
    const char* name;    // host group name
    const char* caption; // editor heading, as short as the panel prints it
};

struct Control
{
    const char* key;       // engine key, which is also the parameter ID
    const char* group;     // a Group id
    const char* name;      // host parameter name
    const char* caption;   // editor label, as the panel prints it
    Unit unit;
    Scale scale = Scale::Linear;
    double centre = 0.0;   // Scale::Centred only
    // Switch positions, "|"-separated, for a key the engine rounds or the
    // hardware only offers in steps of one: the parameter becomes a choice
    // whose index i means minimum + i.
    const char* positions = nullptr;
    // The lever reads upwards towards the minimum (RESONANCE: lower RES-bus
    // volts mean more resonance, so MAX is at the top as on the panel).
    bool inverted = false;
};

// Groups in panel order: Panel 1, Panel 2, Panel 3, the rear panel's jacks,
// then the engine's calibration controls and the plug-in's own settings.
inline constexpr Group kGroups[] {
    { "vco", "VCO", "VCO" },
    { "vcf", "VCF", "VCF" },
    { "vca", "VCA", "VCA" },
    { "pitch", "Pitch", "PITCH" },
    { "ring", "Ring modulator", "RING MODULATOR" },
    { "tone", "Resonance and brilliance", "TONE" },
    { "transposition", "Transposition", "TRANSPOSITION" },
    { "touch", "Touch response", "TOUCH RESPONSE" },
    { "volume", "Volume", "VOLUME" },
    { "sub", "Sub oscillator", "SUB-OSCILLATOR" },
    { "sustain", "Sustain", "SUSTAIN" },
    { "portamento", "Portamento and glissando", "PORTAMENTO / GLISSANDO" },
    { "rear", "Rear panel", "REAR PANEL" },
    { "engine", "Engine", "ENGINE CALIBRATION" },
    { "plugin", "Plug-in", "PLUG-IN" },
};

// The group that takes engine keys without an entry below.
inline constexpr const char* kUnlistedGroup = "engine";

inline constexpr Control kControls[] {
    // Panel 1 VCO: SPEED PWM PW, the pulse and saw levers, NOISE.
    { "pwm_rate", "vco", "PWM speed", "SPEED", Unit::Hertz, Scale::Logarithmic },
    { "pwm_depth", "vco", "PWM depth", "PWM", Unit::DutySwing },
    { "pulse_width", "vco", "Pulse width", "PW", Unit::Duty },
    { "pulse", "vco", "Pulse", "PULSE", Unit::Fraction },
    { "saw", "vco", "Saw", "SAW", Unit::Fraction },
    { "noise", "vco", "Noise", "NOISE", Unit::Fraction },
    // Panel 1 VCF: HPF RESH LPF RESL, then the VCF EG's IL AL A D R.
    { "hp_cutoff", "vcf", "HPF cutoff", "HPF", Unit::Hertz, Scale::Logarithmic },
    { "hp_resonance", "vcf", "HPF resonance", "RESH", Unit::Fraction },
    { "lp_cutoff", "vcf", "LPF cutoff", "LPF", Unit::Hertz, Scale::Logarithmic },
    { "lp_resonance", "vcf", "LPF resonance", "RESL", Unit::Fraction },
    { "filter_initial", "vcf", "VCF EG initial level", "IL", Unit::EgUnits },
    { "filter_peak", "vcf", "VCF EG attack level", "AL", Unit::EgUnits },
    { "filter_attack", "vcf", "VCF EG attack", "A", Unit::Seconds, Scale::Logarithmic },
    { "filter_decay", "vcf", "VCF EG decay", "D", Unit::Seconds, Scale::Logarithmic },
    { "filter_release", "vcf", "VCF EG release", "R", Unit::Seconds, Scale::Logarithmic },
    // Panel 1 VCA: the VCF and sine LEVEL levers, the VCA EG's A D S R, LEVEL.
    { "filter_level", "vca", "VCF level", "VCF", Unit::Fraction },
    { "sine", "vca", "Sine level", "SINE", Unit::Fraction },
    { "amp_attack", "vca", "VCA EG attack", "A", Unit::Seconds, Scale::Logarithmic },
    { "amp_decay", "vca", "VCA EG decay", "D", Unit::Seconds, Scale::Logarithmic },
    { "amp_sustain", "vca", "VCA EG sustain", "S", Unit::Fraction },
    { "amp_release", "vca", "VCA EG release", "R", Unit::Seconds, Scale::Logarithmic },
    { "amp_level", "vca", "VCA level", "LEVEL", Unit::Fraction },
    // Panel 2, left to right.
    { "tune_cents", "pitch", "Pitch", "PITCH", Unit::Cents },
    { "ring_attack", "ring", "Ring attack time", "ATTACK", Unit::Seconds, Scale::Logarithmic },
    { "ring_decay", "ring", "Ring decay time", "DECAY", Unit::Seconds, Scale::Logarithmic },
    { "ring_depth", "ring", "Ring depth", "DEPTH", Unit::Octaves },
    { "ring_rate", "ring", "Ring speed", "SPEED", Unit::Hertz, Scale::Logarithmic },
    { "ring_mix", "ring", "Ring modulation", "MOD", Unit::Fraction },
    { "resonance_offset", "tone", "Resonance", "RESO", Unit::Volts, Scale::Linear, 0.0, nullptr, true },
    { "brilliance", "tone", "Brilliance", "BRILL", Unit::Volts },
    // TRANSPOSITION is a four-position switch (guide printed p.5); the
    // engine's octave would take fractions no position produces.
    { "octave", "transposition", "Transposition", "OCTAVE", Unit::Plain, Scale::Linear, 0.0,
      "2 oct down|1 oct down|Normal|1 oct up" },
    { "pressure_vibrato", "touch", "Touch VCO", "VCO", Unit::Cents },
    { "pressure_wah", "touch", "Touch VCF", "VCF", Unit::Octaves },
    { "pressure_filter", "touch", "Touch brilliance", "BRILL", Unit::Octaves },
    { "pressure_volume", "touch", "Touch level", "LEVEL", Unit::Fraction },
    { "volume", "volume", "Volume", "VOLUME", Unit::Fraction },
    // Panel 3: SUB-OSCILLATOR FUNCTION SPEED VCO VCF VCA, SUSTAIN I/II and
    // its lever, PORTAMENTO/GLISSANDO and its lever. FUNCTION's sixth
    // position, EXT, takes the SUB signal from the EXT IN jack.
    { "lfo_waveform", "sub", "Sub osc function", "FUNCTION", Unit::Plain, Scale::Linear, 0.0,
      "Sine|Saw down|Saw up|Square|Noise|External" },
    { "lfo_rate", "sub", "Sub osc speed", "SPEED", Unit::Hertz, Scale::Logarithmic },
    { "vibrato_cents", "sub", "Sub osc VCO", "VCO", Unit::Cents },
    { "filter_lfo", "sub", "Sub osc VCF", "VCF", Unit::Octaves },
    { "tremolo", "sub", "Sub osc VCA", "VCA", Unit::Fraction },
    { "sustain_mode", "sustain", "Sustain mode", "I / II", Unit::Plain, Scale::Linear, 0.0, "I|II" },
    { "global_release", "sustain", "Sustain", "SUSTAIN", Unit::Seconds, Scale::Centred, 2.0 },
    { "glide_mode", "portamento", "Portamento mode", "MODE", Unit::Plain, Scale::Linear, 0.0,
      "Off|Portamento|Glissando" },
    { "glide_rate", "portamento", "Portamento speed", "SPEED", Unit::SemitonesPerSecond, Scale::Logarithmic },
    // Rear panel: the FOOT CONTROL pedal, and the EXT IN jack's LEVEL pot,
    // the wiper ratio (its taper is not published). The jack is the
    // plug-in's EXT IN input bus.
    { "expression", "rear", "Expression pedal", "PEDAL", Unit::Fraction },
    { "ext_level", "rear", "EXT level", "EXT LEVEL", Unit::Fraction },
    // Engine calibration and modern adaptations, not CS-50 panel controls.
    { "pwm_phase", "engine", "PWM phase", "PWM PHASE", Unit::Degrees },
    { "key_track", "engine", "Filter key tracking", "KEY TRACK", Unit::Fraction },
    { "hp_envelope_scale", "engine", "HPF EG depth", "HPF EG", Unit::Plain },
    { "lp_envelope_scale", "engine", "LPF EG depth", "LPF EG", Unit::Plain },
    { "filter_attack_headroom", "engine", "VCF EG attack headroom", "VCF HEADRM", Unit::Plain, Scale::Logarithmic },
    { "filter_attack_saturation", "engine", "VCF EG attack saturation", "VCF SAT", Unit::Plain, Scale::Centred, 1.0 },
    { "amp_attack_headroom", "engine", "VCA EG attack headroom", "VCA HEADRM", Unit::Plain, Scale::Logarithmic },
    { "amp_attack_saturation", "engine", "VCA EG attack saturation", "VCA SAT", Unit::Plain, Scale::Centred, 1.0 },
    { "voice_spread_cents", "engine", "Voice spread", "SPREAD", Unit::Cents },
    { "drift_cents", "engine", "Drift", "DRIFT", Unit::Cents },
    { "board_pitch_tolerance", "engine", "Board pitch tolerance", "BD PITCH", Unit::Fraction },
    { "board_filter_tolerance", "engine", "Board filter tolerance", "BD FILTER", Unit::Fraction },
    { "board_envelope_tolerance", "engine", "Board envelope tolerance", "BD ENV", Unit::Fraction },
    { "board_envelope_time_tolerance", "engine", "Board envelope time tolerance", "BD EG TIME", Unit::Fraction },
    { "board_vca_tolerance", "engine", "Board VCA tolerance", "BD VCA", Unit::Fraction },
    { "board_temperature_rise", "engine", "Temperature rise", "TEMP", Unit::Kelvin },
    { "expression_source", "engine", "Touch source", "TOUCH SRC", Unit::Plain, Scale::Linear, 0.0,
      "Aftertouch|Velocity" },
    { "velocity_sensitivity", "engine", "Velocity sensitivity", "VEL SENS", Unit::Fraction },
    { "drive", "engine", "Drive", "DRIVE", Unit::Ratio, Scale::Logarithmic },
    { "reset_microseconds", "engine", "VCO reset time", "RESET", Unit::Microseconds, Scale::Logarithmic },
    // A utility gain after the whole circuit, not a Yamaha control: the
    // plug-in's output trim. Programs leave it where it is.
    { "output_trim_db", "plugin", "Output trim", "TRIM", Unit::Decibels },
};

// The plug-in's own settings, not engine keys.
inline constexpr const char* kVoiceCountId = "voice_count";
inline constexpr const char* kOversamplingId = "oversampling";
inline constexpr int kDefaultVoices = 4;           // the CS-50's four boards
inline constexpr int kOversamplingFactors[] { 1, 2, 4 };
inline constexpr int kDefaultOversamplingIndex = 2; // 4x, the engine's reference quality

inline const Control* findControl (const char* key) noexcept
{
    for (const auto& control : kControls)
        if (std::strcmp (control.key, key) == 0)
            return &control;
    return nullptr;
}
} // namespace ceres_plugin

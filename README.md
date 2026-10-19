# Ceres

Ceres is a Protocodus virtual instrument for VST3, CLAP and Standalone on macOS, Windows and Linux, plus an Audio Unit on macOS. This repository is its scaffold: the plug-in builds, loads and passes MIDI to a JUCE-free C++17 engine that renders silence until the instrument itself is defined.

![Ceres editor](Docs/screenshots/ceres.png)

## Audio demos

One demo per factory preset, rendered by the main build from the phrases in `Docs/audio/midi`. Ceres has no presets yet, so there are no demos.

<!-- audio-demos-begin -->
<!-- audio-demos-end -->

## Version history

### Unreleased — 0.0.1 — scaffold

- Repository, plug-in front end, test tiers and CI set up to the portfolio standard. The engine renders silence; there are no controls or presets.

## Downloads

<!-- build-download-begin -->
<!-- build-download-end -->

---

## How it works

Not defined yet. `Source/DSP/CeresEngine.*` takes note-on, note-off and all-notes-off and fills the output with zeros; the plug-in in `Source/Plugin*` applies MIDI at its own sample and stores a versioned state chunk.

## Known gaps

- No instrument model, controls or presets. The engine is a silent placeholder.
- `Ceres.AudioDemos` (the audio tier) fails until the first preset and its `Docs/audio/midi` phrase exist; the renderer requires at least one.
- The editor is a plain panel with the product name.

## Build

The engine and the unit, fuzz and audio tiers build without JUCE:

```sh
cmake -S . -B build-dsp -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel
ctest --test-dir build-dsp -L unit --output-on-failure
```

The plug-in build fetches the pinned JUCE 8.0.14 and clap-juce-extensions unless `CERES_JUCE_PATH` and `CERES_CLAP_JUCE_EXTENSIONS_PATH` point to local checkouts. On Linux, install JUCE's ALSA, FreeType, fontconfig and X11 development packages first (the list is in `.github/workflows/ci.yml`); on macOS, `scripts/build-macos.sh` builds universal binaries and runs every test.

```sh
cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release -DCERES_BUILD_PLUGIN=ON -DBUILD_TESTING=ON
cmake --build build-plugin --parallel
ctest --test-dir build-plugin --output-on-failure
```

Every test is `Ceres.<Suite>` with one tier label: `unit`, `audio` (`Ceres.AudioDemos` renders every preset's demo twice and compares the bits), `render` (`Ceres.EditorRender`, the editor screenshot) or `fuzz` (`Ceres.DspFuzz`, and `Ceres.ParameterMatrix` in the plug-in build). `-DCERES_ENABLE_SANITIZERS=ON` builds everything under ASan + UBSan, and `-DCERES_BUILD_LIBFUZZER=ON` with Clang builds the libFuzzer target.

## Licensing

Ceres's source code is MIT-licensed; see [LICENSE](LICENSE). The plug-in builds with JUCE (AGPLv3 or a commercial JUCE licence) and clap-juce-extensions; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

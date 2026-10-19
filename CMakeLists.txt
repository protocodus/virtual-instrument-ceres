cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(CERES_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(CERES_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(Ceres VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CERES_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
option(CERES_BUILD_CLAP "Build the CLAP format through clap-juce-extensions" ON)
option(CERES_BUILD_TOOLS "Build native audio tools" ON)
set(CERES_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")
set(CERES_CLAP_JUCE_EXTENSIONS_PATH "" CACHE PATH
    "Optional path to a local clap-juce-extensions checkout with submodules")

include(CTest)

option(CERES_SANITIZE "Compatibility alias for CERES_ENABLE_SANITIZERS" OFF)
option(CERES_ENABLE_SANITIZERS "Instrument development builds with ASan and UBSan" OFF)
option(CERES_BUILD_LIBFUZZER "Build CeresDspFuzz as a Clang libFuzzer target" OFF)
if(CERES_SANITIZE)
    set(CERES_ENABLE_SANITIZERS ON)
endif()
if(CERES_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "CERES_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined,float-cast-overflow
                        -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined,float-cast-overflow)
endif()

set(CERES_DSP_SOURCES Source/DSP/CeresEngine.cpp)

add_library(CeresDSP STATIC ${CERES_DSP_SOURCES})
target_include_directories(CeresDSP PUBLIC Source Source/DSP)
set_target_properties(CeresDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(CeresDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(CeresDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The Rack Extension toolchain is C++17. Compiling the engine at that standard
# here keeps Source/DSP portable for the Rack adapter.
if(NOT MSVC)
    add_library(CeresDSPCxx17 OBJECT ${CERES_DSP_SOURCES})
    target_include_directories(CeresDSPCxx17 PRIVATE Source Source/DSP)
    set_target_properties(CeresDSPCxx17 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    target_compile_options(CeresDSPCxx17 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Standalone circuit engine and embedded ABI remain independent of the
# existing CeresDSP JUCE scaffold. Do not link either into the plugin implicitly.
set(CERES_IDENTITY_FILES Source/DSP/ceres.hpp Source/DSP/ceres.cpp Tools/render_main.cpp Tools/build_identity.hpp.in CMakeLists.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CERES_IDENTITY_FILES})
set(CERES_SOURCE_JSON "{")
set(CERES_IDENTITY_SEPARATOR "")
foreach(CERES_IDENTITY_FILE IN LISTS CERES_IDENTITY_FILES)
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${CERES_IDENTITY_FILE}" CERES_IDENTITY_HASH)
  string(APPEND CERES_SOURCE_JSON "${CERES_IDENTITY_SEPARATOR}\"${CERES_IDENTITY_FILE}\":\"${CERES_IDENTITY_HASH}\"")
  set(CERES_IDENTITY_SEPARATOR ",")
endforeach()
string(APPEND CERES_SOURCE_JSON "}")
configure_file(Tools/build_identity.hpp.in generated/build_identity.hpp @ONLY)

add_library(ceres STATIC Source/DSP/ceres.cpp)
add_library(ceres_embedded STATIC Source/DSP/ceres.cpp)
target_compile_definitions(ceres_embedded PUBLIC CERES_EMBEDDED_TARGET=1)
foreach(target ceres ceres_embedded)
  target_include_directories(${target} PUBLIC Source/DSP)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
if(NOT MSVC)
  target_compile_options(ceres_embedded PRIVATE -fno-exceptions)
endif()
if(CERES_BUILD_TOOLS)
add_executable(ceres_render Tools/render_main.cpp)
set_target_properties(ceres_render PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(ceres_render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(ceres_render PRIVATE ceres)
endif()


if(CERES_BUILD_PLUGIN)
    if(CERES_JUCE_PATH)
        if(NOT EXISTS "${CERES_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "CERES_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${CERES_JUCE_PATH}")
        endif()
        add_subdirectory("${CERES_JUCE_PATH}"
                         "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            # JUCE 8.0.14, pinned to its immutable release commit and archive
            # checksum so automated release builds cannot follow a moved tag.
            URL https://github.com/juce-framework/JUCE/archive/2cdfca8feb300fb424002ba2c2751569e5bacb64.tar.gz
            URL_HASH SHA256=814019716960a16f8f850a1593eeb56e2f21910aff2d673f7d3513b4ebaf251d)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(CERES_BUILD_CLAP)
        if(CERES_CLAP_JUCE_EXTENSIONS_PATH)
            foreach(required_file IN ITEMS CMakeLists.txt
                    clap-libs/clap/CMakeLists.txt
                    clap-libs/clap-helpers/CMakeLists.txt)
                if(NOT EXISTS "${CERES_CLAP_JUCE_EXTENSIONS_PATH}/${required_file}")
                    message(FATAL_ERROR
                        "CERES_CLAP_JUCE_EXTENSIONS_PATH is missing ${required_file}; "
                        "use a checkout with git submodule update --init --recursive")
                endif()
            endforeach()
            add_subdirectory("${CERES_CLAP_JUCE_EXTENSIONS_PATH}"
                             "${CMAKE_CURRENT_BINARY_DIR}/clap-juce-extensions"
                             EXCLUDE_FROM_ALL)
        else()
            include(FetchContent)
            FetchContent_Declare(
                clap_juce_extensions
                GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
                GIT_TAG c1a5ad025f95d01e03267857fa8276ebeed16500
                # Archive downloads omit these gitlinks. Fetch both dependencies
                # at the immutable revisions recorded by the wrapper commit.
                GIT_SUBMODULES clap-libs/clap clap-libs/clap-helpers
                GIT_SUBMODULES_RECURSE TRUE)
            FetchContent_MakeAvailable(clap_juce_extensions)
        endif()
    endif()

    if(APPLE)
        set(CERES_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(CERES_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Ceres
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Protocodus"
        BUNDLE_ID "cz.protocodus.ceres"
        PLUGIN_MANUFACTURER_CODE Cers
        PLUGIN_CODE Cer1
        PRODUCT_NAME "Ceres"
        DESCRIPTION "Ceres virtual instrument"
        FORMATS ${CERES_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    if(CERES_BUILD_CLAP)
        # CLAP is supplied by the JUCE extension, outside JUCE's FORMATS list.
        clap_juce_extensions_plugin(TARGET Ceres
            CLAP_ID "cz.protocodus.ceres"
            CLAP_FEATURES instrument synthesizer stereo)
    endif()

    juce_generate_juce_header(Ceres)

    target_sources(Ceres PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Ceres
        PRIVATE
            CeresDSP
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # The About box names the package this binary ships in, with the build
    # number read the way scripts/release_metadata.py reads it.
    if(NOT "$ENV{BUILD_NUMBER}" STREQUAL "")
        set(CERES_BUILD_NUMBER "$ENV{BUILD_NUMBER}")
    elseif(NOT "$ENV{GITHUB_RUN_NUMBER}" STREQUAL "")
        set(CERES_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
    else()
        set(CERES_BUILD_NUMBER "local")
    endif()

    target_compile_definitions(Ceres
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
        PRIVATE
            CERES_BUILD_IDENTITY="${PROJECT_VERSION}-build.${CERES_BUILD_NUMBER}")
endif()

if(CERES_BUILD_TOOLS)
    # Existing standalone circuit research presets, not the separate silent
    # plugin scaffold. PresetDemoRenderer.h is the unchanged portfolio copy.
    add_executable(RenderCeresPresetDemos Tools/RenderPresetDemos.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(RenderCeresPresetDemos PRIVATE ceres Threads::Threads)
    set_target_properties(RenderCeresPresetDemos PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    if(MSVC)
        target_compile_options(RenderCeresPresetDemos PRIVATE /W4 /permissive-)
    else()
        target_compile_options(RenderCeresPresetDemos PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
  add_executable(ceres_configuration_tests Tests/ConfigurationTests.cpp)
  target_link_libraries(ceres_configuration_tests PRIVATE ceres)
  add_test(NAME Ceres.configuration_contracts COMMAND ceres_configuration_tests)
  add_executable(ceres_oscillator_circuit_tests Tests/OscillatorCircuitTests.cpp)
  target_link_libraries(ceres_oscillator_circuit_tests PRIVATE ceres)
  add_test(NAME Ceres.oscillator_circuit_contracts COMMAND ceres_oscillator_circuit_tests)
  add_executable(ceres_tests Tests/dsp_tests.cpp)
  target_link_libraries(ceres_tests PRIVATE ceres)
  add_test(NAME Ceres.native_dsp_contracts COMMAND ceres_tests)
  add_executable(ceres_sound_path_tests Tests/SoundPathTests.cpp)
  target_link_libraries(ceres_sound_path_tests PRIVATE ceres)
  add_test(NAME Ceres.sound_path_circuit_contracts COMMAND ceres_sound_path_tests)
  add_executable(ceres_capacitor_tests Tests/CapacitorContinuityTests.cpp)
  target_link_libraries(ceres_capacitor_tests PRIVATE ceres)
  add_test(NAME Ceres.capacitor_continuity_contracts COMMAND ceres_capacitor_tests)
  add_executable(ceres_ring_depth_tests Tests/RingDepthContinuityTests.cpp)
  target_link_libraries(ceres_ring_depth_tests PRIVATE ceres)
  add_test(NAME Ceres.ring_depth_continuity_contracts COMMAND ceres_ring_depth_tests)
  add_executable(ceres_ring_carrier_tests Tests/RingCarrierTests.cpp)
  target_link_libraries(ceres_ring_carrier_tests PRIVATE ceres)
  add_test(NAME Ceres.ring_carrier_circuit_contracts COMMAND ceres_ring_carrier_tests)
  add_executable(ceres_ring_switching_tests Tests/RingSwitchingTests.cpp)
  target_link_libraries(ceres_ring_switching_tests PRIVATE ceres)
  add_test(NAME Ceres.ring_switching_circuit_contracts COMMAND ceres_ring_switching_tests)
  add_executable(ceres_expression_tests Tests/ExpressionTests.cpp)
  target_link_libraries(ceres_expression_tests PRIVATE ceres)
  add_test(NAME Ceres.voice_expression_contracts COMMAND ceres_expression_tests)
  add_executable(ceres_embedded_contract Tests/EmbeddedContract.cpp)
  target_link_libraries(ceres_embedded_contract PRIVATE ceres_embedded)
  if(NOT MSVC)
    target_compile_options(ceres_embedded_contract PRIVATE -fno-exceptions)
  endif()
  add_test(NAME Ceres.embedded_allocation_contract COMMAND ceres_embedded_contract)
  add_executable(ceres_native_parity Tests/ParityDriver.cpp)
  target_link_libraries(ceres_native_parity PRIVATE ceres)
  add_executable(ceres_embedded_parity Tests/ParityDriver.cpp)
  target_link_libraries(ceres_embedded_parity PRIVATE ceres_embedded)
  add_executable(ceres_native_configuration_parity Tests/ConfigurationParityDriver.cpp)
  target_link_libraries(ceres_native_configuration_parity PRIVATE ceres)
  add_executable(ceres_embedded_configuration_parity Tests/ConfigurationParityDriver.cpp)
  target_link_libraries(ceres_embedded_configuration_parity PRIVATE ceres_embedded)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    if(CERES_BUILD_TOOLS)
      add_test(NAME Ceres.configuration_cli_contract COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/check_configuration_cli.py"
        --renderer "$<TARGET_FILE:ceres_render>")
    endif()
    add_test(NAME Ceres.configuration_native_embedded_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/Tests/check_configuration_parity.py"
      --native "$<TARGET_FILE:ceres_native_configuration_parity>"
      --embedded "$<TARGET_FILE:ceres_embedded_configuration_parity>")
    add_test(NAME Ceres.native_embedded_audio_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/Tools/check_embedded.py" --build-dir "$<TARGET_FILE_DIR:ceres_native_parity>"
      --native "$<TARGET_FILE:ceres_native_parity>" --embedded "$<TARGET_FILE:ceres_embedded_parity>")
    add_test(NAME Ceres.active_sound_path_audio_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/Tests/check_sound_path_parity.py" "$<TARGET_FILE_DIR:ceres_native_parity>"
      --native "$<TARGET_FILE:ceres_native_parity>" --embedded "$<TARGET_FILE:ceres_embedded_parity>")
    add_test(NAME Ceres.expression_audio_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/Tests/check_expression_parity.py" "$<TARGET_FILE_DIR:ceres_native_parity>"
      --native "$<TARGET_FILE:ceres_native_parity>" --embedded "$<TARGET_FILE:ceres_embedded_parity>")
    add_test(NAME Ceres.capacitor_native_embedded_parity COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/Tests/check_capacitor_parity.py" "$<TARGET_FILE_DIR:ceres_native_parity>"
      --native "$<TARGET_FILE:ceres_native_parity>" --embedded "$<TARGET_FILE:ceres_embedded_parity>")
  endif()

  foreach(target IN ITEMS ceres_configuration_tests ceres_oscillator_circuit_tests ceres_tests ceres_sound_path_tests
      ceres_capacitor_tests ceres_ring_depth_tests ceres_ring_carrier_tests ceres_ring_switching_tests ceres_expression_tests
      ceres_embedded_contract ceres_native_parity ceres_embedded_parity
      ceres_native_configuration_parity ceres_embedded_configuration_parity)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
  endforeach()
    add_executable(CeresDSPTests Tests/test_dsp.cpp)
    target_link_libraries(CeresDSPTests PRIVATE CeresDSP)
    add_test(NAME Ceres.DSP COMMAND CeresDSPTests)

    # U4 input fuzzing. With CERES_BUILD_LIBFUZZER (Clang) it is a libFuzzer
    # target over coverage-instrumented DSP; otherwise FuzzHarness.h supplies
    # a main that replays Tests/fuzz-regressions/ and then seeded inputs.
    add_executable(CeresDspFuzz Tests/DspFuzz.cpp)
    target_include_directories(CeresDspFuzz PRIVATE Source Source/DSP)
    if(CERES_BUILD_LIBFUZZER)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "CERES_BUILD_LIBFUZZER requires Clang")
        endif()
        target_sources(CeresDspFuzz PRIVATE ${CERES_DSP_SOURCES})
        target_compile_options(CeresDspFuzz PRIVATE
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(CeresDspFuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    else()
        target_link_libraries(CeresDspFuzz PRIVATE CeresDSP)
        target_compile_definitions(CeresDspFuzz PRIVATE PROTOCODUS_FUZZ_MAIN)
    endif()
    file(GLOB CERES_FUZZ_REGRESSIONS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/Tests/fuzz-regressions/*")
    add_test(NAME Ceres.DspFuzz
             COMMAND CeresDspFuzz -seed=1 -runs=300 ${CERES_FUZZ_REGRESSIONS})
    set_tests_properties(Ceres.DspFuzz PROPERTIES TIMEOUT 600 LABELS fuzz)

    if(CERES_BUILD_TOOLS)
        # Every preset demo renders finite, audible, under the ceiling, 10-20 s
        # long and bit-identical on a second render.
        add_test(NAME Ceres.AudioDemos
                 COMMAND RenderCeresPresetDemos --jobs 4 --check
                         "${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/midi")
        set_tests_properties(Ceres.AudioDemos PROPERTIES TIMEOUT 900 LABELS audio)
    endif()

    find_package(Python3 COMPONENTS Interpreter)

    if(CERES_BUILD_PLUGIN)
        foreach(suite IN ITEMS PluginProcessor ParameterMatrix)
            add_executable(Ceres${suite}Tests Tests/${suite}Tests.cpp)
            target_link_libraries(Ceres${suite}Tests PRIVATE Ceres)
            target_include_directories(Ceres${suite}Tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/Source"
                "$<TARGET_PROPERTY:Ceres,JUCE_GENERATED_SOURCES_DIRECTORY>"
                "$<TARGET_PROPERTY:Ceres,INCLUDE_DIRECTORIES>")
        endforeach()

        add_test(NAME Ceres.PluginProcessor COMMAND CeresPluginProcessorTests)
        set_tests_properties(Ceres.PluginProcessor PROPERTIES TIMEOUT 300)

        # A fixed sample of the U4 matrix at one rate and block size per case;
        # the main build's nightly shards cover every case at all nine.
        add_test(NAME Ceres.ParameterMatrix
                 COMMAND CeresParameterMatrixTests --rotation --shard 1/50 --jobs 4)
        set_tests_properties(Ceres.ParameterMatrix PROPERTIES TIMEOUT 1800 LABELS fuzz)

        if(Python3_Interpreter_FOUND)
            add_test(NAME Ceres.EditorRender
                     COMMAND "${Python3_EXECUTABLE}" -B
                             "${CMAKE_CURRENT_SOURCE_DIR}/Tests/EditorRenderTests.py"
                             --processor-tests "$<TARGET_FILE:CeresPluginProcessorTests>")
            set_tests_properties(Ceres.EditorRender PROPERTIES TIMEOUT 600 LABELS render)
        endif()
    endif()

    # Every test carries exactly one tier label so CI selects tiers with
    # `ctest -L`; tests not labelled above are unit contracts.
    get_property(CERES_TESTS DIRECTORY PROPERTY TESTS)
    foreach(test IN LISTS CERES_TESTS)
        get_test_property(${test} LABELS test_labels)
        if(NOT test_labels)
            set_tests_properties(${test} PROPERTIES LABELS unit)
        endif()
    endforeach()
endif()

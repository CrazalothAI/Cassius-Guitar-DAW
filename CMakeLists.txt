cmake_minimum_required(VERSION 3.24)
project(Cassian VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Ship the C/C++ runtime inside the binaries so a fresh PC does not need a
# separate Visual C++ Redistributable. Set before all fetched targets are made.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
# Header-only time stretching and FFT support, pinned independently.
FetchContent_Declare(signalsmith-linear GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG de55e6a50ffcf6f8f43f649692d94691c7025151)
FetchContent_MakeAvailable(signalsmith-linear)
FetchContent_Declare(signalsmith-stretch GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG a670068d9aeb64913331d5cc29337b19a457a7df GIT_SUBMODULES "")
FetchContent_MakeAvailable(signalsmith-stretch)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209)
FetchContent_MakeAvailable(JUCE)
if(WIN32)
    set(CASSIAN_ASIO_SDK "${CMAKE_CURRENT_SOURCE_DIR}/.deps/asio/ASIOSDK" CACHE PATH "Steinberg ASIO SDK root")
    if(NOT EXISTS "${CASSIAN_ASIO_SDK}/common/iasiodrv.h")
        message(FATAL_ERROR "ASIO SDK missing. Run scripts/setup-asio.ps1 first.")
    endif()
endif()
# NAM ships tool executables, not a linkable nam_core target.
FetchContent_Declare(nam GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG 1f42f88535884450104b8711d7595019afa0495b GIT_SUBMODULES_RECURSE TRUE SOURCE_SUBDIR cassian-no-subproject)
FetchContent_MakeAvailable(nam)
file(GLOB_RECURSE NAM_SOURCES CONFIGURE_DEPENDS "${nam_SOURCE_DIR}/NAM/*.cpp")
add_library(nam_core STATIC ${NAM_SOURCES})
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nam_core SYSTEM PUBLIC "${nam_SOURCE_DIR}" "${nam_SOURCE_DIR}/Dependencies/eigen" "${nam_SOURCE_DIR}/Dependencies/nlohmann")
# Eigen keeps its default 16-byte alignment (guaranteed by 64-bit allocators): turning
# alignment off also turned off most of its vectorisation, and NAM ran 2-3x slower.
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
# NAM registers architectures through static initializers. Retain every factory
# even when nothing directly references its translation unit.
if(MSVC)
    target_link_options(nam_core INTERFACE "/WHOLEARCHIVE:$<TARGET_FILE:nam_core>")
elseif(APPLE)
    target_link_options(nam_core INTERFACE "LINKER:-force_load,$<TARGET_FILE:nam_core>")
else()
    target_link_options(nam_core INTERFACE "LINKER:--whole-archive,$<TARGET_FILE:nam_core>,--no-whole-archive")
endif()
if(WIN32)
    target_compile_definitions(nam_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
file(SHA256 "${UI_DIR}/package-lock.json" UI_LOCK_HASH)
set(UI_INSTALL_STAMP "${UI_DIR}/node_modules/.cassian-lock-hash")
set(UI_INSTALLED_HASH "")
if(EXISTS "${UI_INSTALL_STAMP}")
    file(READ "${UI_INSTALL_STAMP}" UI_INSTALLED_HASH)
endif()
if(NOT UI_INSTALLED_HASH STREQUAL UI_LOCK_HASH)
    execute_process(COMMAND ${NPM_EXECUTABLE} ci WORKING_DIRECTORY "${UI_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    file(WRITE "${UI_INSTALL_STAMP}" "${UI_LOCK_HASH}")
endif()
execute_process(COMMAND ${NPM_EXECUTABLE} run build WORKING_DIRECTORY "${UI_DIR}" COMMAND_ERROR_IS_FATAL ANY)
file(GLOB_RECURSE UI_INPUTS CONFIGURE_DEPENDS "${UI_DIR}/src/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${UI_INPUTS} "${UI_DIR}/package-lock.json" "${UI_DIR}/vite.config.js" "${UI_DIR}/index.html")
# Vite produces a single self-contained HTML resource; no install-time asset paths.
juce_add_binary_data(AmpSuiteAssets SOURCES "${UI_DIR}/dist/index.html")
set(CASSIAN_ICON "${UI_DIR}/src/assets/cassian-logo-192.png")
juce_add_binary_data(CassianBrandAssets NAMESPACE CassianBrand HEADER_NAME CassianBrandData.h SOURCES "${CASSIAN_ICON}")
juce_add_plugin(AmpSuite COMPANY_NAME MyAudioLab BUNDLE_ID com.myaudiolab.ampsuite
    COMPANY_COPYRIGHT "Crazaloth"
    PLUGIN_MANUFACTURER_CODE Maud PLUGIN_CODE Apsu
    FORMATS Standalone VST3 PRODUCT_NAME Cassian
    ICON_BIG "${CASSIAN_ICON}" ICON_SMALL "${CASSIAN_ICON}"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER TRUE NEEDS_WEBVIEW2 TRUE)
target_sources(AmpSuite PRIVATE Source/Main.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/RigPack.cpp Source/CompleteRig.cpp Source/PracticeEngine.cpp Source/PracticeSections.cpp Source/TakeLibrary.cpp Source/MidiControl.cpp)
target_sources(AmpSuite_Standalone PRIVATE Source/Standalone.cpp)
target_link_libraries(AmpSuite_Standalone PRIVATE CassianBrandAssets)
target_compile_definitions(AmpSuite_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_compile_definitions(AmpSuite PUBLIC JUCE_WEB_BROWSER=1 JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
if(WIN32)
    target_compile_definitions(AmpSuite PUBLIC JUCE_ASIO=1)
    target_include_directories(AmpSuite PUBLIC "${CASSIAN_ASIO_SDK}/common")
    if(MSVC)
        # Microsoft's prebuilt WebView2 static loader requests MSVCRT; all
        # Cassian targets use /MT instead, so resolve through that one runtime.
        target_link_options(AmpSuite PUBLIC /NODEFAULTLIB:MSVCRT)
    endif()
endif()
target_link_libraries(AmpSuite PRIVATE nam_core AmpSuiteAssets signalsmith-stretch juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra juce::juce_cryptography
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(AmpSuiteTests PRODUCT_NAME CassianTests NEEDS_WEBVIEW2 TRUE NEEDS_WEB_BROWSER TRUE)
    target_sources(AmpSuiteTests PRIVATE tests/ProcessorTests.cpp tests/LibraryTests.cpp tests/QualityTests.cpp tests/SoundTests.cpp tests/PracticeTests.cpp tests/TakeTests.cpp tests/MidiTests.cpp tests/ModulationTests.cpp tests/SceneTests.cpp tests/PracticeSectionTests.cpp tests/GuitarMixTests.cpp tests/PedalboardStateTests.cpp tests/BoardIntegrationTests.cpp)
    target_link_libraries(AmpSuiteTests PRIVATE AmpSuite nam_core juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)
    add_test(NAME processor COMMAND AmpSuiteTests "${nam_SOURCE_DIR}/example_models/wavenet.nam")
    juce_add_console_app(CassianModelCheck PRODUCT_NAME CassianModelCheck)
    target_sources(CassianModelCheck PRIVATE tests/ModelCheck.cpp)
    target_compile_definitions(CassianModelCheck PRIVATE JUCE_USE_CURL=0)
    target_link_libraries(CassianModelCheck PRIVATE nam_core juce::juce_core juce::juce_audio_basics juce::juce_dsp juce::juce_recommended_config_flags)
    add_test(NAME nam_slimmable COMMAND CassianModelCheck "${nam_SOURCE_DIR}/example_models/slimmable_container.nam")
    add_test(NAME nam_wavenet COMMAND CassianModelCheck "${nam_SOURCE_DIR}/example_models/wavenet.nam")
    add_test(NAME nam_lstm COMMAND CassianModelCheck "${nam_SOURCE_DIR}/example_models/lstm.nam")
    juce_add_console_app(CassianAudioCheck PRODUCT_NAME CassianAudioCheck)
    target_sources(CassianAudioCheck PRIVATE tests/AudioInterfaceCheck.cpp)
    target_compile_definitions(CassianAudioCheck PRIVATE JUCE_ASIO=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_include_directories(CassianAudioCheck PRIVATE "${CASSIAN_ASIO_SDK}/common")
    target_link_libraries(CassianAudioCheck PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()

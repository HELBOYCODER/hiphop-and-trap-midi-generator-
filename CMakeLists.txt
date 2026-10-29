cmake_minimum_required(VERSION 3.22)
project(HipHopMidiGenerator VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS" FORCE)
endif()

if(MSVC)
  # static CRT: the .vst3 must not depend on a VC++ redistributable being installed
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(HHMG_BUILD_PLUGIN "Build the JUCE VST3 plugin" ON)
option(HHMG_BUILD_TESTS "Build core tests" ON)

# ---- dependency-free core (theory, patterns, generator, MIDI writer, tool layer) -------------------
add_library(hhmg_core STATIC
  core/src/Theory.cpp core/src/Patterns.cpp core/src/Generator.cpp core/src/MidiFile.cpp core/src/Tools.cpp core/src/Guide.cpp core/src/MidiRead.cpp core/src/Analyze.cpp core/src/Transcribe.cpp core/src/Studio.cpp core/src/AudioRead.cpp core/src/Library.cpp)
target_include_directories(hhmg_core PUBLIC core/include)
set_target_properties(hhmg_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(hhmg_core PRIVATE /W4)
else()
  target_compile_options(hhmg_core PRIVATE -Wall -Wextra)
endif()

add_executable(hhmg-cli cli/main.cpp)
target_link_libraries(hhmg-cli PRIVATE hhmg_core)

if(HHMG_BUILD_TESTS)
  enable_testing()
  add_executable(hhmg_tests tests/test_core.cpp)
  target_link_libraries(hhmg_tests PRIVATE hhmg_core)
  add_test(NAME core COMMAND hhmg_tests)
endif()

# ---- JUCE VST3 plugin --------------------------------------------------------------------------------
if(HHMG_BUILD_PLUGIN)
  include(FetchContent)
  set(JUCE_PATH "" CACHE PATH "Use an existing JUCE checkout instead of downloading")
  option(HHMG_JUCE_PACKAGE "Use an installed JUCE (find_package) - required when cross-compiling" OFF)
  if(HHMG_JUCE_PACKAGE)
    find_package(JUCE CONFIG REQUIRED)
  elseif(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} juce EXCLUDE_FROM_ALL)
  else()
    FetchContent_Declare(juce
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.6
      GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()
  add_subdirectory(plugin)
endif()

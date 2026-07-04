cmake_minimum_required(VERSION 3.22.1)

project(volum LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# cpp dir is NeuralAmpModeler/projects/android/app/src/main/cpp
# Seven levels up lands on the VoLum repo root.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../.." ABSOLUTE)
set(PROJECT_DIR "${REPO_ROOT}/NeuralAmpModeler")

# --- DSP core (mirrors NeuralAmpModeler/tests/CMakeLists.txt source set) ------
# This is the shared, iPlug-free C++ that M0 proves builds + runs on arm64.
set(VOLUM_DSP_SOURCES
  "${REPO_ROOT}/AudioDSPTools/dsp/dsp.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/Delay.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/Reverb.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/NoiseGate.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/RecursiveLinearFilter.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/wav.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/activations.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/convnet.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/conv1d.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/container.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/dsp.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/get_dsp.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/lstm.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/ring_buffer.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/wavenet/model.cpp"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM/wavenet/slimmable.cpp"
  "${PROJECT_DIR}/ToneStack.cpp"
)

add_library(volum SHARED
  volum_jni.cpp
  engine/VolumMobileEngine.cpp
  audio/OboeAudioHost.cpp
  ${VOLUM_DSP_SOURCES}
)

# Oboe (low-latency audio) via the AAR's prefab package.
find_package(oboe REQUIRED CONFIG)

target_include_directories(volum PRIVATE
  "${PROJECT_DIR}"
  "${PROJECT_DIR}/third_party/signalsmith"
  "${REPO_ROOT}/eigen"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM"
  "${REPO_ROOT}/iPlug2/Dependencies/Extras"
  "${REPO_ROOT}/iPlug2/Dependencies/Extras/nlohmann"
  # Appended AFTER NeuralAmpModelerCore/NAM so a bare #include "dsp.h" from our
  # own sources still resolves to NAM/dsp.h (nam::DSP). AudioDSPTools headers
  # (NoiseGate/Delay/Reverb/RecursiveLinearFilter) resolve their sibling "dsp.h"
  # via the quote-include current-directory rule.
  "${REPO_ROOT}/AudioDSPTools/dsp"
)

target_compile_definitions(volum PRIVATE
  NAM_ENABLE_A2_FAST
)

# Release: optimize for the phone. NAM inference is the hot path.
target_compile_options(volum PRIVATE
  $<$<CONFIG:Release>:-O3 -ffast-math -fno-finite-math-only>
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(volum
  oboe::oboe
  ${log-lib}
  ${android-lib}
)

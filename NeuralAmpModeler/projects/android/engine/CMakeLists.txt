cmake_minimum_required(VERSION 3.22)
project(VoLumAndroidEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(VOLUM_ROOT "${REPO_ROOT}/NeuralAmpModeler")
set(NAM_ROOT "${REPO_ROOT}/NeuralAmpModelerCore")
set(AUDIO_DSP_ROOT "${REPO_ROOT}/AudioDSPTools")
set(EIGEN_ROOT "${REPO_ROOT}/eigen")

foreach(required_dir IN ITEMS "${NAM_ROOT}/NAM" "${AUDIO_DSP_ROOT}/dsp" "${EIGEN_ROOT}/Eigen")
  if(NOT EXISTS "${required_dir}")
    message(FATAL_ERROR "missing required submodule content: ${required_dir}")
  endif()
endforeach()

file(GLOB NAM_SOURCES CONFIGURE_DEPENDS
  "${NAM_ROOT}/NAM/*.cpp"
  "${NAM_ROOT}/NAM/*/*.cpp"
)
set(AUDIO_DSP_SOURCES
  "${AUDIO_DSP_ROOT}/dsp/Delay.cpp"
  "${AUDIO_DSP_ROOT}/dsp/ImpulseResponse.cpp"
  "${AUDIO_DSP_ROOT}/dsp/NoiseGate.cpp"
  "${AUDIO_DSP_ROOT}/dsp/RecursiveLinearFilter.cpp"
  "${AUDIO_DSP_ROOT}/dsp/Reverb.cpp"
  "${AUDIO_DSP_ROOT}/dsp/dsp.cpp"
  "${AUDIO_DSP_ROOT}/dsp/wav.cpp"
)

add_library(volum_engine STATIC
  src/main/cpp/engine.cpp
  src/main/cpp/compile_surface.cpp
  "${VOLUM_ROOT}/ToneStack.cpp"
  ${NAM_SOURCES}
  ${AUDIO_DSP_SOURCES}
)
set_target_properties(volum_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(volum_engine PUBLIC NAM_ENABLE_A2_FAST EIGEN_MPL2_ONLY)
target_include_directories(volum_engine PUBLIC
  src/main/cpp
  "${VOLUM_ROOT}"
  "${NAM_ROOT}/NAM"
  "${NAM_ROOT}/Dependencies/nlohmann"
  "${AUDIO_DSP_ROOT}/dsp"
  "${EIGEN_ROOT}"
)
target_link_libraries(volum_engine PUBLIC Threads::Threads)

if(ANDROID)
  add_library(volum_engine_jni SHARED src/main/cpp/jni.cpp)
  target_link_libraries(volum_engine_jni PRIVATE "-Wl,--whole-archive" volum_engine "-Wl,--no-whole-archive")
else()
  add_executable(volum_engine_render src/main/cpp/cli.cpp)
  target_link_libraries(volum_engine_render PRIVATE "-Wl,--whole-archive" volum_engine "-Wl,--no-whole-archive")
endif()

cmake_minimum_required(VERSION 3.16)
project(volum_linux_leaf LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "volum_linux_leaf compiles the Linux iPlug2 leaf units.")
endif()

# compile-check -> linux -> scripts -> NeuralAmpModeler -> repo root
set(VOLUM_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../..)
set(IPLUG ${VOLUM_ROOT}/iPlug2)

add_library(volum_linux_leaf OBJECT
  ${IPLUG}/IPlug/IPlugPaths.cpp
  ${IPLUG}/IPlug/IPlugTimer.cpp
  ${IPLUG}/IPlug/Linux/IPlugLinuxRunLoop.cpp
  ${IPLUG}/IGraphics/Drawing/IGraphicsNanoVG.cpp
  ${IPLUG}/IGraphics/Platforms/IGraphicsLinuxFont.cpp
  ${IPLUG}/IGraphics/Platforms/IGraphicsLinuxGL.cpp
  ${IPLUG}/Dependencies/IGraphics/glad_GL2/src/glad.c
  include_leaf.cpp
)

target_compile_features(volum_linux_leaf PRIVATE cxx_std_17)
target_compile_definitions(volum_linux_leaf PRIVATE IGRAPHICS_NANOVG IGRAPHICS_GL2)
# iPlug2 headers use Apple's #pragma mark. Those lines stay; they are not Linux edits.
# Vendored headers are SYSTEM so their pre-existing -Wunused/-Wmultichar noise
# does not fail this leaf TU. Warnings in the Linux .cpp files stay -Werror.
target_compile_options(volum_linux_leaf PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas)

target_include_directories(volum_linux_leaf SYSTEM PRIVATE
  ${VOLUM_ROOT}/NeuralAmpModeler
  ${IPLUG}/IPlug
  ${IPLUG}/IPlug/APP
  ${IPLUG}/IPlug/Extras
  ${IPLUG}/IGraphics
  ${IPLUG}/IGraphics/Platforms
  ${IPLUG}/IGraphics/Drawing
  ${IPLUG}/IGraphics/Controls
  ${IPLUG}/WDL
  ${IPLUG}/Dependencies/IGraphics/NanoVG/src
  ${IPLUG}/Dependencies/IGraphics/NanoSVG/src
  ${IPLUG}/Dependencies/IGraphics/glad_GL2/include
  ${IPLUG}/Dependencies/IGraphics/STB
)

set_source_files_properties(
  ${IPLUG}/Dependencies/IGraphics/glad_GL2/src/glad.c
  PROPERTIES COMPILE_OPTIONS "-w"
)

# Quote-includes next to these translation units are not system headers, and the
# pre-existing bodies warn under GCC/Clang -Werror. The Linux edits in them are
# preprocessor branches; silencing these does not hide a warning in new code
# that lives in its own file.
set_source_files_properties(${IPLUG}/IPlug/IPlugPaths.cpp PROPERTIES COMPILE_OPTIONS
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-unused-function>"
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-unused-variable>"
  "$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-function>"
  "$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-variable>"
)
set_source_files_properties(${IPLUG}/IGraphics/Drawing/IGraphicsNanoVG.cpp PROPERTIES COMPILE_OPTIONS
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-unused-parameter>"
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-sequence-point>"
  "$<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>"
  "$<$<CXX_COMPILER_ID:Clang>:-Wno-unused-parameter>"
  "$<$<CXX_COMPILER_ID:Clang>:-Wno-unsequenced>"
)

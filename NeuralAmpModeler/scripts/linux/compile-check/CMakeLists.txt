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
target_compile_options(volum_linux_leaf PRIVATE -Wall -Wextra -Werror -Wno-unknown-pragmas)

target_include_directories(volum_linux_leaf PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${VOLUM_ROOT}/NeuralAmpModeler
  ${IPLUG}/IPlug
  ${IPLUG}/IPlug/APP
  ${IPLUG}/IGraphics
  ${IPLUG}/IGraphics/Platforms
  ${IPLUG}/IGraphics/Drawing
  ${IPLUG}/IGraphics/Controls
)

target_include_directories(volum_linux_leaf SYSTEM PRIVATE
  ${IPLUG}/WDL
  ${IPLUG}/Dependencies/IGraphics/NanoVG/src
  ${IPLUG}/Dependencies/IGraphics/NanoSVG/src
  ${IPLUG}/Dependencies/IGraphics/glad_GL2/include
)

set_source_files_properties(
  ${IPLUG}/Dependencies/IGraphics/glad_GL2/src/glad.c
  PROPERTIES COMPILE_OPTIONS "-w"
)

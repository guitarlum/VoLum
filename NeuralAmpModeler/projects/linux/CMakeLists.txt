# VoLum Linux VST3 and standalone.
#
# Resource layout (see copy-resources.cmake): files are loaded from disk, not
# embedded the way the Windows .rc embeds them. Ticket 05's LocateResource
# looks up fonts as type "ttf" and images by file extension, under
# Contents/Resources for the VST3 and resources/ next to the app.
# rigs/ is copied as VoLumRigs next to the app and inside the bundle Resources
# (spec Paths).
#
# JACK and Pulse are not linked. projects/linux/VoLumJackPulseDlopen.cpp
# defines the symbols RtAudio calls and dlopens libjack / libpulse-simple.
# ALSA, X11, GL, fontconfig and freetype are linked normally.
# libstdc++ and libgcc are static.

cmake_minimum_required(VERSION 3.16)

project(VoLumLinux LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This CMake build produces the Linux VST3 and standalone.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(VOLUM_NAM "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(VOLUM_ROOT "${VOLUM_NAM}/.." ABSOLUTE)
set(IPLUG "${VOLUM_ROOT}/iPlug2")
set(VST3_SDK "${IPLUG}/Dependencies/IPlug/VST3_SDK")

if(NOT EXISTS "${VST3_SDK}/public.sdk/source/main/linuxmain.cpp")
  message(FATAL_ERROR
    "VST3 SDK is missing linuxmain.cpp. Run NeuralAmpModeler/scripts/build-linux.sh, "
    "which clones the SDK (and keeps the validator sample) into ${VST3_SDK}.")
endif()

option(VOLUM_BUILD_VALIDATOR "Build the Steinberg VST3 validator from the vendored SDK" ON)
# ASan + UBSan for every target. With GCC the plug-in links libasan.so, so a
# host that is not sanitized itself (REAPER, pluginval) needs
# LD_PRELOAD="$(gcc -print-file-name=libasan.so)".
option(VOLUM_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(VOLUM_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

set(VOLUM_DSP_SOURCES
  "${VOLUM_ROOT}/AudioDSPTools/dsp/dsp.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/ImpulseResponse.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/Delay.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/Reverb.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/NoiseGate.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/RecursiveLinearFilter.cpp"
  "${VOLUM_ROOT}/AudioDSPTools/dsp/wav.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/activations.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/convnet.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/conv1d.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/container.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/dsp.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/get_dsp.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/lstm.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/ring_buffer.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/wavenet/model.cpp"
  "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM/wavenet/slimmable.cpp"
  "${VOLUM_NAM}/ToneStack.cpp"
  "${VOLUM_NAM}/VoLumHttpGet.cpp"
  "${VOLUM_NAM}/VoLumHttpGetLinux.cpp"
)

set(VOLUM_GFX_SOURCES
  "${IPLUG}/IGraphics/Controls/IControls.cpp"
  "${IPLUG}/IGraphics/Controls/IPopupMenuControl.cpp"
  "${IPLUG}/IGraphics/Controls/ITextEntryControl.cpp"
  "${IPLUG}/IGraphics/Drawing/IGraphicsNanoVG.cpp"
  "${IPLUG}/IGraphics/IControl.cpp"
  "${IPLUG}/IGraphics/IGraphics.cpp"
  "${IPLUG}/IGraphics/IGraphicsEditorDelegate.cpp"
  "${IPLUG}/IGraphics/Platforms/IGraphicsLinux.cpp"
  "${IPLUG}/IGraphics/Platforms/IGraphicsLinuxFont.cpp"
  "${IPLUG}/IGraphics/Platforms/IGraphicsLinuxGL.cpp"
  "${IPLUG}/IGraphics/Platforms/IGraphicsLinuxKeys.cpp"
  "${IPLUG}/IPlug/IPlugAPIBase.cpp"
  "${IPLUG}/IPlug/IPlugParameter.cpp"
  "${IPLUG}/IPlug/IPlugPluginBase.cpp"
  "${IPLUG}/IPlug/IPlugProcessor.cpp"
  "${IPLUG}/IPlug/IPlugTimer.cpp"
  "${IPLUG}/IPlug/Linux/IPlugLinuxRunLoop.cpp"
  "${IPLUG}/Dependencies/IGraphics/NanoVG/src/nanovg.c"
  "${IPLUG}/Dependencies/IGraphics/glad_GL2/src/glad.c"
)

set(VOLUM_VST3_SDK_SOURCES
  "${VST3_SDK}/base/source/baseiids.cpp"
  "${VST3_SDK}/base/source/fbuffer.cpp"
  "${VST3_SDK}/base/source/fdebug.cpp"
  "${VST3_SDK}/base/source/fdynlib.cpp"
  "${VST3_SDK}/base/source/fobject.cpp"
  "${VST3_SDK}/base/source/fstreamer.cpp"
  "${VST3_SDK}/base/source/fstring.cpp"
  "${VST3_SDK}/base/source/timer.cpp"
  "${VST3_SDK}/base/source/updatehandler.cpp"
  "${VST3_SDK}/base/thread/source/fcondition.cpp"
  "${VST3_SDK}/base/thread/source/flock.cpp"
  "${VST3_SDK}/pluginterfaces/base/conststringtable.cpp"
  "${VST3_SDK}/pluginterfaces/base/coreiids.cpp"
  "${VST3_SDK}/pluginterfaces/base/funknown.cpp"
  "${VST3_SDK}/pluginterfaces/base/ustring.cpp"
  "${VST3_SDK}/public.sdk/source/common/memorystream.cpp"
  "${VST3_SDK}/public.sdk/source/common/pluginview.cpp"
  "${VST3_SDK}/public.sdk/source/common/commoniids.cpp"
  "${VST3_SDK}/public.sdk/source/main/linuxmain.cpp"
  "${VST3_SDK}/public.sdk/source/main/pluginfactory.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstaudioeffect.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstbus.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstcomponent.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstcomponentbase.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstparameters.cpp"
  "${VST3_SDK}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
)

set(VOLUM_SWELL_SOURCES
  "${IPLUG}/WDL/swell/swell.cpp"
  "${IPLUG}/WDL/swell/swell-ini.cpp"
  "${IPLUG}/WDL/swell/swell-miscdlg-generic.cpp"
  "${IPLUG}/WDL/swell/swell-wnd-generic.cpp"
  "${IPLUG}/WDL/swell/swell-menu-generic.cpp"
  "${IPLUG}/WDL/swell/swell-kb-generic.cpp"
  "${IPLUG}/WDL/swell/swell-dlg-generic.cpp"
  "${IPLUG}/WDL/swell/swell-gdi-generic.cpp"
  "${IPLUG}/WDL/swell/swell-misc-generic.cpp"
  "${IPLUG}/WDL/swell/swell-generic-headless.cpp"
  "${IPLUG}/WDL/swell/swell-appstub-generic.cpp"
)

function(volum_includes target)
  target_include_directories(${target} PRIVATE
    "${VOLUM_NAM}"
    "${VOLUM_NAM}/resources"
    "${VOLUM_ROOT}/eigen"
    "${VOLUM_ROOT}/NeuralAmpModelerCore/NAM"
    "${VOLUM_ROOT}/AudioDSPTools/dsp"
    "${VOLUM_NAM}/third_party/signalsmith"
    "${IPLUG}/IPlug"
    "${IPLUG}/IPlug/APP"
    "${IPLUG}/IPlug/Extras"
    "${IPLUG}/IPlug/VST3"
    "${IPLUG}/IGraphics"
    "${IPLUG}/IGraphics/Controls"
    "${IPLUG}/IGraphics/Drawing"
    "${IPLUG}/IGraphics/Platforms"
    "${IPLUG}/WDL"
    "${IPLUG}/WDL/swell"
    "${IPLUG}/Dependencies/IGraphics/NanoVG/src"
    "${IPLUG}/Dependencies/IGraphics/NanoSVG/src"
    "${IPLUG}/Dependencies/IGraphics/glad_GL2/include"
    "${IPLUG}/Dependencies/IGraphics/glad_GL2/src"
    "${IPLUG}/Dependencies/IGraphics/STB"
    "${IPLUG}/Dependencies/Extras"
    "${IPLUG}/Dependencies/Extras/nlohmann"
    "${IPLUG}/Dependencies/IPlug/RTAudio"
    "${IPLUG}/Dependencies/IPlug/RTMidi"
    "${VST3_SDK}"
  )
endfunction()

function(volum_common_defs target)
  target_compile_definitions(${target} PRIVATE
    IGRAPHICS_NANOVG
    IGRAPHICS_GL2
    NAM_ENABLE_A2_FAST
    "GRAYED_ALPHA=0.5f"
    IPLUG_EDITOR=1
    IPLUG_DSP=1
    NOMINMAX
  )
  target_compile_options(${target} PRIVATE -Wno-unknown-pragmas)
endfunction()

add_library(volum_dsp OBJECT ${VOLUM_DSP_SOURCES})
volum_includes(volum_dsp)
target_compile_definitions(volum_dsp PRIVATE NAM_ENABLE_A2_FAST)

# IControl derives from Steinberg::FObject only under VST3_API, so these
# sources are compiled once per plug-in API. Sharing one object set between the
# VST3 and the app gives the two sides different IControl vtables.
foreach(api VST3_API APP_API)
  add_library(volum_gfx_${api} OBJECT ${VOLUM_GFX_SOURCES})
  volum_includes(volum_gfx_${api})
  volum_common_defs(volum_gfx_${api})
  target_compile_definitions(volum_gfx_${api} PRIVATE ${api})
endforeach()
set_source_files_properties(
  "${IPLUG}/Dependencies/IGraphics/glad_GL2/src/glad.c"
  "${IPLUG}/Dependencies/IGraphics/NanoVG/src/nanovg.c"
  PROPERTIES COMPILE_OPTIONS "-w"
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties("${IPLUG}/IGraphics/Drawing/IGraphicsNanoVG.cpp" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sequence-point;-Wno-maybe-uninitialized")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_source_files_properties("${IPLUG}/IGraphics/Drawing/IGraphicsNanoVG.cpp" PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-unsequenced;-Wno-unused-private-field")
endif()

set(VOLUM_VST3_BUNDLE "${CMAKE_BINARY_DIR}/VoLum.vst3")
set(VOLUM_APP_DIR "${CMAKE_BINARY_DIR}/standalone")

add_library(VoLum-vst3 SHARED
  ${VOLUM_VST3_SDK_SOURCES}
  "${IPLUG}/IPlug/IPlugPaths.cpp"
  "${IPLUG}/IPlug/VST3/IPlugVST3.cpp"
  "${IPLUG}/IPlug/VST3/IPlugVST3_ProcessorBase.cpp"
  "${VOLUM_NAM}/NeuralAmpModeler.cpp"
  $<TARGET_OBJECTS:volum_dsp>
  $<TARGET_OBJECTS:volum_gfx_VST3_API>
)
volum_includes(VoLum-vst3)
volum_common_defs(VoLum-vst3)
target_compile_definitions(VoLum-vst3 PRIVATE VST3_API)
set_target_properties(VoLum-vst3 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "VoLum"
  SUFFIX ".so"
  LIBRARY_OUTPUT_DIRECTORY "${VOLUM_VST3_BUNDLE}/Contents/x86_64-linux"
)
target_link_libraries(VoLum-vst3 PRIVATE
  -Wl,--no-as-needed
  X11
  GL
  fontconfig
  freetype
  -Wl,--as-needed
  ${CMAKE_DL_LIBS}
  m
  atomic
)
target_link_options(VoLum-vst3 PRIVATE
  -static-libstdc++
  -static-libgcc
  -pthread
  -Wl,--no-undefined
)

add_executable(VoLum-app
  "${IPLUG}/IPlug/APP/IPlugAPP.cpp"
  "${IPLUG}/IPlug/APP/IPlugAPP_dialog.cpp"
  "${IPLUG}/IPlug/APP/IPlugAPP_host.cpp"
  "${IPLUG}/IPlug/APP/IPlugAPP_main.cpp"
  "${IPLUG}/IPlug/IPlugPaths.cpp"
  "${IPLUG}/Dependencies/IPlug/RTAudio/RtAudio.cpp"
  "${IPLUG}/Dependencies/IPlug/RTMidi/RtMidi.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/VoLumJackPulseDlopen.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/VoLumSwellHeadless.cpp"
  ${VOLUM_SWELL_SOURCES}
  "${VOLUM_NAM}/NeuralAmpModeler.cpp"
  $<TARGET_OBJECTS:volum_dsp>
  $<TARGET_OBJECTS:volum_gfx_APP_API>
)
volum_includes(VoLum-app)
volum_common_defs(VoLum-app)
target_compile_definitions(VoLum-app PRIVATE APP_API)
set_source_files_properties("${IPLUG}/Dependencies/IPlug/RTAudio/RtAudio.cpp" PROPERTIES
  COMPILE_DEFINITIONS "__LINUX_ALSA__;__UNIX_JACK__;__LINUX_PULSE__")
set_source_files_properties("${IPLUG}/Dependencies/IPlug/RTMidi/RtMidi.cpp" PROPERTIES
  COMPILE_DEFINITIONS "__LINUX_ALSA__")
set_source_files_properties(${VOLUM_SWELL_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-Wno-multichar;-Wno-unused-function;-Wno-deprecated-declarations;-Wno-unused-parameter")
set_target_properties(VoLum-app PROPERTIES
  OUTPUT_NAME "VoLum"
  RUNTIME_OUTPUT_DIRECTORY "${VOLUM_APP_DIR}"
)
target_link_libraries(VoLum-app PRIVATE
  -Wl,--no-as-needed
  asound
  X11
  GL
  fontconfig
  freetype
  -Wl,--as-needed
  ${CMAKE_DL_LIBS}
  m
  atomic
)
target_link_options(VoLum-app PRIVATE
  -static-libstdc++
  -static-libgcc
  -pthread
  -Wl,--no-undefined
)

set(VOLUM_COPY_RESOURCES "${CMAKE_CURRENT_LIST_DIR}/copy-resources.cmake")
add_custom_command(TARGET VoLum-vst3 POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DRES=${VOLUM_NAM}/resources
    -DDEST=${VOLUM_VST3_BUNDLE}/Contents/Resources
    -DRIGS_SRC=${VOLUM_ROOT}/rigs
    -DRIGS_DEST=${VOLUM_VST3_BUNDLE}/Contents/Resources/VoLumRigs
    -P ${VOLUM_COPY_RESOURCES}
  COMMENT "Copy VST3 fonts, images and VoLumRigs"
)
add_custom_command(TARGET VoLum-app POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DRES=${VOLUM_NAM}/resources
    -DDEST=${VOLUM_APP_DIR}/resources
    -DRIGS_SRC=${VOLUM_ROOT}/rigs
    -DRIGS_DEST=${VOLUM_APP_DIR}/VoLumRigs
    -P ${VOLUM_COPY_RESOURCES}
  COMMENT "Copy standalone fonts, images and VoLumRigs"
)

if(VOLUM_BUILD_VALIDATOR)
  set(VOLUM_VALIDATOR_DIR "${VST3_SDK}/public.sdk/samples/vst-hosting/validator")
  if(NOT EXISTS "${VOLUM_VALIDATOR_DIR}/source/validator.cpp")
    message(FATAL_ERROR
      "VOLUM_BUILD_VALIDATOR is ON but ${VOLUM_VALIDATOR_DIR} is missing. "
      "build-linux.sh clones the SDK without deleting public.sdk/samples.")
  endif()

  # The SDK's own CMake requires 3.25. Ubuntu 22.04 ships 3.22, so the
  # validator is compiled from the same sources its CMakeLists lists, plus
  # the sdk_hosting / sdk_common / base pieces that list links.
  add_library(volum_vst3_validator_sdk STATIC
    "${VST3_SDK}/base/source/baseiids.cpp"
    "${VST3_SDK}/base/source/fbuffer.cpp"
    "${VST3_SDK}/base/source/fdebug.cpp"
    "${VST3_SDK}/base/source/fdynlib.cpp"
    "${VST3_SDK}/base/source/fobject.cpp"
    "${VST3_SDK}/base/source/fstreamer.cpp"
    "${VST3_SDK}/base/source/fstring.cpp"
    "${VST3_SDK}/base/source/timer.cpp"
    "${VST3_SDK}/base/source/updatehandler.cpp"
    "${VST3_SDK}/base/thread/source/fcondition.cpp"
    "${VST3_SDK}/base/thread/source/flock.cpp"
    "${VST3_SDK}/pluginterfaces/base/conststringtable.cpp"
    "${VST3_SDK}/pluginterfaces/base/coreiids.cpp"
    "${VST3_SDK}/pluginterfaces/base/funknown.cpp"
    "${VST3_SDK}/pluginterfaces/base/ustring.cpp"
    "${VST3_SDK}/public.sdk/source/common/commoniids.cpp"
    "${VST3_SDK}/public.sdk/source/common/commonstringconvert.cpp"
    "${VST3_SDK}/public.sdk/source/common/memorystream.cpp"
    "${VST3_SDK}/public.sdk/source/common/openurl.cpp"
    "${VST3_SDK}/public.sdk/source/common/readfile.cpp"
    "${VST3_SDK}/public.sdk/source/common/systemclipboard_linux.cpp"
    "${VST3_SDK}/public.sdk/source/common/threadchecker_linux.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/connectionproxy.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/eventlist.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/hostclasses.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/module.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/module_linux.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/parameterchanges.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/plugprovider.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/processdata.cpp"
    "${VST3_SDK}/public.sdk/source/vst/moduleinfo/moduleinfoparser.cpp"
    "${VST3_SDK}/public.sdk/source/vst/utility/stringconvert.cpp"
    "${VST3_SDK}/public.sdk/source/vst/utility/testing.cpp"
    "${VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp"
    "${VST3_SDK}/public.sdk/source/vst/vstpresetfile.cpp"
  )
  target_include_directories(volum_vst3_validator_sdk PUBLIC "${VST3_SDK}")
  target_compile_features(volum_vst3_validator_sdk PUBLIC cxx_std_17)
  target_compile_definitions(volum_vst3_validator_sdk PUBLIC RELEASE=1)
  target_compile_options(volum_vst3_validator_sdk PRIVATE -Wno-unknown-pragmas)

  file(GLOB VOLUM_VALIDATOR_TESTS
    "${VST3_SDK}/public.sdk/source/vst/testsuite/bus/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/general/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/noteexpression/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/processing/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/state/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/unit/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/testsuite/*.cpp"
    "${VST3_SDK}/public.sdk/source/vst/hosting/test/*.cpp"
  )
  add_executable(validator
    "${VOLUM_VALIDATOR_DIR}/source/main.cpp"
    "${VOLUM_VALIDATOR_DIR}/source/validator.cpp"
    "${VOLUM_VALIDATOR_DIR}/source/usediids.cpp"
    "${VST3_SDK}/public.sdk/source/main/moduleinit.cpp"
    ${VOLUM_VALIDATOR_TESTS}
  )
  target_include_directories(validator PRIVATE
    "${VOLUM_VALIDATOR_DIR}/source"
    "${VST3_SDK}"
  )
  target_compile_features(validator PRIVATE cxx_std_17)
  target_compile_definitions(validator PRIVATE RELEASE=1)
  target_compile_options(validator PRIVATE -Wno-unknown-pragmas)
  target_link_libraries(validator PRIVATE volum_vst3_validator_sdk ${CMAKE_DL_LIBS} pthread)
endif()

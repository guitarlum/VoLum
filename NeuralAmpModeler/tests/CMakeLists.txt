cmake_minimum_required(VERSION 3.20)

project(NeuralAmpModelerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(REPO_ROOT "${PROJECT_DIR}/..")

add_executable(NeuralAmpModeler-Tests
  test_main.cpp
  test_nam_rigs.cpp
  test_process_io.cpp
  test_volum_chunk_version.cpp
  test_volum_paths.cpp
  test_volum_settings_atomic_write.cpp
  test_volum_user_settings_io.cpp
  test_volum_ui_regressions.cpp
  test_volum_amp_catalog.cpp
  test_delay_reverb_dsp.cpp
  test_volum_pre_effects.cpp
  test_golden_dsp.cpp
  test_golden_rigs.cpp
  test_golden_presets.cpp
  test_keyboard_steps.cpp
  test_eparam_order.cpp
  test_tuner_dsp.cpp
  test_metronome_dsp.cpp
  test_tone_stack.cpp
  test_volum_pre_pedal_captures.cpp
  test_volum_processing_plan.cpp
  test_volum_ir_staging.cpp
  test_volum_master_safety.cpp
  test_volum_chunk_codec.cpp
  test_volum_pre_post_lock.cpp
  test_volum_nan_guard.cpp
  test_volum_bypass_identity.cpp
  test_volum_level_mute.cpp
  test_volum_knob_wheel.cpp
  test_iplug_app_vector_accumulator.cpp
  "${REPO_ROOT}/AudioDSPTools/dsp/dsp.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/Delay.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/Reverb.cpp"
  "${REPO_ROOT}/AudioDSPTools/dsp/RecursiveLinearFilter.cpp"
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

target_include_directories(NeuralAmpModeler-Tests PRIVATE
  "${PROJECT_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/third_party"
  "${REPO_ROOT}/eigen"
  "${REPO_ROOT}/NeuralAmpModelerCore/NAM"
  "${REPO_ROOT}/iPlug2/IPlug/APP"
  "${REPO_ROOT}/iPlug2/Dependencies/Extras"
  "${REPO_ROOT}/iPlug2/Dependencies/Extras/nlohmann"
)

target_compile_definitions(NeuralAmpModeler-Tests PRIVATE
  _CRT_SECURE_NO_DEPRECATE
  _CRT_NONSTDC_NO_DEPRECATE
)

if(APPLE)
  target_link_libraries(NeuralAmpModeler-Tests PRIVATE "-framework CoreFoundation")
endif()

if(VOLUM_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(NeuralAmpModeler-Tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(NeuralAmpModeler-Tests PRIVATE -fsanitize=address,undefined)
  else()
    message(FATAL_ERROR "VOLUM_ENABLE_SANITIZERS requires Clang")
  endif()
endif()

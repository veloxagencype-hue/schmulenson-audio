# SCHMU Chopper · sampler VST3/AU con los Vocal Chops de Schmulenson (JUCE + motor DSP propio sin dependencias).
# Misma configuración que SCHMU 90s.
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release -j
#   ctest --test-dir build -C Release --output-on-failure
#
# Opciones:
#   -DSCHMU_JUCE_DIR=/ruta/a/JUCE   usa un JUCE ya descargado (si no, lo baja con FetchContent, fijado a un tag)
#   -DSCHMU_BUILD_PLUGIN=OFF        compila solo el motor DSP, los tests y el render (no necesita JUCE ni red)
cmake_minimum_required(VERSION 3.22)
project(SCHMUChopper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SCHMU_BUILD_PLUGIN "Compilar el plugin (necesita JUCE)" ON)
option(SCHMU_BUILD_TESTS "Compilar los tests de DSP" ON)
set(SCHMU_JUCE_DIR "" CACHE PATH "Carpeta de JUCE ya descargada (opcional)")
set(SCHMU_JUCE_TAG "8.0.8" CACHE STRING "Tag de JUCE a descargar si no se da SCHMU_JUCE_DIR")

# ---------- Motor DSP (solo cabeceras, sin JUCE) ----------
add_library(schmu_dsp INTERFACE)
target_include_directories(schmu_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
if(MSVC)
  target_compile_options(schmu_dsp INTERFACE /W4 /permissive- /utf-8)
  target_compile_definitions(schmu_dsp INTERFACE NOMINMAX _USE_MATH_DEFINES)
else()
  target_compile_options(schmu_dsp INTERFACE -Wall -Wextra)
endif()

add_executable(schmu_render Tools/render_demo.cpp)
target_link_libraries(schmu_render PRIVATE schmu_dsp)

if(SCHMU_BUILD_TESTS)
  enable_testing()
  add_executable(schmu_dsp_tests Tests/dsp_tests.cpp)
  target_link_libraries(schmu_dsp_tests PRIVATE schmu_dsp)
  add_test(NAME dsp COMMAND schmu_dsp_tests)
endif()

# ---------- Plugin ----------
if(SCHMU_BUILD_PLUGIN)
  if(SCHMU_JUCE_DIR)
    add_subdirectory(${SCHMU_JUCE_DIR} juce EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(juce
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG ${SCHMU_JUCE_TAG}
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
  endif()

  set(SCHMU_FORMATS VST3 Standalone)
  if(APPLE)
    list(APPEND SCHMU_FORMATS AU)
  endif()

  juce_add_plugin(SCHMUChopper
    PRODUCT_NAME "SCHMU Chopper"
    COMPANY_NAME "Schmulenson"
    BUNDLE_ID com.schmulenson.schmuchopper
    PLUGIN_MANUFACTURER_CODE Schm
    PLUGIN_CODE Chop
    FORMATS ${SCHMU_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(SCHMUChopper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

  target_compile_definitions(SCHMUChopper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=1)   # ver LICENCIAS.md: depende de la licencia de JUCE que se use

  target_link_libraries(SCHMUChopper
    PRIVATE
      schmu_dsp
      juce::juce_audio_utils
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags)

  # Prueba de humo del plugin real (procesador + editor + presets + estado), se ejecuta con ctest.
  if(SCHMU_BUILD_TESTS)
    juce_add_console_app(schmu_plugin_smoke PRODUCT_NAME "schmu_plugin_smoke")
    target_sources(schmu_plugin_smoke PRIVATE Tests/plugin_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(schmu_plugin_smoke PRIVATE
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
      JUCE_STANDALONE_APPLICATION=1
      JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(schmu_plugin_smoke PRIVATE schmu_dsp juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_test(NAME plugin_smoke COMMAND schmu_plugin_smoke)
  endif()
endif()

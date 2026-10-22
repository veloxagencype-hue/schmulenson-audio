# SCHMU Analyzer. Licencia del código propio: AGPL-3.0-or-later (ver LICENSE en la raíz y LICENCIAS.md).
cmake_minimum_required(VERSION 3.22)
project(SchmuAnalyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

option(SCHMU_BUILD_PLUGIN "Compilar el plugin (descarga JUCE)" ON)
option(SCHMU_BUILD_TESTS "Compilar los tests de DSP (sin JUCE)" ON)
set(SCHMU_JUCE_DIR "" CACHE PATH "Carpeta de JUCE ya descargada (opcional)")
set(SCHMU_JUCE_TAG "8.0.8" CACHE STRING "Tag de JUCE (el mismo que SCHMU 90s / N12)")

# Tests de DSP: solo C++ estándar, corren en cualquier lado.
if(SCHMU_BUILD_TESTS)
  add_executable(schmu_dsp_tests Tests/dsp_tests.cpp)
  enable_testing()
  add_test(NAME dsp COMMAND schmu_dsp_tests)
endif()

if(SCHMU_BUILD_PLUGIN)
  if(SCHMU_JUCE_DIR)
    add_subdirectory(${SCHMU_JUCE_DIR} juce EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG ${SCHMU_JUCE_TAG}
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()

  set(SCHMU_FORMATS VST3)
  if(APPLE)
    list(APPEND SCHMU_FORMATS AU)
  endif()

  juce_add_plugin(SchmuAnalyzer
    COMPANY_NAME "Schmulenson"
    PLUGIN_MANUFACTURER_CODE Schm
    PLUGIN_CODE Sana
    FORMATS ${SCHMU_FORMATS}
    PRODUCT_NAME "SCHMU Analyzer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect)

  target_sources(SchmuAnalyzer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

  target_compile_definitions(SchmuAnalyzer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

  target_link_libraries(SchmuAnalyzer
    PRIVATE juce::juce_audio_utils juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

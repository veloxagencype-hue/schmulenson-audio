// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
#pragma once
#include "dsp/Correlation.h"
#include "dsp/Key.h"
#include "dsp/Loudness.h"
#include "dsp/Spectrum.h"
#include "dsp/TruePeak.h"
#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

// SCHMU Analyzer: pasa el audio sin tocarlo y mide. Todo el análisis corre en el hilo de audio;
// el editor lee los resultados por atómicos (sin bloqueos).
class SchmuAnalyzerProcessor : public juce::AudioProcessor
{
public:
    SchmuAnalyzerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SCHMU Analyzer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void resetMeasurements() { resetRequested = true; }

    juce::AudioProcessorValueTreeState apvts;

    // Resultados para la UI.
    static constexpr int numBands = schmu::SpectrumAnalyzer::numBands;
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, numBands> bandDb {}, peakDb {};
    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
    std::atomic<float> momentary { -200.f }, shortTerm { -200.f }, integrated { -200.f };
    std::atomic<float> maxMomentary { -200.f }, rmsDb { -200.f };
    std::atomic<float> truePeakDb { -180.f }, truePeakMaxDb { -180.f }, correlation { 1.f };
    std::atomic<float> hostBpm { 0.f };
    std::atomic<int> keyTonic { -1 }, keyMinor { 0 };
    std::atomic<float> keyConfidence { 0.f };
    std::atomic<int> overs { 0 }; // eventos: veces que el true peak cruzó -1 dBTP hacia arriba

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    schmu::LoudnessMeter loudness;
    schmu::TruePeakDetector tpL, tpR;
    schmu::CorrelationMeter corr;
    schmu::SpectrumAnalyzer spectrum;
    schmu::KeyDetector key;
    std::atomic<bool> resetRequested { false };
    int scopePos = 0, uiDecim = 0, tpDecayCount = 0;
    float tpHeld = 0.f, tpMax = 0.f;
    int oversCount = 0;
    bool tpOver = false; // true mientras el true peak está por encima de -1 dBTP (para contar eventos, no muestras)
    std::atomic<float>* slopeParam = nullptr; // se resuelve una vez en el constructor (no buscar por texto en el hilo de audio)
    double fs = 48000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchmuAnalyzerProcessor)
};

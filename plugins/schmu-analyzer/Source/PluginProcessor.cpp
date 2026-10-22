// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
#include "PluginProcessor.h"
#include "PluginEditor.h"

SchmuAnalyzerProcessor::SchmuAnalyzerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ESTADO", layout())
{
    slopeParam = apvts.getRawParameterValue ("slope");
}

juce::AudioProcessorValueTreeState::ParameterLayout SchmuAnalyzerProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "preset", 1 }, "Colores",
                                                         juce::StringArray { "Ácido", "Ámbar", "Hielo", "Mono" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "video", 1 }, "Modo video", false));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "slope", 1 }, "Pendiente dB/oct",
                                                        juce::NormalisableRange<float> (0.f, 6.f, 0.5f), 3.f));
    return l;
}

bool SchmuAnalyzerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
           && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
               || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void SchmuAnalyzerProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    loudness.prepare (fs);
    corr.prepare (fs);
    spectrum.prepare (fs);
    key.reset();
    tpL.reset(); tpR.reset();
    tpHeld = tpMax = 0.f; oversCount = 0; tpOver = false;
}

void SchmuAnalyzerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (resetRequested.exchange (false))
    {
        loudness.reset(); key.reset(); tpL.reset(); tpR.reset();
        tpHeld = tpMax = 0.f; oversCount = 0; tpOver = false;
        maxMomentary = -200.f; truePeakMaxDb = -180.f; overs = 0;
    }

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = (float) *bpm;

    const int n = buffer.getNumSamples();
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
    const float slope = slopeParam->load();
    spectrum.setSlopeDbPerOctave (slope);

    bool newFrame = false;
    for (int i = 0; i < n; ++i)
    {
        loudness.process (l[i], r[i]);
        corr.process (l[i], r[i]);
        const float p = std::max (tpL.process (l[i]), tpR.process (r[i]));
        tpHeld = std::max (p, tpHeld);
        const bool over = p > 0.891f; // > -1 dBTP
        if (over && ! tpOver) ++oversCount; // un evento por cruce hacia arriba, no una cuenta por muestra
        tpOver = over;
        newFrame |= spectrum.push (0.5f * (l[i] + r[i]));

        if ((++uiDecim & 7) == 0)
        {
            scopeL[(size_t) scopePos].store (l[i], std::memory_order_relaxed);
            scopeR[(size_t) scopePos].store (r[i], std::memory_order_relaxed);
            scopePos = (scopePos + 1) % scopeSize;
        }
    }

    if (newFrame)
    {
        key.accumulate (spectrum.magnitudes(), fs, schmu::SpectrumAnalyzer::fftSize);
        const auto& b = spectrum.bands();
        const auto& pk = spectrum.peaks();
        for (int k = 0; k < numBands; ++k) { bandDb[(size_t) k] = b[(size_t) k]; peakDb[(size_t) k] = pk[(size_t) k]; }
        const auto kr = key.estimate();
        keyTonic = kr.tonic; keyMinor = kr.minor ? 1 : 0; keyConfidence = (float) kr.confidence;
    }

    // True peak: valor de bloque con caída lenta; máximo hasta el reset.
    const float tpDb = schmu::TruePeakDetector::toDb (tpHeld);
    truePeakDb = tpDb;
    tpMax = std::max (tpMax, tpHeld);
    truePeakMaxDb = schmu::TruePeakDetector::toDb (tpMax);
    tpHeld *= std::pow (0.5f, (float) n / (float) (fs * 0.5)); // cae a la mitad cada 0,5 s
    overs = oversCount;

    momentary = (float) loudness.getMomentary();
    shortTerm = (float) loudness.getShortTerm();
    integrated = (float) loudness.getIntegrated();
    maxMomentary = (float) loudness.getMaxMomentary();
    rmsDb = (float) loudness.getRmsDb();
    correlation = (float) corr.get();
}

juce::AudioProcessorEditor* SchmuAnalyzerProcessor::createEditor() { return new SchmuAnalyzerEditor (*this); }

void SchmuAnalyzerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SchmuAnalyzerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SchmuAnalyzerProcessor(); }

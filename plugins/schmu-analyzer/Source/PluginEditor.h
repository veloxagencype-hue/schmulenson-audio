// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
#pragma once
#include "PluginProcessor.h"

class SchmuAnalyzerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SchmuAnalyzerEditor (SchmuAnalyzerProcessor&);
    ~SchmuAnalyzerEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Paleta { juce::Colour fondo, panel, acento, acento2, texto, tenue, alerta; };
    Paleta paleta() const;

    void timerCallback() override { repaint(); }
    void dibujarEspectro (juce::Graphics&, juce::Rectangle<float>, const Paleta&);
    void dibujarGonio (juce::Graphics&, juce::Rectangle<float>, const Paleta&);
    void dibujarCorrelacion (juce::Graphics&, juce::Rectangle<float>, const Paleta&);
    void dibujarMedidor (juce::Graphics&, juce::Rectangle<float>, const juce::String& rotulo, float valor, const juce::String& unidad, const Paleta&, bool grande, bool alerta = false);
    static juce::String fmt (float v, int dec = 1) { return v < -150.f ? juce::String ("-inf") : juce::String (v, dec); }

    SchmuAnalyzerProcessor& proc;
    juce::TextButton botonVideo { "VIDEO" }, botonReset { "RESET" };
    juce::ComboBox comboColores;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment adjColores;
    juce::AudioProcessorValueTreeState::ButtonAttachment adjVideo;
    juce::Rectangle<float> zonaEspectro, zonaGonio, zonaCorr, zonaMedidores;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchmuAnalyzerEditor)
};

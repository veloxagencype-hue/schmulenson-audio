// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
#include "PluginEditor.h"

SchmuAnalyzerEditor::SchmuAnalyzerEditor (SchmuAnalyzerProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      adjColores (p.apvts, "preset", comboColores),
      adjVideo (p.apvts, "video", botonVideo)
{
    comboColores.addItemList ({ "Ácido", "Ámbar", "Hielo", "Mono" }, 1);
    botonVideo.setClickingTogglesState (true);
    botonVideo.onClick = [this] { resized(); };
    botonReset.onClick = [this] { proc.resetMeasurements(); };
    addAndMakeVisible (botonVideo);
    addAndMakeVisible (botonReset);
    addAndMakeVisible (comboColores);

    setResizable (true, true);
    setResizeLimits (480, 270, 3840, 2160);
    setSize (900, 480);
    startTimerHz (30);
}

SchmuAnalyzerEditor::Paleta SchmuAnalyzerEditor::paleta() const
{
    const bool video = proc.apvts.getRawParameterValue ("video")->load() > 0.5f;
    const int preset = (int) proc.apvts.getRawParameterValue ("preset")->load();
    using juce::Colour;
    Paleta pal;
    pal.fondo = Colour (video ? 0xff000000 : 0xff0a0b10);
    pal.panel = Colour (video ? 0xff0c0d12 : 0xff12141c);
    pal.texto = Colour (0xfff4f5f8);
    pal.tenue = Colour (video ? 0xffa6aab8 : 0xff6c7080);
    pal.alerta = Colour (0xffff3b55);
    switch (preset)
    {
        case 1:  pal.acento = Colour (0xffffb000); pal.acento2 = Colour (0xffff5e1a); break; // Ámbar
        case 2:  pal.acento = Colour (0xff4de3ff); pal.acento2 = Colour (0xff7a8bff); break; // Hielo
        case 3:  pal.acento = Colour (0xffffffff); pal.acento2 = Colour (0xff9aa0ad); break; // Mono
        default: pal.acento = Colour (0xffc6ff00); pal.acento2 = Colour (0xff00e5a0); break; // Ácido
    }
    return pal;
}

void SchmuAnalyzerEditor::resized()
{
    auto area = getLocalBounds().toFloat().reduced (12.f);
    const bool video = proc.apvts.getRawParameterValue ("video")->load() > 0.5f;

    auto barra = area.removeFromTop (28.f);
    botonVideo.setBounds (barra.removeFromRight (70.f).toNearestInt());
    barra.removeFromRight (6.f);
    botonReset.setBounds (barra.removeFromRight (70.f).toNearestInt());
    barra.removeFromRight (6.f);
    comboColores.setBounds (barra.removeFromRight (100.f).toNearestInt());
    area.removeFromTop (8.f);

    if (video)
    {   // Modo video: espectro ancho arriba, cifras gigantes abajo.
        zonaMedidores = area.removeFromBottom (area.getHeight() * 0.38f);
        area.removeFromBottom (8.f);
        zonaEspectro = area;
        zonaGonio = zonaCorr = {};
    }
    else
    {
        zonaMedidores = area.removeFromRight (juce::jmin (260.f, area.getWidth() * 0.3f));
        area.removeFromRight (10.f);
        zonaCorr = area.removeFromBottom (22.f);
        area.removeFromBottom (8.f);
        zonaGonio = area.removeFromBottom (juce::jmin (120.f, area.getHeight() * 0.3f)).removeFromLeft (120.f);
        zonaEspectro = area;
    }
}

void SchmuAnalyzerEditor::dibujarEspectro (juce::Graphics& g, juce::Rectangle<float> r, const Paleta& pal)
{
    g.setColour (pal.panel);
    g.fillRoundedRectangle (r, 8.f);
    auto in = r.reduced (8.f, 6.f);
    in.removeFromBottom (14.f);

    // Rejilla: dB y frecuencias.
    g.setFont (juce::FontOptions (10.f));
    for (int db = -80; db <= 0; db += 20)
    {
        const float y = in.getY() + in.getHeight() * (float) (-db) / 90.f;
        g.setColour (pal.tenue.withAlpha (0.18f));
        g.drawHorizontalLine ((int) y, in.getX(), in.getRight());
        g.setColour (pal.tenue);
        g.drawText (juce::String (db), (int) in.getX() + 2, (int) y - 11, 30, 10, juce::Justification::left);
    }
    for (float f : { 50.f, 100.f, 500.f, 1000.f, 5000.f, 10000.f })
    {
        const float x = in.getX() + in.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
        g.setColour (pal.tenue.withAlpha (0.18f));
        g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
        g.setColour (pal.tenue);
        g.drawText (f >= 1000.f ? juce::String (f / 1000.f, 0) + "k" : juce::String ((int) f),
                    (int) x - 14, (int) in.getBottom() + 1, 28, 12, juce::Justification::centred);
    }

    const int nb = SchmuAnalyzerProcessor::numBands;
    auto yDe = [&] (float db) { return in.getY() + in.getHeight() * juce::jlimit (0.f, 1.f, -db / 90.f); };
    juce::Path relleno, pico;
    for (int b = 0; b < nb; ++b)
    {
        const float x = in.getX() + in.getWidth() * (float) b / (float) (nb - 1);
        const float y = yDe (proc.bandDb[(size_t) b].load (std::memory_order_relaxed));
        const float yp = yDe (proc.peakDb[(size_t) b].load (std::memory_order_relaxed));
        if (b == 0) { relleno.startNewSubPath (x, in.getBottom()); relleno.lineTo (x, y); pico.startNewSubPath (x, yp); }
        else { relleno.lineTo (x, y); pico.lineTo (x, yp); }
    }
    relleno.lineTo (in.getRight(), in.getBottom());
    relleno.closeSubPath();
    g.setGradientFill (juce::ColourGradient (pal.acento.withAlpha (0.85f), 0.f, in.getY(),
                                             pal.acento2.withAlpha (0.12f), 0.f, in.getBottom(), false));
    g.fillPath (relleno);
    g.setColour (pal.texto.withAlpha (0.7f));
    g.strokePath (pico, juce::PathStrokeType (1.2f));
}

void SchmuAnalyzerEditor::dibujarGonio (juce::Graphics& g, juce::Rectangle<float> r, const Paleta& pal)
{
    g.setColour (pal.panel);
    g.fillRoundedRectangle (r, 8.f);
    const auto c = r.getCentre();
    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.45f;
    g.setColour (pal.tenue.withAlpha (0.3f));
    g.drawLine (c.x - rad, c.y, c.x + rad, c.y);
    g.drawLine (c.x, c.y - rad, c.x, c.y + rad);
    g.setColour (pal.acento.withAlpha (0.75f));
    for (int i = 0; i < SchmuAnalyzerProcessor::scopeSize; ++i)
    {
        const float l = proc.scopeL[(size_t) i].load (std::memory_order_relaxed);
        const float rr = proc.scopeR[(size_t) i].load (std::memory_order_relaxed);
        const float m = (l + rr) * 0.7071f, s = (l - rr) * 0.7071f; // mid vertical, side horizontal
        g.fillRect (c.x + s * rad, c.y - m * rad, 1.5f, 1.5f);
    }
}

void SchmuAnalyzerEditor::dibujarCorrelacion (juce::Graphics& g, juce::Rectangle<float> r, const Paleta& pal)
{
    g.setColour (pal.panel);
    g.fillRoundedRectangle (r, 6.f);
    const float corr = proc.correlation.load();
    const float cx = r.getCentreX(), w = r.getWidth() / 2.f - 6.f;
    g.setColour (corr < 0.f ? pal.alerta : pal.acento);
    const float x = cx + corr * w;
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (cx, x), r.getY() + 5.f, std::abs (x - cx) + 2.f, r.getHeight() - 10.f), 3.f);
    g.setColour (pal.tenue);
    g.setFont (juce::FontOptions (10.f));
    g.drawText ("-1", r.toNearestInt().reduced (6, 0), juce::Justification::centredLeft);
    g.drawText ("+1", r.toNearestInt().reduced (6, 0), juce::Justification::centredRight);
    g.drawText ("CORR " + juce::String (corr, 2), r.toNearestInt(), juce::Justification::centred);
}

void SchmuAnalyzerEditor::dibujarMedidor (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& rotulo, float valor,
                                          const juce::String& unidad, const Paleta& pal, bool grande, bool alerta)
{
    g.setColour (pal.tenue);
    g.setFont (juce::FontOptions (grande ? r.getHeight() * 0.16f : 11.f, juce::Font::bold));
    g.drawText (rotulo, r.removeFromTop (grande ? r.getHeight() * 0.2f : 14.f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour (alerta ? pal.alerta : pal.texto);
    g.setFont (juce::FontOptions (grande ? r.getHeight() * 0.62f : 26.f, juce::Font::bold));
    g.drawText (fmt (valor), r.toNearestInt(), juce::Justification::centredLeft);
    g.setColour (pal.tenue);
    g.setFont (juce::FontOptions (grande ? r.getHeight() * 0.18f : 10.f));
    g.drawText (unidad, r.toNearestInt(), juce::Justification::bottomRight);
}

void SchmuAnalyzerEditor::paint (juce::Graphics& g)
{
    const auto pal = paleta();
    const bool video = proc.apvts.getRawParameterValue ("video")->load() > 0.5f;
    g.fillAll (pal.fondo);

    // Marca.
    g.setColour (pal.acento);
    g.setFont (juce::FontOptions (video ? 20.f : 16.f, juce::Font::bold));
    g.drawText ("SCHMULENSON", 12, 10, 260, 24, juce::Justification::centredLeft);
    g.setColour (pal.tenue);
    g.setFont (juce::FontOptions (11.f));
    g.drawText ("ANALYZER", 150, 14, 100, 16, juce::Justification::centredLeft);

    dibujarEspectro (g, zonaEspectro, pal);

    const float tp = proc.truePeakDb.load();
    const bool pasado = tp > -1.f;
    const int tonic = proc.keyTonic.load();
    schmu::KeyResult kr { tonic, proc.keyMinor.load() == 1, proc.keyConfidence.load() };
    const float bpm = proc.hostBpm.load();

    if (video)
    {   // Cifras gigantes en una fila: M / S / I / TP + marca/tonalidad/BPM.
        auto z = zonaMedidores;
        auto col = [&] (float frac) { return z.removeFromLeft (z.getWidth() * frac); };
        const float tot = z.getWidth();
        (void) tot;
        auto a = col (0.25f), b = col (0.333f), c = col (0.5f), d = col (1.f);
        dibujarMedidor (g, a.reduced (6.f), "MOMENTARY", proc.momentary.load(), "LUFS", pal, true);
        dibujarMedidor (g, b.reduced (6.f), "SHORT", proc.shortTerm.load(), "LUFS", pal, true);
        dibujarMedidor (g, c.reduced (6.f), "INTEGRATED", proc.integrated.load(), "LUFS", pal, true);
        dibujarMedidor (g, d.reduced (6.f), "TRUE PEAK", tp, "dBTP", pal, true, pasado);
        g.setColour (pal.acento);
        g.setFont (juce::FontOptions (zonaMedidores.getHeight() * 0.16f, juce::Font::bold));
        g.drawText ((tonic >= 0 ? kr.name() + "  " + kr.camelot() : juce::String ("--")) + (bpm > 0 ? "   " + juce::String (bpm, 1) + " BPM" : ""),
                    zonaMedidores.toNearestInt(), juce::Justification::topRight);
        return;
    }

    dibujarGonio (g, zonaGonio, pal);
    dibujarCorrelacion (g, zonaCorr, pal);

    auto z = zonaMedidores;
    const float h = z.getHeight() / 6.f;
    dibujarMedidor (g, z.removeFromTop (h), "MOMENTARY", proc.momentary.load(), "LUFS", pal, false);
    dibujarMedidor (g, z.removeFromTop (h), "SHORT-TERM", proc.shortTerm.load(), "LUFS", pal, false);
    dibujarMedidor (g, z.removeFromTop (h), "INTEGRATED", proc.integrated.load(), "LUFS", pal, false);
    dibujarMedidor (g, z.removeFromTop (h), "TRUE PEAK  (máx " + fmt (proc.truePeakMaxDb.load()) + ")", tp, "dBTP", pal, false, pasado);
    dibujarMedidor (g, z.removeFromTop (h), "RMS", proc.rmsDb.load(), "dBFS", pal, false);
    auto ultimo = z.removeFromTop (h);
    g.setColour (pal.tenue);
    g.setFont (juce::FontOptions (11.f, juce::Font::bold));
    g.drawText ("KEY / BPM", ultimo.removeFromTop (14.f).toNearestInt(), juce::Justification::centredLeft);
    g.setColour (pal.texto);
    g.setFont (juce::FontOptions (20.f, juce::Font::bold));
    g.drawText ((tonic >= 0 ? kr.name() + "  " + kr.camelot() : juce::String ("--")) + (bpm > 0 ? "   " + juce::String (bpm, 1) : ""),
                ultimo.toNearestInt(), juce::Justification::centredLeft);
}

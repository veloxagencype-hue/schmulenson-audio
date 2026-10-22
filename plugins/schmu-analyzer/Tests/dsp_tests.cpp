// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// Tests de DSP sin JUCE: LUFS EBU R128, true peak, correlación, espectro y tonalidad.
#include "../Source/dsp/Correlation.h"
#include "../Source/dsp/Key.h"
#include "../Source/dsp/Loudness.h"
#include "../Source/dsp/Spectrum.h"
#include "../Source/dsp/TruePeak.h"
#include <cstdio>
#include <cstdlib>

static int fallos = 0;
static void chequear (bool ok, const char* nombre, double valor)
{
    std::printf ("%s %-58s (%.3f)\n", ok ? "OK  " : "FALLA", nombre, valor);
    if (! ok) ++fallos;
}
static const double PI = 3.14159265358979323846;
static float senoDb (double f, double db, double fs, long n) { return (float) (std::pow (10.0, db / 20.0) * std::sin (2 * PI * f * n / fs)); }

int main()
{
    const double fs = 48000;

    { // EBU Tech 3341 caso 1: seno estéreo de 997 Hz a -23 dBFS → -23,0 LUFS
        schmu::LoudnessMeter m; m.prepare (fs);
        for (long n = 0; n < (long) (fs * 20); ++n) { const float s = senoDb (997, -23.0, fs, n); m.process (s, s); }
        chequear (std::fabs (m.getIntegrated() + 23.0) <= 0.1, "integrado seno 997 Hz -23 dBFS = -23,0 LUFS", m.getIntegrated());
        chequear (std::fabs (m.getMomentary() + 23.0) <= 0.1, "momentáneo = -23,0 LUFS", m.getMomentary());
        chequear (std::fabs (m.getShortTerm() + 23.0) <= 0.1, "corto = -23,0 LUFS", m.getShortTerm());
    }
    { // seno 0 dBFS mono en un canal → -3,01 LUFS (valor de referencia BS.1770)
        schmu::LoudnessMeter m; m.prepare (fs);
        for (long n = 0; n < (long) (fs * 10); ++n) m.process (senoDb (997, 0.0, fs, n), 0.0f);
        chequear (std::fabs (m.getIntegrated() + 3.01) <= 0.1, "seno 0 dBFS en un canal = -3,01 LUFS", m.getIntegrated());
    }
    { // 44,1 kHz da lo mismo (filtro K recalculado)
        schmu::LoudnessMeter m; m.prepare (44100);
        for (long n = 0; n < 44100L * 20; ++n) { const float s = senoDb (997, -23.0, 44100, n); m.process (s, s); }
        chequear (std::fabs (m.getIntegrated() + 23.0) <= 0.15, "integrado a 44,1 kHz = -23,0 LUFS", m.getIntegrated());
    }
    { // Compuerta relativa (Tech 3341 caso 3 simplificado): 20 s a -40 LUFS + 20 s a -23 LUFS → -23
        schmu::LoudnessMeter m; m.prepare (fs);
        for (long n = 0; n < (long) (fs * 20); ++n) { const float s = senoDb (997, -40.0, fs, n); m.process (s, s); }
        for (long n = 0; n < (long) (fs * 20); ++n) { const float s = senoDb (997, -23.0, fs, n); m.process (s, s); }
        chequear (std::fabs (m.getIntegrated() + 23.0) <= 0.1, "compuerta relativa ignora el tramo de -40 LUFS", m.getIntegrated());
    }
    { // Compuerta absoluta: silencio y señal bajo -70 LUFS no cuentan
        schmu::LoudnessMeter m; m.prepare (fs);
        for (long n = 0; n < (long) (fs * 5); ++n) m.process (senoDb (997, -90.0, fs, n), 0.0f);
        chequear (m.getIntegrated() < -150.0, "señal bajo -70 LUFS: integrado sin valor", m.getIntegrated());
        for (long n = 0; n < (long) (fs * 20); ++n) { const float s = senoDb (997, -23.0, fs, n); m.process (s, s); } // los bloques mixtos del borde pesan poco
        chequear (std::fabs (m.getIntegrated() + 23.0) <= 0.1, "compuerta absoluta descarta lo anterior", m.getIntegrated());
    }
    { // True peak: seno a fs/4 con fase 45° → muestras en ±0,707, pico real = 1,0 (0 dBTP)
        schmu::TruePeakDetector tp; float samplePeak = 0;
        for (long n = 0; n < 4800; ++n)
        {
            const float s = (float) std::sin (2 * PI * (fs / 4) * n / fs + PI / 4);
            samplePeak = std::max (samplePeak, std::fabs (s));
            tp.process (s);
        }
        const double d = schmu::TruePeakDetector::toDb (tp.getMax());
        chequear (std::fabs (samplePeak - 0.7071) < 0.001, "pico de muestra = -3,01 dBFS", samplePeak);
        chequear (std::fabs (d) <= 0.2, "true peak 4x detecta 0 dBTP (error ≤ 0,2 dB)", d);
    }
    { // True peak de seno 1 kHz a -6 dBFS ≈ -6 dBTP
        schmu::TruePeakDetector tp;
        for (long n = 0; n < 9600; ++n) tp.process (senoDb (1000, -6.0, fs, n));
        const double d = schmu::TruePeakDetector::toDb (tp.getMax());
        chequear (std::fabs (d + 6.0) <= 0.1, "true peak seno -6 dBFS = -6 dBTP", d);
    }
    { // Correlación
        schmu::CorrelationMeter c; c.prepare (fs);
        for (long n = 0; n < 48000; ++n) { const float s = senoDb (440, -10, fs, n); c.process (s, s); }
        chequear (c.get() > 0.99, "mono → correlación +1", c.get());
        c.reset();
        for (long n = 0; n < 48000; ++n) { const float s = senoDb (440, -10, fs, n); c.process (s, -s); }
        chequear (c.get() < -0.99, "fuera de fase → correlación -1", c.get());
        c.reset();
        for (long n = 0; n < 96000; ++n) c.process (senoDb (440, -10, fs, n), senoDb (440 * 1.5, -10, fs, n));
        chequear (std::fabs (c.get()) < 0.1, "tonos distintos → correlación ~0", c.get());
    }
    { // Espectro: pico en 1 kHz con nivel ≈ -6 dB (seno -6 dBFS)
        schmu::SpectrumAnalyzer sp; sp.prepare (fs);
        for (long n = 0; n < 48000; ++n) sp.push (senoDb (1000, -6.0, fs, n));
        int best = 0;
        for (int b = 0; b < schmu::SpectrumAnalyzer::numBands; ++b) if (sp.bands()[b] > sp.bands()[best]) best = b;
        const double f = schmu::SpectrumAnalyzer::bandFreq (best);
        chequear (f > 900 && f < 1100, "pico del espectro cerca de 1 kHz", f);
        chequear (std::fabs (sp.bands()[best] + 6.0) <= 1.5, "nivel del pico ≈ -6 dB", sp.bands()[best]);
    }
    { // Tonalidad: acorde de La menor (A C E) → A min / 8A ; acorde de Do mayor (C E G) → C maj / 8B
        struct Caso { double n1, n2, n3; const char* esperado; const char* camelot; };
        const Caso casos[] = { { 220.0, 261.63, 329.63, "A min", "8A" }, { 261.63, 329.63, 392.0, "C maj", "8B" } };
        for (const auto& c : casos)
        {
            schmu::SpectrumAnalyzer sp; sp.prepare (fs);
            schmu::KeyDetector kd;
            for (long n = 0; n < 48000 * 4; ++n)
            {
                const float s = senoDb (c.n1, -14, fs, n) + senoDb (c.n2, -14, fs, n) + senoDb (c.n3, -14, fs, n);
                if (sp.push (s)) kd.accumulate (sp.magnitudes(), fs, schmu::SpectrumAnalyzer::fftSize);
            }
            const auto k = kd.estimate();
            chequear (k.name() == c.esperado && k.camelot() == c.camelot, c.esperado, k.confidence);
        }
    }

    std::printf ("\n%s (%d fallos)\n", fallos ? "FALLÓ" : "TODO OK", fallos);
    return fallos ? 1 : 0;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// Tonalidad aproximada: cromagrama acumulado del espectro + perfiles de Krumhansl-Kessler.
#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace schmu
{
struct KeyResult
{
    int tonic = -1;      // 0 = C ... 11 = B; -1 = sin datos
    bool minor = false;
    double confidence = 0; // correlación (0..1)

    std::string name() const
    {
        static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return tonic < 0 ? "--" : std::string (n[tonic]) + (minor ? " min" : " maj");
    }

    // Notación Camelot (rueda armónica de DJ): 8B = C mayor, 8A = A menor.
    std::string camelot() const
    {
        if (tonic < 0) return "--";
        static const int major[] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 }; // por tónica mayor
        const int num = minor ? major[(tonic + 3) % 12] : major[tonic];
        return std::to_string (num) + (minor ? "A" : "B");
    }
};

class KeyDetector
{
public:
    void reset() { chroma.fill (0.0); }

    // Acumula un cuadro de magnitudes (dB por bin) con decaimiento lento.
    void accumulate (const std::vector<float>& magDb, double fs, int fftSize, double decay = 0.97)
    {
        std::array<double, 12> c {};
        for (int k = 1; k < (int) magDb.size(); ++k)
        {
            const double f = k * fs / fftSize;
            if (f < 65.0 || f > 2100.0) continue;
            const double amp = std::pow (10.0, magDb[k] / 20.0);
            const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
            c[pc] += amp * amp; // energía
        }
        for (int i = 0; i < 12; ++i) chroma[i] = chroma[i] * decay + c[i];
    }

    KeyResult estimate() const
    {
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double min[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double total = 0;
        for (double v : chroma) total += v;
        KeyResult best;
        if (total < 1e-9) return best;
        double bestR = -2;
        for (int minor = 0; minor < 2; ++minor)
            for (int t = 0; t < 12; ++t)
            {
                double mx = 0, my = 0;
                for (int i = 0; i < 12; ++i) { mx += chroma[(t + i) % 12]; my += minor ? min[i] : maj[i]; }
                mx /= 12; my /= 12;
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const double dx = chroma[(t + i) % 12] - mx, dy = (minor ? min[i] : maj[i]) - my;
                    sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
                }
                const double r = sxy / std::sqrt (sxx * syy + 1e-30);
                if (r > bestR) { bestR = r; best = { t, minor == 1, std::max (0.0, r) }; }
            }
        return best;
    }

private:
    std::array<double, 12> chroma {};
};
} // namespace schmu

// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// True peak (ITU-R BS.1770-4 Anexo 2): sobremuestreo 4x con FIR polifásico (sinc con ventana Kaiser).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace schmu
{
class TruePeakDetector
{
public:
    TruePeakDetector() { designFilter(); reset(); }

    void reset() { hist.fill (0.0f); pos = 0; peak = 0; }

    // Devuelve el pico de las 4 muestras interpoladas (lineal, absoluto).
    float process (float x)
    {
        pos = (pos + 1) % taps;
        hist[pos] = x;
        float blockPeak = std::fabs (x);
        for (int ph = 1; ph < 4; ++ph)
        {
            float acc = 0;
            for (int k = 0; k < taps; ++k)
                acc += coef[ph][k] * hist[(pos - k + taps) % taps];
            blockPeak = std::max (blockPeak, std::fabs (acc));
        }
        peak = std::max (peak, blockPeak);
        return blockPeak;
    }

    float getMax() const { return peak; }
    static float toDb (float lin) { return lin > 1e-9f ? 20.0f * std::log10 (lin) : -180.0f; }

private:
    static constexpr int taps = 24; // por fase

    static double bessel0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
        return s;
    }

    void designFilter()
    {
        const double pi = 3.14159265358979323846, beta = 8.0;
        const double center = 2.0 * taps; // filtro de 4*taps+1 coef., centro en múltiplo de 4
        for (int ph = 0; ph < 4; ++ph)
            for (int k = 0; k < taps; ++k)
            {
                const double n = k * 4 + ph; // polifásica: h[4k+ph]
                const double t = (n - center) / 4.0;
                const double sinc = std::fabs (t) < 1e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double r = (n - center) / center;
                const double win = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
                coef[ph][k] = (float) (sinc * win);
            }
    }

    // La muestra original coincide con el centro del filtro: fase 0 se usa directa.
    std::array<std::array<float, taps>, 4> coef {};
    std::array<float, taps> hist {};
    int pos = 0;
    float peak = 0;
};
} // namespace schmu

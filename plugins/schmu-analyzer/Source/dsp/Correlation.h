// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// Correlación estéreo (-1..+1) y posición para el goniómetro, con suavizado exponencial.
#pragma once
#include <cmath>

namespace schmu
{
class CorrelationMeter
{
public:
    void prepare (double fs, double tauSeconds = 0.3)
    {
        alpha = 1.0 - std::exp (-1.0 / (fs * tauSeconds));
        reset();
    }
    void reset() { ll = rr = lr = 0; }

    void process (float l, float r)
    {
        ll += alpha * ((double) l * l - ll);
        rr += alpha * ((double) r * r - rr);
        lr += alpha * ((double) l * r - lr);
    }

    // 1 = mono, 0 = descorrelado, -1 = fuera de fase. En silencio devuelve 1.
    double get() const
    {
        const double d = std::sqrt (ll * rr);
        return d < 1e-12 ? 1.0 : std::fmax (-1.0, std::fmin (1.0, lr / d));
    }

private:
    double alpha = 0.0001, ll = 0, rr = 0, lr = 0;
};
} // namespace schmu

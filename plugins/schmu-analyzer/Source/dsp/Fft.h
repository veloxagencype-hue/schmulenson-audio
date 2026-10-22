// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// FFT radix-2 propia (sin dependencias) para poder testear el DSP sin JUCE.
#pragma once
#include <cmath>
#include <complex>
#include <vector>

namespace schmu
{
class Fft
{
public:
    explicit Fft (int order) : n (1 << order), twiddle (n / 2), rev (n)
    {
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < n / 2; ++i)
            twiddle[i] = std::polar (1.0f, (float) (-2.0 * pi * i / n));
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[i] = r;
        }
    }

    int size() const { return n; }

    // Transformada directa in-place.
    void forward (std::vector<std::complex<float>>& x) const
    {
        for (int i = 0; i < n; ++i)
            if (rev[i] > i) std::swap (x[i], x[rev[i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const auto t = x[i + k + half] * twiddle[k * step];
                    x[i + k + half] = x[i + k] - t;
                    x[i + k] += t;
                }
        }
    }

private:
    int n;
    std::vector<std::complex<float>> twiddle;
    std::vector<int> rev;
};
} // namespace schmu

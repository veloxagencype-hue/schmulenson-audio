// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// Analizador de espectro: FFT con ventana Hann, mapeo a bandas logarítmicas (20 Hz–20 kHz),
// suavizado (ataque rápido / caída lenta) y pico retenido con caída.
#pragma once
#include "Fft.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace schmu
{
class SpectrumAnalyzer
{
public:
    static constexpr int order = 12;          // FFT de 4096
    static constexpr int fftSize = 1 << order;
    static constexpr int hop = 1024;
    static constexpr int numBands = 256;

    SpectrumAnalyzer() : fft (order), window (fftSize), ring (fftSize, 0.0f), buf (fftSize)
    {
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < fftSize; ++i)
            window[i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / fftSize));
        bandDb.assign (numBands, floorDb);
        peakDb.assign (numBands, floorDb);
        magDb.assign (fftSize / 2, floorDb);
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (bandDb.begin(), bandDb.end(), floorDb);
        std::fill (peakDb.begin(), peakDb.end(), floorDb);
        writePos = sinceHop = 0;
        // Para cada banda log: rango de bins FFT.
        lo.assign (numBands, 0); hi.assign (numBands, 0);
        for (int b = 0; b < numBands; ++b)
        {
            const double f0 = bandFreq (b), f1 = bandFreq (b + 1);
            lo[b] = std::clamp ((int) std::floor (f0 * fftSize / fs), 1, fftSize / 2 - 1);
            hi[b] = std::clamp ((int) std::ceil (f1 * fftSize / fs), lo[b], fftSize / 2 - 1);
        }
    }

    // Empuja una muestra mono. Devuelve true cuando se calculó un cuadro nuevo.
    bool push (float mono)
    {
        ring[writePos] = mono;
        writePos = (writePos + 1) % fftSize;
        if (++sinceHop < hop) return false;
        sinceHop = 0;
        analyse();
        return true;
    }

    static double bandFreq (int b) { return 20.0 * std::pow (1000.0, (double) b / numBands); } // 20 Hz → 20 kHz

    const std::vector<float>& bands() const { return bandDb; }
    const std::vector<float>& peaks() const { return peakDb; }
    const std::vector<float>& magnitudes() const { return magDb; } // dB por bin (para el cromagrama)
    double sampleRate() const { return fs; }
    void setSlopeDbPerOctave (float s) { slope = s; }

    static constexpr float floorDb = -100.0f;

private:
    void analyse()
    {
        auto& x = buf; // preasignado en el constructor: nada de memoria dinámica en el hilo de audio
        for (int i = 0; i < fftSize; ++i)
            x[i] = { ring[(writePos + i) % fftSize] * window[i], 0.0f };
        fft.forward (x);

        // Normalización: una senoide de 0 dBFS da ~0 dB (Hann: ganancia coherente 0,5).
        const float norm = 2.0f / (0.5f * fftSize);
        for (int k = 0; k < fftSize / 2; ++k)
            magDb[k] = std::max (floorDb, 20.0f * std::log10 (std::abs (x[k]) * norm + 1e-9f));

        const float hopSec = (float) hop / (float) fs;
        for (int b = 0; b < numBands; ++b)
        {
            float m = floorDb;
            for (int k = lo[b]; k <= hi[b]; ++k) m = std::max (m, magDb[k]);
            m += slope * (float) std::log2 (bandFreq (b) / 1000.0); // compensación de pendiente (ej. +4,5 dB/oct)
            // Suavizado: ataque casi instantáneo, caída ~ 30 dB/s.
            const float fall = bandDb[b] - 30.0f * hopSec;
            bandDb[b] = std::max (m, fall);
            // Pico retenido: se mantiene y cae ~ 12 dB/s.
            peakDb[b] = std::max (bandDb[b], peakDb[b] - 12.0f * hopSec);
        }
    }

    Fft fft;
    std::vector<float> window, ring;
    std::vector<std::complex<float>> buf;
    std::vector<float> bandDb, peakDb, magDb;
    std::vector<int> lo, hi;
    double fs = 48000;
    int writePos = 0, sinceHop = 0;
    float slope = 0.0f;
};
} // namespace schmu

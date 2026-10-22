// SPDX-License-Identifier: AGPL-3.0-or-later
// SCHMU Analyzer — Copyright (C) 2026 Tomás Schmulenson. Software libre bajo AGPLv3 o posterior: ver LICENSE.
// Medidor de sonoridad ITU-R BS.1770-4 / EBU R128: filtro K, momentáneo (400 ms), corto (3 s)
// e integrado con compuerta absoluta (-70 LUFS) y relativa (-10 LU).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace schmu
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// Filtro K para cualquier frecuencia de muestreo (etapa 1: shelf agudo; etapa 2: pasa-altos RLB).
struct KFilter
{
    Biquad shelf, hp;

    void prepare (double fs)
    {
        const double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
            hp.a1 = 2.0 * (K * K - 1.0) / a0;
            hp.a2 = (1.0 - K / Q + K * K) / a0;
        }
        reset();
    }
    void reset() { shelf.reset(); hp.reset(); }
    double process (double x) { return hp.process (shelf.process (x)); }
};

class LoudnessMeter
{
public:
    static constexpr double silence = -200.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        subLen = (int) std::lround (fs * 0.1);
        kL.prepare (fs);
        kR.prepare (fs);
        reset();
    }

    void reset()
    {
        kL.reset(); kR.reset();
        subSum = 0; subCount = 0;
        subHead = subN = 0;
        subs.fill (0.0);
        if (histCount.size() != (size_t) histBins) // solo la primera vez (prepare); el RESET en el hilo de audio no asigna memoria
        {
            histCount.assign ((size_t) histBins, 0);
            histEnergy.assign ((size_t) histBins, 0.0);
        }
        else
        {
            std::fill (histCount.begin(), histCount.end(), 0);
            std::fill (histEnergy.begin(), histEnergy.end(), 0.0);
        }
        momentary = shortTerm = integrated = silence;
        maxMomentary = maxShort = silence;
        rmsAcc = 0; rmsDb = silence;
    }

    // Procesa un par de muestras (mono: pasar la misma en l y r).
    void process (float l, float r)
    {
        const double fl = kL.process (l), fr = kR.process (r);
        subSum += fl * fl + fr * fr;
        rmsAcc += 0.5 * ((double) l * l + (double) r * r);
        if (++subCount >= subLen)
            closeSubBlock();
    }

    double getMomentary() const { return momentary; }
    double getShortTerm() const { return shortTerm; }
    double getIntegrated() const { return integrated; }
    double getMaxMomentary() const { return maxMomentary; }
    double getMaxShortTerm() const { return maxShort; }
    double getRmsDb() const { return rmsDb; }

    static double toLufs (double energy) { return energy > 0 ? -0.691 + 10.0 * std::log10 (energy) : silence; }

private:
    static constexpr int histBins = 8000; // 0,01 LU entre -70 y +10 LUFS
    static int binOf (double lufs) { return std::clamp ((int) ((lufs + 70.0) * 100.0), 0, histBins - 1); }

    void closeSubBlock()
    {
        const double e = subSum / subLen;
        subs[(size_t) subHead] = e; // anillo de 30 sub-bloques de 100 ms (sin memoria dinámica)
        subHead = (subHead + 1) % 30;
        if (subN < 30) ++subN;
        rmsDb = rmsAcc > 0 ? 10.0 * std::log10 (rmsAcc / subLen) : silence;
        subSum = 0; rmsAcc = 0; subCount = 0;

        if (subN >= 4)
        {
            double m = 0;
            for (int i = 1; i <= 4; ++i) m += subAt (i);
            m /= 4.0;
            momentary = toLufs (m);
            maxMomentary = std::max (maxMomentary, momentary);
            if (momentary > -70.0) // compuerta absoluta
            {
                const int b = binOf (momentary);
                histCount[b]++;
                histEnergy[b] += m;
            }
        }
        if (subN > 0)
        {
            double s = 0;
            for (int i = 1; i <= subN; ++i) s += subAt (i);
            shortTerm = subN >= 30 ? toLufs (s / 30.0) : silence;
            maxShort = std::max (maxShort, shortTerm);
        }
        updateIntegrated();
    }

    // i = 1 es el sub-bloque más reciente.
    double subAt (int i) const { return subs[(size_t) ((subHead - i + 30) % 30)]; }

    void updateIntegrated()
    {
        double sum = 0; long long cnt = 0;
        for (int i = 0; i < histBins; ++i) { sum += histEnergy[i]; cnt += histCount[i]; }
        if (cnt == 0) { integrated = silence; return; }
        const double relThr = toLufs (sum / (double) cnt) - 10.0; // compuerta relativa
        sum = 0; cnt = 0;
        for (int i = binOf (relThr); i < histBins; ++i)
            if (-70.0 + i * 0.01 >= relThr - 1e-9) { sum += histEnergy[i]; cnt += histCount[i]; }
        integrated = cnt ? toLufs (sum / (double) cnt) : silence;
    }

    double fs = 48000;
    int subLen = 4800, subCount = 0;
    double subSum = 0, rmsAcc = 0;
    KFilter kL, kR;
    std::array<double, 30> subs {};
    int subHead = 0, subN = 0;
    std::vector<long long> histCount;
    std::vector<double> histEnergy;
    double momentary = silence, shortTerm = silence, integrated = silence;
    double maxMomentary = silence, maxShort = silence, rmsDb = silence;
};
} // namespace schmu

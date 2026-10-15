// ResonatorDSP.h
// Pure C++17 (no JUCE dependency) model of a 3-band resonant state-variable filter
// bank, fitted to measured frequency-response captures of a hardware 3-channel
// resonant filter (Polymoog-style "Resonator").
//
// Structure (one instance per audio channel):
//   3 x 2-pole SVF (LF / MF / HF).  Each SVF gives LP, BP (un-normalised, peak = Q)
//   and HP outputs.  A mode selects which outputs are summed:
//        LPF  : sum of LP outputs
//        BPF1 : LF.BP - MF.BP + HF.BP   (inverted mid band, smooth "formant" shape)
//        BPF2 : LF.BP + MF.BP + HF.BP   (all in phase, deep notches between bands)
//        HPF  : sum of HP outputs
//   Fitted constants live in namespace resonator::fit.
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <complex>

namespace resonator
{
constexpr double kPi = 3.14159265358979323846;

enum class Mode { LPF = 0, BPF1 = 1, BPF2 = 2, HPF = 3 };
enum class FreqLaw { Analog = 0, Exponential = 1 };

namespace fit
{
    // Frequency range of each band's FREQ knob (Hz), measured.
    constexpr std::array<double, 3> fMin { 61.0, 327.5, 1525.0 };
    constexpr std::array<double, 3> fMax { 304.4, 1235.8, 7526.0 };

    // Q measured at emphasis = 0 %, 50 %, 100 % (per band).
    constexpr std::array<std::array<double, 3>, 3> qAt {{
        { 0.527, 2.05, 8.71 },    // LF
        { 0.901, 3.83, 13.80 },   // MF
        { 0.596, 2.27, 11.30 }    // HF
    }};

    // Output mix gains per band for each SVF output.
    constexpr std::array<double, 3> gLP { 0.5062, 0.5147, 0.5646 };
    constexpr std::array<double, 3> gBP { 0.7379, 0.5161, 0.7221 };
    constexpr std::array<double, 3> gHP { 0.4752, 0.4652, 0.4538 };

    // Extra one-pole roll-off seen on the LP path only (Hz).
    constexpr double lpExtraPole = 13263.0;
}

// ---- parameter mapping helpers (also used by the unit test) -------------------
inline double knobToFreq (int band, double t, FreqLaw law)
{
    t = std::clamp (t, 0.0, 1.0);
    const double lo = fit::fMin[(size_t) band], hi = fit::fMax[(size_t) band];
    const double r = hi / lo;
    if (law == FreqLaw::Exponential) return lo * std::pow (r, t);
    // Linear pot in series with a fixed resistor: f ~ 1 / (R_fixed + R_pot)
    return hi / (1.0 + (r - 1.0) * (1.0 - t));
}

inline double emphasisToQ (int band, double e)
{
    e = std::clamp (e, 0.0, 1.0);
    const auto& q = fit::qAt[(size_t) band];
    const double l0 = std::log (q[0]), lm = std::log (q[1]), l1 = std::log (q[2]);
    // quadratic through (0,l0) (0.5,lm) (1,l1) in the log domain
    const double lq = l0 * (1 - e) * (1 - 2 * e) + lm * 4 * e * (1 - e) + l1 * e * (2 * e - 1);
    return std::exp (lq);
}

// ---- one band: ZDF / TPT state variable filter --------------------------------
struct SVF
{
    double ic1 = 0, ic2 = 0;
    double a1 = 0, a2 = 0, a3 = 0, k = 1;

    void setCoeffs (double f0, double Q, double fs)
    {
        const double fc = std::min (f0, 0.49 * fs);
        const double g = std::tan (kPi * fc / fs);
        k = 1.0 / std::max (Q, 0.05);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    // returns lp, bp (peak = Q), hp
    inline void tick (double v0, double& lp, double& bp, double& hp)
    {
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

struct OnePoleLP
{
    double s = 0, G = 0;
    void set (double fc, double fs) { const double g = std::tan (kPi * std::min (fc, 0.49 * fs) / fs); G = g / (1 + g); }
    void reset() { s = 0; }
    inline double tick (double x) { const double v = (x - s) * G; const double y = v + s; s = y + v; return y; }
};

// ---- full 3-band filter for ONE audio channel ---------------------------------
class ResonatorChannel
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        smoothCoef = 1.0 - std::exp (-1.0 / (0.010 * fs));   // ~10 ms
        reset();
        firstParams = true;      // next setParams() jumps straight to its targets
        updateCoeffs();
        lpPole.set (fit::lpExtraPole, fs);
    }
    void reset()
    {
        for (auto& b : band) b.reset();
        lpPole.reset();
    }

    // knob values are all normalised 0..1
    void setParams (const std::array<double, 3>& freqKnob, double emphasis, Mode m, FreqLaw law)
    {
        for (int i = 0; i < 3; ++i)
        {
            tgtFreq[(size_t) i] = knobToFreq (i, freqKnob[(size_t) i], law);
            tgtQ[(size_t) i]    = emphasisToQ (i, emphasis);
        }
        for (int i = 0; i < 3; ++i)
        {
            const double sgn = (m == Mode::BPF1 && i == 1) ? -1.0 : 1.0;
            tgtW[(size_t) i][0] = (m == Mode::LPF)  ? fit::gLP[(size_t) i] : 0.0;
            tgtW[(size_t) i][1] = (m == Mode::BPF1 || m == Mode::BPF2) ? sgn * fit::gBP[(size_t) i] : 0.0;
            tgtW[(size_t) i][2] = (m == Mode::HPF)  ? fit::gHP[(size_t) i] : 0.0;
        }
        if (firstParams) { firstParams = false; snap(); }
    }

    // jump straight to the targets (no smoothing) – call after setParams on first use
    void snap()
    {
        for (size_t i = 0; i < 3; ++i) { curFreq[i] = tgtFreq[i]; curQ[i] = tgtQ[i]; curW[i] = tgtW[i]; }
        updateCoeffs();
        lpPole.set (fit::lpExtraPole, fs);
    }

    inline float process (float in)
    {
        if (++coefCounter >= kCoefInterval) { coefCounter = 0; smoothStep(); updateCoeffs(); }
        double sumLP = 0, sumOther = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            double lp, bp, hp;
            band[i].tick ((double) in, lp, bp, hp);
            sumLP    += curW[i][0] * lp;
            sumOther += curW[i][1] * bp + curW[i][2] * hp;
        }
        const double y = lpPole.tick (sumLP) + sumOther;
        if (! std::isfinite (y)) { reset(); return 0.0f; }   // safety net: never let a NaN/Inf latch
        return (float) y;
    }

private:
    static constexpr int kCoefInterval = 8;
    double fs = 44100.0, smoothCoef = 0.01;
    int coefCounter = 0;
    std::array<SVF, 3> band;
    OnePoleLP lpPole;
    bool firstParams = true;
    std::array<double, 3> tgtFreq { 300.0, 1000.0, 5000.0 }, curFreq { 300.0, 1000.0, 5000.0 },
                          tgtQ { 1.0, 1.0, 1.0 }, curQ { 1.0, 1.0, 1.0 };
    std::array<std::array<double, 3>, 3> tgtW {}, curW {};

    void smoothStep()
    {
        // the step is taken every kCoefInterval samples, so scale the coefficient
        const double c = 1.0 - std::pow (1.0 - smoothCoef, (double) kCoefInterval);
        for (size_t i = 0; i < 3; ++i)
        {
            curFreq[i] += c * (tgtFreq[i] - curFreq[i]);
            curQ[i]    += c * (tgtQ[i] - curQ[i]);
            for (size_t j = 0; j < 3; ++j) curW[i][j] += c * (tgtW[i][j] - curW[i][j]);
        }
    }
    void updateCoeffs()
    {
        for (size_t i = 0; i < 3; ++i) band[i].setCoeffs (curFreq[i], curQ[i], fs);
    }
};

// ---- analytic reference response of the *analog* prototype (for tests) --------
inline double analogMagnitudeDb (double f, const std::array<double, 3>& freqKnob, double emphasis,
                                 Mode m, FreqLaw law)
{
    using C = std::complex<double>;
    C out = 0;
    for (int i = 0; i < 3; ++i)
    {
        const double f0 = knobToFreq (i, freqKnob[(size_t) i], law), Q = emphasisToQ (i, emphasis);
        const C s (0.0, f / f0);
        const C den = s * s + s / Q + 1.0;
        if (m == Mode::LPF)  out += fit::gLP[(size_t) i] / den;
        if (m == Mode::BPF1) out += (i == 1 ? -1.0 : 1.0) * fit::gBP[(size_t) i] * s / den;
        if (m == Mode::BPF2) out += fit::gBP[(size_t) i] * s / den;
        if (m == Mode::HPF)  out += fit::gHP[(size_t) i] * s * s / den;
    }
    if (m == Mode::LPF) out /= C (1.0, f / fit::lpExtraPole);
    return 20.0 * std::log10 (std::abs (out) + 1e-12);
}
} // namespace resonator

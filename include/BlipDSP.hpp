// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace blipext {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kNyquistMargin = 0.999;
constexpr int kMaxHarmonics = 2048;
constexpr double kWidthEpsilon = 1.0e-5;
constexpr double kPowerMinimum = 1.0e-3;

struct ComplexPair {
    double real;
    double imag;
};

struct WeightedSum {
    double numerator;
    double weight;
};

inline bool isFinite(double value) noexcept {
    return std::isfinite(value);
}

inline double clamp(double value, double low, double high) noexcept {
    return std::max(low, std::min(value, high));
}

inline double lerp(double a, double b, double fraction) noexcept {
    return a + fraction * (b - a);
}

inline double wrapPhase(double phase) noexcept {
    if (!isFinite(phase))
        return 0.0;
    phase = std::remainder(phase, kTwoPi);
    if (phase >= kPi)
        phase -= kTwoPi;
    else if (phase < -kPi)
        phase += kTwoPi;
    return phase;
}

inline bool advancePhase(double& phase, double increment) noexcept {
    if (!isFinite(increment))
        return false;

    const double unwrapped = phase + increment;
    const bool crossed = unwrapped >= kPi || unwrapped < -kPi;
    if (std::abs(unwrapped) < kTwoPi) {
        phase = unwrapped;
        if (phase >= kPi)
            phase -= kTwoPi;
        else if (phase < -kPi)
            phase += kTwoPi;
    } else {
        phase = wrapPhase(unwrapped);
    }
    return crossed;
}

inline int requestedHarmonics(double value, int maximum = kMaxHarmonics) noexcept {
    if (!isFinite(value))
        return 0;
    value = clamp(std::floor(value), 1.0, static_cast<double>(maximum));
    return static_cast<int>(value);
}

inline int effectiveHarmonics(double freq, double sampleRate, int requested) noexcept {
    requested = std::max(1, std::min(requested, kMaxHarmonics));
    if (!isFinite(freq) || !isFinite(sampleRate) || sampleRate <= 0.0)
        return 1;

    const double magnitude = std::abs(freq);
    if (magnitude <= std::numeric_limits<double>::min())
        return requested;

    const double safeNyquist = 0.5 * sampleRate * kNyquistMargin;
    const double safeCount = std::floor(safeNyquist / magnitude);
    if (!isFinite(safeCount) || safeCount >= requested)
        return requested;
    return std::max(1, static_cast<int>(safeCount));
}

inline void sincosPair(double phase, double& sine, double& cosine) noexcept {
    sine = std::sin(phase);
    cosine = std::cos(phase);
}

inline double sinc(double value) noexcept {
    const double magnitude = std::abs(value);
    if (magnitude < 1.0e-4) {
        const double squared = value * value;
        return 1.0 + squared * (-1.0 / 6.0 + squared * (1.0 / 120.0 - squared / 5040.0));
    }
    return std::sin(value) / value;
}

// Ratio of two normalized sinc functions.  This gives the analytic zero
// limit needed by all finite-kernel sine ratios.
inline double safeSincRatio(double numeratorPhase, double denominatorPhase) noexcept {
    return sinc(numeratorPhase) / sinc(denominatorPhase);
}

inline ComplexPair complexAdd(ComplexPair a, ComplexPair b) noexcept {
    return { a.real + b.real, a.imag + b.imag };
}

inline ComplexPair complexScale(ComplexPair value, double scale) noexcept {
    return { value.real * scale, value.imag * scale };
}

inline ComplexPair complexMultiply(ComplexPair a, ComplexPair b) noexcept {
    return { a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real };
}

inline ComplexPair complexDivide(ComplexPair numerator, ComplexPair denominator) noexcept {
    const double scale = denominator.real * denominator.real + denominator.imag * denominator.imag;
    if (scale <= std::numeric_limits<double>::min())
        return { 0.0, 0.0 };
    return { (numerator.real * denominator.real + numerator.imag * denominator.imag) / scale,
             (numerator.imag * denominator.real - numerator.real * denominator.imag) / scale };
}

inline ComplexPair cis(double phase) noexcept {
    double sine;
    double cosine;
    sincosPair(phase, sine, cosine);
    return { cosine, sine };
}

inline ComplexPair harmonicComplexSum(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    phase = wrapPhase(phase);
    const double half = 0.5 * phase;
    const double amplitude =
        static_cast<double>(harmonics) * safeSincRatio(static_cast<double>(harmonics) * half, half);
    return complexScale(cis(static_cast<double>(harmonics + 1) * half), amplitude);
}

inline double dirichletCosSum(double phase, int harmonics) noexcept {
    return harmonicComplexSum(phase, harmonics).real / static_cast<double>(std::max(1, harmonics));
}

inline double fullDirichlet(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    return 1.0 + 2.0 * static_cast<double>(harmonics) * dirichletCosSum(phase, harmonics);
}

inline double fejerPeak(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    phase = wrapPhase(phase);
    const double half = 0.5 * phase;
    const double ratio =
        safeSincRatio(static_cast<double>(harmonics + 1) * half, half);
    return ratio * ratio;
}

inline double weightedArithmeticCosSum(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    return (static_cast<double>(harmonics + 1) * fejerPeak(phase, harmonics)
            - 1.0)
        / static_cast<double>(harmonics);
}

inline ComplexPair complexExpm1(double real, double imag) noexcept {
    double sine;
    double cosine;
    sincosPair(imag, sine, cosine);
    const double expReal = std::exp(real);
    return { std::expm1(real) * cosine + (cosine - 1.0), expReal * sine };
}

inline ComplexPair geometricSeriesZero(double ratio, double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    ratio = clamp(ratio, 0.0, 1.0);
    phase = wrapPhase(phase);

    if (ratio == 0.0)
        return { 1.0, 0.0 };
    if (ratio == 1.0) {
        const ComplexPair sum = harmonicComplexSum(phase, harmonics);
        return complexMultiply(cis(-phase), sum);
    }

    const double logRatio = std::log(ratio);
    const ComplexPair denominator = complexExpm1(logRatio, phase);
    if (denominator.real * denominator.real + denominator.imag * denominator.imag < 1.0e-24)
        return { static_cast<double>(harmonics), 0.0 };
    const ComplexPair numerator =
        complexExpm1(static_cast<double>(harmonics) * logRatio,
                     static_cast<double>(harmonics) * phase);
    return complexDivide(numerator, denominator);
}

inline double geometricWeight(double ratio, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    ratio = clamp(ratio, 0.0, 1.0);
    if (ratio == 0.0)
        return 1.0;
    if (ratio == 1.0)
        return static_cast<double>(harmonics);
    const double logRatio = std::log(ratio);
    return std::expm1(static_cast<double>(harmonics) * logRatio) / std::expm1(logRatio);
}

inline double geometricCosSum(double phase, int harmonics, double ratio) noexcept {
    const ComplexPair sum =
        complexMultiply(cis(wrapPhase(phase)), geometricSeriesZero(ratio, phase, harmonics));
    const double weight = geometricWeight(ratio, harmonics);
    return weight > 0.0 ? sum.real / weight : 0.0;
}

inline double geometricCosSumPrepared(double phase, int harmonics,
                                      double ratio, double ratioPower,
                                      double weight) noexcept {
    harmonics = std::max(1, harmonics);
    ratio = clamp(ratio, 0.0, 1.0);
    phase = wrapPhase(phase);
    if (ratio == 0.0)
        return std::cos(phase);
    if (ratio == 1.0)
        return dirichletCosSum(phase, harmonics);

    const ComplexPair unitPhase = cis(phase);
    const ComplexPair finalPhase =
        cis(static_cast<double>(harmonics) * phase);
    const ComplexPair numerator {
        1.0 - ratioPower * finalPhase.real,
        -ratioPower * finalPhase.imag
    };
    const ComplexPair denominator {
        1.0 - ratio * unitPhase.real,
        -ratio * unitPhase.imag
    };
    const ComplexPair sum =
        complexMultiply(unitPhase,
                        complexDivide(numerator, denominator));
    return weight > 0.0 ? sum.real / weight : 0.0;
}

inline double binomialCoefficient(int n, int k) noexcept {
    k = std::min(k, n - k);
    double result = 1.0;
    for (int index = 1; index <= k; ++index)
        result *= static_cast<double>(n - k + index) / static_cast<double>(index);
    return result;
}

inline double cosinePowerNumerator(double phase, int harmonics,
                                   int power) noexcept {
    harmonics = std::max(1, harmonics);
    power = std::max(1, std::min(power, 8));
    const double angleStep = kPi / (2.0 * static_cast<double>(harmonics));
    const double scale = std::ldexp(1.0, -power);
    ComplexPair sum { 0.0, 0.0 };

    for (int index = 0; index <= power; ++index) {
        const int multiple = power - 2 * index;
        const double offset = static_cast<double>(multiple) * angleStep;
        const double coefficient = scale * binomialCoefficient(power, index);
        const ComplexPair shifted =
            complexMultiply(cis(-offset), harmonicComplexSum(phase + offset, harmonics));
        sum = complexAdd(sum, complexScale(shifted, coefficient));
    }

    return sum.real;
}

inline double cosinePowerWeight(int harmonics, int power) noexcept {
    return std::max(cosinePowerNumerator(0.0, harmonics, power),
                    std::numeric_limits<double>::min());
}

inline WeightedSum cosinePowerSum(double phase, int harmonics, int power) noexcept {
    return { cosinePowerNumerator(phase, harmonics, power),
             cosinePowerWeight(harmonics, power) };
}

inline double cosinePowerKernel(double phase, int harmonics, double curve) noexcept {
    curve = clamp(curve, 1.0, 8.0);
    const int lower = static_cast<int>(std::floor(curve));
    const int upper = std::min(8, lower + 1);
    const double fraction = curve - static_cast<double>(lower);
    const WeightedSum a = cosinePowerSum(phase, harmonics, lower);
    if (upper == lower)
        return a.numerator / a.weight;
    const WeightedSum b = cosinePowerSum(phase, harmonics, upper);
    return lerp(a.numerator / a.weight, b.numerator / b.weight, fraction);
}

inline double weightedDerivativeSinSum(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    phase = wrapPhase(phase);
    const double n = static_cast<double>(harmonics);
    const double normalization = 0.5 * n * (n + 1.0);

    if (std::abs(phase) * n < 0.1) {
        const double n2 = n * n;
        const double sum2 = n * (n + 1.0) * (2.0 * n + 1.0) / 6.0;
        const double sum4 =
            n * (n + 1.0) * (2.0 * n + 1.0) * (3.0 * n2 + 3.0 * n - 1.0) / 30.0;
        const double sum6 = n * (n + 1.0) * (2.0 * n + 1.0)
            * (3.0 * n2 * n2 + 6.0 * n2 * n - 3.0 * n + 1.0) / 42.0;
        const double p2 = phase * phase;
        const double raw = phase * (sum2 + p2 * (-sum4 / 6.0 + p2 * sum6 / 120.0));
        return -raw / normalization;
    }

    const ComplexPair z = cis(phase);
    const ComplexPair zN = cis(n * phase);
    const ComplexPair zN1 = complexMultiply(zN, z);
    const ComplexPair inner {
        1.0 - (n + 1.0) * zN.real + n * zN1.real,
        -(n + 1.0) * zN.imag + n * zN1.imag
    };
    const ComplexPair numerator = complexMultiply(z, inner);
    const ComplexPair oneMinusZ { 1.0 - z.real, -z.imag };
    const ComplexPair denominator = complexMultiply(oneMinusZ, oneMinusZ);
    const ComplexPair sum = complexDivide(numerator, denominator);
    return -sum.imag / normalization;
}

inline double fractionalCosSum(double phase, double harmonics) noexcept {
    harmonics = std::max(1.0, harmonics);
    int integerPart = static_cast<int>(std::floor(harmonics));
    integerPart = std::max(1, std::min(integerPart, kMaxHarmonics));
    const double fraction = std::min(1.0, harmonics - static_cast<double>(integerPart));
    const double integerSum =
        static_cast<double>(integerPart) * dirichletCosSum(phase, integerPart);
    const double last = fraction * std::cos(static_cast<double>(integerPart + 1) * phase);
    return (integerSum + last) / (static_cast<double>(integerPart) + fraction);
}

inline double widthSeries(double phase, int harmonics, double width) noexcept {
    harmonics = std::max(1, harmonics);
    width = clamp(width, kWidthEpsilon, 1.0 - kWidthEpsilon);
    double cosineK = std::cos(phase);
    double sineK = std::sin(phase);
    const double cosineStep = cosineK;
    const double sineStep = sineK;
    double sum = 0.0;
    for (int k = 1; k <= harmonics; ++k) {
        sum += (4.0 * std::sin(kPi * static_cast<double>(k) * width)
                / (kPi * static_cast<double>(k)))
            * cosineK;
        const double nextCosine = cosineK * cosineStep - sineK * sineStep;
        sineK = sineK * cosineStep + cosineK * sineStep;
        cosineK = nextCosine;
    }
    return sum;
}

inline double widthPeakScale(double width) noexcept {
    width = clamp(width, kWidthEpsilon, 1.0 - kWidthEpsilon);
    return 1.0 / (2.0 * std::max(width, 1.0 - width));
}

inline double harmonicNumber(int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    double result = 0.0;
    for (int k = 1; k <= harmonics; ++k)
        result += 1.0 / static_cast<double>(k);
    return result;
}

inline double inverseHarmonicSinSum(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    double sine;
    double cosine;
    sincosPair(phase, sine, cosine);
    const double sineStep = sine;
    const double cosineStep = cosine;
    double sum = 0.0;
    for (int k = 1; k <= harmonics; ++k) {
        sum += sine / static_cast<double>(k);
        const double nextCosine = cosine * cosineStep - sine * sineStep;
        sine = sine * cosineStep + cosine * sineStep;
        cosine = nextCosine;
    }
    return sum / harmonicNumber(harmonics);
}

inline double dispersedCosSum(double phase, int harmonics, double dispersion) noexcept {
    harmonics = std::max(1, harmonics);
    double sum = 0.0;
    for (int k = 1; k <= harmonics; ++k) {
        const double kd = static_cast<double>(k);
        sum += std::cos(kd * phase + dispersion * kd * kd);
    }
    return sum / static_cast<double>(harmonics);
}

inline double onePoleDifferenceNormalization(double attackCoefficient,
                                             double decayCoefficient) noexcept {
    const double aa = clamp(attackCoefficient, 0.0, 1.0 - 1.0e-15);
    const double ad = clamp(decayCoefficient, aa, 1.0 - 1.0e-15);
    const double ba = 1.0 - aa;
    const double bd = 1.0 - ad;

    double peak = std::abs(bd - ba);
    const double denominator = std::log(ad) - std::log(aa);
    if (ba > 0.0 && bd > 0.0 && std::abs(denominator) > 1.0e-15) {
        const double ratio = (ba * std::log(aa)) / (bd * std::log(ad));
        if (ratio > 0.0 && isFinite(ratio)) {
            const double stationary = std::log(ratio) / denominator;
            const int center = static_cast<int>(std::floor(std::max(0.0, stationary)));
            for (int offset = 0; offset < 3; ++offset) {
                const int sample = center + offset;
                const double value =
                    bd * std::pow(ad, sample) - ba * std::pow(aa, sample);
                peak = std::max(peak, std::abs(value));
            }
        }
    }
    // The isolated-sample response can imply very large gain when both poles
    // are slow.  A finite Dirichlet excitation may span many samples, so that
    // isolated-response gain is safe only up to unity.  The ceiling is a
    // mathematical state bound, not output-dependent normalization.
    const double responseGain = peak > 1.0e-15 ? 1.0 / peak : 1.0;
    return -std::min(1.0, responseGain);
}

inline double zap(double value) noexcept {
    if (!isFinite(value) || std::abs(value) < 1.0e-30)
        return 0.0;
    return value;
}

} // namespace blipext

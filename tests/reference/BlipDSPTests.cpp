// SPDX-License-Identifier: GPL-3.0-or-later

#include "BlipDSP.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace blipext;

int failures = 0;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void close(double actual, double expected, double tolerance,
           const std::string& message) {
    if (!std::isfinite(actual)
        || std::abs(actual - expected) > tolerance) {
        std::cerr << "FAIL: " << message << " actual=" << actual
                  << " expected=" << expected
                  << " tolerance=" << tolerance << '\n';
        ++failures;
    }
}

double additiveRect(double phase, int harmonics) {
    double sum = 0.0;
    for (int k = 1; k <= harmonics; ++k)
        sum += std::cos(static_cast<double>(k) * phase);
    return sum / static_cast<double>(harmonics);
}

double additiveTriangle(double phase, int harmonics) {
    double sum = 0.0;
    double weight = 0.0;
    for (int k = 1; k <= harmonics; ++k) {
        const double local =
            1.0 - static_cast<double>(k - 1)
                / static_cast<double>(harmonics);
        sum += local * std::cos(static_cast<double>(k) * phase);
        weight += local;
    }
    return sum / weight;
}

double additiveCosinePower(double phase, int harmonics, int power) {
    double sum = 0.0;
    double weight = 0.0;
    for (int k = 1; k <= harmonics; ++k) {
        const double angle =
            kPi * static_cast<double>(k - 1)
            / (2.0 * static_cast<double>(harmonics));
        const double local = std::pow(std::cos(angle), power);
        sum += local * std::cos(static_cast<double>(k) * phase);
        weight += local;
    }
    return sum / weight;
}

double additiveDecay(double phase, int harmonics, double ratio) {
    double sum = 0.0;
    double weight = 0.0;
    double local = 1.0;
    for (int k = 1; k <= harmonics; ++k) {
        sum += local * std::cos(static_cast<double>(k) * phase);
        weight += local;
        local *= ratio;
    }
    return sum / weight;
}

double additiveDiff(double phase, int harmonics) {
    double sum = 0.0;
    for (int k = 1; k <= harmonics; ++k)
        sum -= static_cast<double>(k)
            * std::sin(static_cast<double>(k) * phase);
    return sum
        / (0.5 * static_cast<double>(harmonics)
           * static_cast<double>(harmonics + 1));
}

void testPhaseAndLimiter() {
    close(wrapPhase(1000000.0 * kTwoPi + 0.25), 0.25, 1.0e-9,
          "large phase wraps accurately");
    close(wrapPhase(-1000000.0 * kTwoPi - 0.25), -0.25, 1.0e-9,
          "large negative phase wraps accurately");
    require(effectiveHarmonics(0.0, 48000.0, 200) == 200,
            "zero frequency preserves requested count");
    require(effectiveHarmonics(-1000.0, 48000.0, 200) == 23,
            "negative frequency uses absolute value");
    require(effectiveHarmonics(24000.0, 48000.0, 200) == 1,
            "near-Nyquist count remains at least one");
    require(requestedHarmonics(std::numeric_limits<double>::infinity()) == 0,
            "non-finite harmonic request is rejected");
}

void testClosedForms() {
    const int counts[] { 1, 2, 7, 32, 257, 2048 };
    const double phases[] {
        0.0, 1.0e-12, 1.0e-7, 0.003, 0.27, 1.9, -2.8
    };
    for (const int count : counts) {
        for (const double phase : phases) {
            close(dirichletCosSum(phase, count),
                  additiveRect(phase, count), 2.0e-10,
                  "Dirichlet sum matches additive reference");
            close(weightedArithmeticCosSum(phase, count),
                  additiveTriangle(phase, count), 3.0e-10,
                  "triangular sum matches additive reference");
            close(weightedDerivativeSinSum(phase, count),
                  additiveDiff(phase, count), 5.0e-9,
                  "derivative sum matches additive reference");
        }
    }

    for (int power = 1; power <= 8; ++power) {
        for (const int count : { 1, 8, 31, 128 }) {
            for (const double phase : { 0.0, 0.01, 0.7, -2.4 }) {
                const WeightedSum value =
                    cosinePowerSum(phase, count, power);
                close(value.numerator / value.weight,
                      additiveCosinePower(phase, count, power),
                      2.0e-10,
                      "cosine-power expansion matches direct weights");
            }
        }
    }

    for (const double ratio :
         { 0.0, 1.0e-8, 0.2, 0.9, 0.999999999, 1.0 }) {
        for (const int count : { 1, 8, 128, 2048 }) {
            for (const double phase :
                 { 0.0, 1.0e-10, 0.01, 1.2, -2.7 }) {
                close(geometricCosSum(phase, count, ratio),
                      additiveDecay(phase, count, ratio),
                      3.0e-8,
                      "geometric sum matches additive reference");
            }
        }
    }
}

void testShapesAndBounds() {
    for (const int count : { 1, 4, 64, 512 }) {
        close(fejerPeak(0.0, count), 1.0, 1.0e-14,
              "Fejer peak has exact zero-phase limit");
        for (int index = 0; index < 4000; ++index) {
            const double phase = -kPi
                + kTwoPi * static_cast<double>(index) / 4000.0;
            const double peak = fejerPeak(phase, count);
            require(std::isfinite(peak) && peak >= -1.0e-14
                        && peak <= 1.0 + 1.0e-12,
                    "Fejer kernel is finite, nonnegative, and bounded");
            require(std::abs(dirichletCosSum(phase, count))
                        <= 1.0 + 1.0e-12,
                    "rectangular kernel respects peak bound");
            require(std::abs(weightedDerivativeSinSum(phase, count))
                        <= 1.0 + 1.0e-10,
                    "derivative bound normalization is valid");
        }
    }

    close(fractionalCosSum(0.7, 12.0),
          additiveRect(0.7, 12), 1.0e-12,
          "integer fractional count matches rectangular kernel");
    const double expected =
        (12.0 * additiveRect(0.7, 12) + 0.25 * std::cos(13.0 * 0.7))
        / 12.25;
    close(fractionalCosSum(0.7, 12.25), expected, 1.0e-12,
          "fractional final partial has the requested amplitude");

    for (const int count : { 1, 8, 64 }) {
        for (const double width : { 0.01, 0.1, 0.5, 0.9, 0.99 }) {
            for (const double phase : { 0.0, 0.2, 1.0, -2.5 }) {
                const double value = widthSeries(phase, count, width)
                    * widthPeakScale(width);
                require(std::isfinite(value),
                        "width reference remains finite");
            }
        }
    }
}

void testExplicitReferences() {
    for (const int count : { 1, 8, 128, 2048 }) {
        for (const double phase : { 0.0, 1.0e-8, 0.2, -2.0 }) {
            const double value = inverseHarmonicSinSum(phase, count);
            require(std::isfinite(value) && std::abs(value) <= 1.0 + 1.0e-12,
                    "inverse-harmonic sum respects documented bound");
        }
        for (const double dispersion : { 0.0, 0.001, -0.2, 9.0 }) {
            const double value =
                dispersedCosSum(0.3, count, dispersion);
            require(std::isfinite(value) && std::abs(value) <= 1.0 + 1.0e-12,
                    "dispersed sum remains finite and bounded");
        }
    }
    for (const double attack : { 1.0 / 48000.0, 0.001, 0.02 }) {
        const double aa = std::exp(-1.0 / (attack * 48000.0));
        const double ad =
            std::exp(-1.0 / ((attack + 0.05) * 48000.0));
        const double normalization =
            onePoleDifferenceNormalization(aa, ad);
        require(std::isfinite(normalization) && normalization < 0.0,
                "attack-decay normalization is finite and polarity-correct");
    }
}

} // namespace

int main() {
    testPhaseAndLimiter();
    testClosedForms();
    testShapesAndBounds();
    testExplicitReferences();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "BlipDSP reference tests passed\n";
    return EXIT_SUCCESS;
}

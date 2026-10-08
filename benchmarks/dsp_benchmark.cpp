// SPDX-License-Identifier: GPL-3.0-or-later

#include "BlipDSP.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;
using namespace blipext;

volatile double sink = 0.0;

template <typename Function>
double measure(Function function, int iterations) {
    double phase = 0.123;
    double result = 0.0;
    const auto start = Clock::now();
    for (int index = 0; index < iterations; ++index) {
        result += function(phase);
        phase = wrapPhase(phase + 0.017453292519943295);
    }
    const auto end = Clock::now();
    sink = result;
    const auto nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count();
    return static_cast<double>(nanoseconds)
        / static_cast<double>(iterations);
}

void printCase(const std::string& name, int harmonics,
               double nanoseconds) {
    std::cout << std::left << std::setw(18) << name
              << " N=" << std::setw(4) << harmonics
              << " ns/sample=" << std::fixed << std::setprecision(2)
              << nanoseconds << '\n';
}

} // namespace

int main() {
#if defined(__clang__)
    std::cout << "compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
    std::cout << "compiler=GCC " << __VERSION__ << '\n';
#elif defined(_MSC_VER)
    std::cout << "compiler=MSVC " << _MSC_VER << '\n';
#endif
#if defined(__APPLE__)
    std::cout << "platform=macOS\n";
#elif defined(_WIN32)
    std::cout << "platform=Windows\n";
#elif defined(__linux__)
    std::cout << "platform=Linux\n";
#endif
#ifdef NDEBUG
    std::cout << "build=Release\n";
#else
    std::cout << "build=Debug\n";
#endif
    std::cout << "iterations=200000\n";

    constexpr int iterations = 200000;
    for (const int harmonics : { 8, 32, 128, 512, 2048 }) {
        const double cosineWeight =
            cosinePowerWeight(harmonics, 4);
        const double decayRatioPower =
            std::pow(0.8, harmonics);
        const double decayWeight =
            geometricWeight(0.8, harmonics);
        printCase("rect", harmonics,
                  measure([harmonics](double phase) {
                      return dirichletCosSum(phase, harmonics);
                  }, iterations));
        printCase("series", harmonics,
                  measure([harmonics](double phase) {
                      const double half = 0.5 * phase;
                      return safeSincRatio(
                          static_cast<double>(harmonics) * half, half)
                          * std::cos(
                              static_cast<double>(harmonics - 1) * half);
                  }, iterations));
        printCase("triangle", harmonics,
                  measure([harmonics](double phase) {
                      return weightedArithmeticCosSum(phase, harmonics);
                  }, iterations));
        printCase("cosine-power-4", harmonics,
                  measure([harmonics, cosineWeight](double phase) {
                      return cosinePowerNumerator(
                          phase, harmonics, 4) / cosineWeight;
                  }, iterations));
        printCase("peak", harmonics,
                  measure([harmonics](double phase) {
                      return fejerPeak(phase, harmonics);
                  }, iterations));
        printCase("decay-static", harmonics,
                  measure([harmonics, decayRatioPower,
                           decayWeight](double phase) {
                      return geometricCosSumPrepared(
                          phase, harmonics, 0.8,
                          decayRatioPower, decayWeight);
                  }, iterations));
        printCase("decay-audio", harmonics,
                  measure([harmonics](double phase) {
                      const double ratio =
                          0.5 + 0.49 * std::sin(0.031 * phase);
                      return geometricCosSum(
                          phase, harmonics, ratio);
                  }, iterations));
        printCase("fractional", harmonics,
                  measure([harmonics](double phase) {
                      return fractionalCosSum(
                          phase, static_cast<double>(harmonics) - 0.5);
                  }, iterations));
        printCase("derivative", harmonics,
                  measure([harmonics](double phase) {
                      return weightedDerivativeSinSum(phase, harmonics);
                  }, iterations));
        printCase("width-derivative", harmonics,
                  measure([harmonics](double phase) {
                      constexpr double width = 0.25;
                      const double offset = kPi * width;
                      return (2.0 * static_cast<double>(harmonics) / kPi)
                          * (dirichletCosSum(
                                 phase + offset, harmonics)
                             - dirichletCosSum(
                                 phase - offset, harmonics));
                  }, iterations));
        printCase("attack-decay-step", harmonics,
                  measure([harmonics, attackState = 0.0,
                           decayState = 0.0](double phase) mutable {
                      constexpr double attackCoefficient = 0.99;
                      constexpr double decayCoefficient = 0.999;
                      const double excitation =
                          fullDirichlet(phase, harmonics)
                          / static_cast<double>(2 * harmonics + 1);
                      attackState =
                          attackCoefficient * attackState
                          + (1.0 - attackCoefficient) * excitation;
                      decayState =
                          decayCoefficient * decayState
                          + (1.0 - decayCoefficient) * excitation;
                      return decayState - attackState;
                  }, iterations));
        printCase("feedback-step", harmonics,
                  measure([harmonics, previous = 0.0](
                              double phase) mutable {
                      previous = dirichletCosSum(
                          phase + 0.7 * previous
                              / static_cast<double>(harmonics),
                          harmonics);
                      return previous;
                  }, iterations));
        printCase("dispersion-O(N)", harmonics,
                  measure([harmonics](double phase) {
                      return dispersedCosSum(phase, harmonics, 0.013);
                  }, iterations / 20));
        printCase("integral-O(N)", harmonics,
                  measure([harmonics](double phase) {
                      return inverseHarmonicSinSum(phase, harmonics);
                  }, iterations / 20));
    }
    return sink == 123456789.0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

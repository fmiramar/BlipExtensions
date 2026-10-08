// SPDX-License-Identifier: GPL-3.0-or-later
//
// The finite Dirichlet-kernel architecture and topology-transition idea are
// informed by SuperCollider's GPL-licensed OscUGens.cpp.  This implementation
// is a new double-phase, analytic-limit design; it does not use the legacy
// fixed-point sine/cosecant table evaluator.

#include "SC_PlugIn.h"
#include "BlipDSP.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

static InterfaceTable* ft;

namespace {

using namespace blipext;

inline bool finiteValue(double value) noexcept {
    return blipext::isFinite(value);
}


struct InputView {
    const float* data;
    int stride;

    double at(int index) const noexcept {
        return static_cast<double>(data[index * stride]);
    }
};

inline InputView inputView(Unit* unit, int index) noexcept {
    return { IN(index), INRATE(index) == calc_FullRate ? 1 : 0 };
}

inline double transitionAt(int index, int count) noexcept {
    return count <= 1 ? 1.0
                      : static_cast<double>(index) / static_cast<double>(count - 1);
}

inline double blockMaxAbs(Unit* unit, int inputIndex, int count) noexcept {
    const InputView input = inputView(unit, inputIndex);
    double maximum = 0.0;
    for (int index = 0; index < count; ++index) {
        const double value = input.at(index);
        if (!finiteValue(value))
            return std::numeric_limits<double>::infinity();
        maximum = std::max(maximum, std::abs(value));
    }
    return maximum;
}

inline int blockHarmonics(Unit* unit, int frequencyIndex, int requested,
                          int count) noexcept {
    return effectiveHarmonics(blockMaxAbs(unit, frequencyIndex, count),
                              static_cast<double>(SAMPLERATE), requested);
}

inline bool advanceCarrier(double& phase, double frequency,
                           double radiansPerSample) noexcept {
    if (!finiteValue(frequency))
        return false;
    return advancePhase(phase, frequency * radiansPerSample);
}

inline void clearOutput(Unit* unit, int count) noexcept {
    Clear(count, OUT(0));
}

struct BasicPhaseUnit : Unit {
    double phase;
    double radiansPerSample;
    int currentHarmonics;
};

inline void initializeBasic(BasicPhaseUnit* unit, int requested) noexcept {
    unit->phase = 0.0;
    unit->radiansPerSample = kTwoPi / static_cast<double>(SAMPLERATE);
    unit->currentHarmonics =
        effectiveHarmonics(static_cast<double>(ZIN0(0)),
                           static_cast<double>(SAMPLERATE), requested);
}

// BlipSeries -----------------------------------------------------------------

struct BlipSeries : Unit {
    double firstPhase;
    double spacingPhase;
    double radiansPerSample;
    int currentPartials;
};

inline int seriesBlockPartials(BlipSeries* unit, int requested,
                               int count) noexcept {
    const InputView frequency = inputView(unit, 0);
    const InputView first = inputView(unit, 2);
    const InputView spacing = inputView(unit, 3);
    const double safeNyquist =
        0.5 * static_cast<double>(SAMPLERATE) * kNyquistMargin;
    int safe = requested;

    for (int index = 0; index < count; ++index) {
        const double f = frequency.at(index);
        const double aInput = first.at(index);
        const double dInput = spacing.at(index);
        if (!finiteValue(f) || !finiteValue(aInput) || !finiteValue(dInput))
            return 1;
        const double magnitude = std::abs(f);
        const double a = std::max(0.0, aInput);
        const double d = std::max(0.0, dInput);
        if (magnitude <= std::numeric_limits<double>::min())
            continue;

        if (d <= std::numeric_limits<double>::min()) {
            if (magnitude * a > safeNyquist)
                safe = 1;
        } else {
            const double remaining = safeNyquist / magnitude - a;
            const int localSafe =
                std::max(1, static_cast<int>(std::floor(remaining / d)) + 1);
            safe = std::min(safe, localSafe);
        }
    }
    return std::max(1, std::min(safe, requested));
}

inline double seriesValue(double firstPhase, double spacingPhase,
                          int partials) noexcept {
    partials = std::max(1, partials);
    spacingPhase = wrapPhase(spacingPhase);
    const double half = 0.5 * spacingPhase;
    const double ratio =
        safeSincRatio(static_cast<double>(partials) * half, half);
    return ratio
        * std::cos(firstPhase + static_cast<double>(partials - 1) * half);
}

void BlipSeries_next(BlipSeries* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }

    const int nextPartials =
        seriesBlockPartials(unit, requested, inNumSamples);
    const int previousPartials = unit->currentPartials;
    const bool transition = previousPartials != nextPartials;
    const InputView frequency = inputView(unit, 0);
    const InputView first = inputView(unit, 2);
    const InputView spacing = inputView(unit, 3);
    double firstPhase = unit->firstPhase;
    double spacingPhase = unit->spacingPhase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double aInput = first.at(index);
        const double dInput = spacing.at(index);
        if (!finiteValue(f) || !finiteValue(aInput) || !finiteValue(dInput)) {
            out[index] = 0.0f;
            if (finiteValue(f)) {
                advancePhase(firstPhase,
                             f * std::max(0.0, finiteValue(aInput) ? aInput : 0.0)
                                 * unit->radiansPerSample);
                advancePhase(spacingPhase,
                             f * std::max(0.0, finiteValue(dInput) ? dInput : 0.0)
                                 * unit->radiansPerSample);
            }
            continue;
        }

        double value = seriesValue(firstPhase, spacingPhase, nextPartials);
        if (transition) {
            const double oldValue =
                seriesValue(firstPhase, spacingPhase, previousPartials);
            value = lerp(oldValue, value, transitionAt(index, inNumSamples));
        }
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;

        advancePhase(firstPhase, f * std::max(0.0, aInput)
                                        * unit->radiansPerSample);
        advancePhase(spacingPhase, f * std::max(0.0, dInput)
                                          * unit->radiansPerSample);
    }

    unit->firstPhase = firstPhase;
    unit->spacingPhase = spacingPhase;
    unit->currentPartials = nextPartials;
}

void BlipSeries_Ctor(BlipSeries* unit) {
    SETCALC(BlipSeries_next);
    unit->firstPhase = 0.0;
    unit->spacingPhase = 0.0;
    unit->radiansPerSample = kTwoPi / static_cast<double>(SAMPLERATE);
    const double frequency = static_cast<double>(ZIN0(0));
    const double request = static_cast<double>(ZIN0(1));
    const double first = static_cast<double>(ZIN0(2));
    const double spacing = static_cast<double>(ZIN0(3));
    const int parsedRequest = requestedHarmonics(request);
    const int requested = std::max(1, parsedRequest);
    unit->currentPartials =
        seriesBlockPartials(unit, requested, 1);
    ZOUT0(0) = finiteValue(frequency) && parsedRequest != 0
            && finiteValue(first) && finiteValue(spacing)
        ? 1.0f : 0.0f;
}

// BlipKernel -----------------------------------------------------------------

struct BlipKernel : BasicPhaseUnit {
    double previousCurve;
    double currentCosineWeights[9];
    double targetCosineWeights[9];
    int kernel;
};

inline WeightedSum rectangularSum(double phase, int harmonics) noexcept {
    harmonics = std::max(1, harmonics);
    return { static_cast<double>(harmonics)
                 * dirichletCosSum(phase, harmonics),
             static_cast<double>(harmonics) };
}

inline WeightedSum triangularSum(double phase, int harmonics) noexcept {
    const double weight = 0.5 * static_cast<double>(harmonics + 1);
    return { weight * weightedArithmeticCosSum(phase, harmonics), weight };
}

inline WeightedSum kernelTarget(double phase, int harmonics, int kernel,
                                double curve,
                                const double* cosineWeights) noexcept {
    if (kernel == 1)
        return triangularSum(phase, harmonics);
    if (kernel == 2) {
        curve = clamp(curve, 1.0, 8.0);
        const int lower = static_cast<int>(std::floor(curve));
        const int upper = std::min(8, lower + 1);
        const double fraction = curve - static_cast<double>(lower);
        const WeightedSum a {
            cosinePowerNumerator(phase, harmonics, lower),
            cosineWeights[lower]
        };
        if (upper == lower)
            return a;
        const WeightedSum b {
            cosinePowerNumerator(phase, harmonics, upper),
            cosineWeights[upper]
        };
        return { lerp(a.numerator, b.numerator, fraction),
                 lerp(a.weight, b.weight, fraction) };
    }
    return rectangularSum(phase, harmonics);
}

inline double kernelValue(double phase, int harmonics, int kernel,
                          double amount, double curve,
                          const double* cosineWeights) noexcept {
    const WeightedSum rectangular = rectangularSum(phase, harmonics);
    if (kernel == 0 || amount <= 0.0)
        return rectangular.numerator / rectangular.weight;
    const WeightedSum target =
        kernelTarget(phase, harmonics, kernel, curve, cosineWeights);
    amount = clamp(amount, 0.0, 1.0);
    const double numerator =
        lerp(rectangular.numerator, target.numerator, amount);
    const double weight = lerp(rectangular.weight, target.weight, amount);
    return weight > 0.0 ? numerator / weight : 0.0;
}

void BlipKernel_next(BlipKernel* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    const double targetCurve = static_cast<double>(ZIN0(4));
    if (requested == 0 || !finiteValue(targetCurve)) {
        clearOutput(unit, inNumSamples);
        return;
    }

    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool topologyTransition =
        previousHarmonics != nextHarmonics;
    if (topologyTransition && unit->kernel == 2) {
        for (int power = 1; power <= 8; ++power)
            unit->targetCosineWeights[power] =
                cosinePowerWeight(nextHarmonics, power);
    }
    const InputView frequency = inputView(unit, 0);
    const InputView amount = inputView(unit, 3);
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double shape = amount.at(index);
        if (!finiteValue(f) || !finiteValue(shape)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advanceCarrier(phase, f, unit->radiansPerSample);
            continue;
        }

        const double transitionFraction =
            transitionAt(index, inNumSamples);
        const double curve =
            lerp(unit->previousCurve, clamp(targetCurve, 1.0, 8.0),
                 transitionFraction);
        double value =
            kernelValue(
                phase, nextHarmonics, unit->kernel, shape, curve,
                topologyTransition ? unit->targetCosineWeights
                                   : unit->currentCosineWeights);
        if (topologyTransition) {
            const double oldValue =
                kernelValue(phase, previousHarmonics, unit->kernel,
                            shape, curve, unit->currentCosineWeights);
            value = lerp(oldValue, value, transitionFraction);
        }
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }

    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
    unit->previousCurve = clamp(targetCurve, 1.0, 8.0);
    if (topologyTransition && unit->kernel == 2) {
        for (int power = 1; power <= 8; ++power)
            unit->currentCosineWeights[power] =
                unit->targetCosineWeights[power];
    }
}

void BlipKernel_Ctor(BlipKernel* unit) {
    SETCALC(BlipKernel_next);
    const double frequency = static_cast<double>(ZIN0(0));
    const double request = static_cast<double>(ZIN0(1));
    const double amount = static_cast<double>(ZIN0(3));
    const int parsedRequest = requestedHarmonics(request);
    const int requested = std::max(1, parsedRequest);
    initializeBasic(unit, requested);
    const double selector = static_cast<double>(ZIN0(2));
    unit->kernel = finiteValue(selector)
        ? std::max(0, std::min(2, static_cast<int>(selector)))
        : 0;
    const double curve = static_cast<double>(ZIN0(4));
    unit->previousCurve =
        finiteValue(curve) ? clamp(curve, 1.0, 8.0) : 2.0;
    unit->currentCosineWeights[0] = 0.0;
    unit->targetCosineWeights[0] = 0.0;
    for (int power = 1; power <= 8; ++power) {
        unit->currentCosineWeights[power] =
            cosinePowerWeight(unit->currentHarmonics, power);
        unit->targetCosineWeights[power] =
            unit->currentCosineWeights[power];
    }
    ZOUT0(0) = finiteValue(frequency) && parsedRequest != 0
            && finiteValue(selector) && finiteValue(amount)
            && finiteValue(curve)
        ? 1.0f : 0.0f;
}

// BlipPeak -------------------------------------------------------------------

struct BlipPeak : BasicPhaseUnit {};

inline double peakValue(double phase, int harmonics, double center) noexcept {
    const double peak = fejerPeak(phase, harmonics);
    const double dc = 1.0 / static_cast<double>(harmonics + 1);
    const double centered = (peak - dc) / (1.0 - dc);
    return lerp(peak, centered, clamp(center, 0.0, 1.0));
}

void BlipPeak_next(BlipPeak* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    const InputView center = inputView(unit, 2);
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double c = center.at(index);
        if (!finiteValue(f) || !finiteValue(c)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advanceCarrier(phase, f, unit->radiansPerSample);
            continue;
        }
        double value = peakValue(phase, nextHarmonics, c);
        if (transition)
            value = lerp(peakValue(phase, previousHarmonics, c), value,
                         transitionAt(index, inNumSamples));
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }

    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
}

void BlipPeak_Ctor(BlipPeak* unit) {
    SETCALC(BlipPeak_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0
            && finiteValue(static_cast<double>(ZIN0(2)))
        ? 1.0f : 0.0f;
}

// BlipDecay ------------------------------------------------------------------

struct BlipDecay : BasicPhaseUnit {};

void BlipDecay_next(BlipDecay* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    const InputView decay = inputView(unit, 2);
    const bool staticDecay = decay.stride == 0 && finiteValue(decay.at(0));
    const double staticRatio =
        staticDecay ? clamp(decay.at(0), 0.0, 1.0) : 0.0;
    const double nextRatioPower = staticDecay
        ? std::pow(staticRatio, nextHarmonics) : 0.0;
    const double nextWeight = staticDecay
        ? geometricWeight(staticRatio, nextHarmonics) : 1.0;
    const double previousRatioPower = staticDecay
        ? std::pow(staticRatio, previousHarmonics) : 0.0;
    const double previousWeight = staticDecay
        ? geometricWeight(staticRatio, previousHarmonics) : 1.0;
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double ratio = decay.at(index);
        if (!finiteValue(f) || !finiteValue(ratio)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advanceCarrier(phase, f, unit->radiansPerSample);
            continue;
        }
        const double r = clamp(ratio, 0.0, 1.0);
        double value = staticDecay
            ? geometricCosSumPrepared(
                  phase, nextHarmonics, staticRatio,
                  nextRatioPower, nextWeight)
            : geometricCosSum(phase, nextHarmonics, r);
        if (transition)
            value = lerp(
                staticDecay
                    ? geometricCosSumPrepared(
                          phase, previousHarmonics, staticRatio,
                          previousRatioPower, previousWeight)
                    : geometricCosSum(
                          phase, previousHarmonics, r),
                value, transitionAt(index, inNumSamples));
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }

    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
}

void BlipDecay_Ctor(BlipDecay* unit) {
    SETCALC(BlipDecay_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0
            && finiteValue(static_cast<double>(ZIN0(2)))
        ? 1.0f : 0.0f;
}

// BlipFrac -------------------------------------------------------------------

struct BlipFrac : Unit {
    double phase;
    double radiansPerSample;
};

void BlipFrac_next(BlipFrac* unit, int inNumSamples) {
    float* out = OUT(0);
    const InputView frequency = inputView(unit, 0);
    const InputView requested = inputView(unit, 1);
    const double safeNyquist =
        0.5 * static_cast<double>(SAMPLERATE) * kNyquistMargin;
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double request = requested.at(index);
        if (!finiteValue(f) || !finiteValue(request)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advanceCarrier(phase, f, unit->radiansPerSample);
            continue;
        }
        double harmonics = clamp(request, 1.0,
                                 static_cast<double>(kMaxHarmonics));
        if (std::abs(f) > std::numeric_limits<double>::min())
            harmonics = std::min(
                harmonics,
                std::max(1.0, safeNyquist / std::abs(f)));
        const double value = fractionalCosSum(phase, harmonics);
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }
    unit->phase = phase;
}

void BlipFrac_Ctor(BlipFrac* unit) {
    SETCALC(BlipFrac_next);
    unit->phase = 0.0;
    unit->radiansPerSample = kTwoPi / static_cast<double>(SAMPLERATE);
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && finiteValue(static_cast<double>(ZIN0(1)))
        ? 1.0f : 0.0f;
}

// BlipDiff -------------------------------------------------------------------

struct BlipDiff : BasicPhaseUnit {};

void BlipDiff_next(BlipDiff* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        if (!finiteValue(f)) {
            out[index] = 0.0f;
            continue;
        }
        double value =
            weightedDerivativeSinSum(phase, nextHarmonics);
        if (transition)
            value = lerp(
                weightedDerivativeSinSum(phase, previousHarmonics),
                value, transitionAt(index, inNumSamples));
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }
    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
}

void BlipDiff_Ctor(BlipDiff* unit) {
    SETCALC(BlipDiff_next);
    const double request = static_cast<double>(ZIN0(1));
    initializeBasic(unit,
                    std::max(1, requestedHarmonics(request)));
    ZOUT0(0) = 0.0f;
}

// BlipPM ---------------------------------------------------------------------

struct BlipPM : BasicPhaseUnit {};

void BlipPM_next(BlipPM* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    const InputView phaseInput = inputView(unit, 2);
    double carrierPhase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double modulation = phaseInput.at(index);
        if (!finiteValue(f) || !finiteValue(modulation)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advanceCarrier(carrierPhase, f, unit->radiansPerSample);
            continue;
        }
        const double evaluationPhase =
            wrapPhase(carrierPhase + wrapPhase(modulation));
        double value =
            dirichletCosSum(evaluationPhase, nextHarmonics);
        if (transition)
            value = lerp(
                dirichletCosSum(evaluationPhase, previousHarmonics),
                value, transitionAt(index, inNumSamples));
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(carrierPhase, f, unit->radiansPerSample);
    }
    unit->phase = carrierPhase;
    unit->currentHarmonics = nextHarmonics;
}

void BlipPM_Ctor(BlipPM* unit) {
    SETCALC(BlipPM_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    const double modulation = static_cast<double>(ZIN0(2));
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0 && finiteValue(modulation)
        ? static_cast<float>(
              dirichletCosSum(wrapPhase(modulation),
                              unit->currentHarmonics))
        : 0.0f;
}

// BlipFB ---------------------------------------------------------------------

struct BlipFB : BasicPhaseUnit {
    double previousRaw;
};

void BlipFB_next(BlipFB* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        unit->previousRaw = 0.0;
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    const InputView feedback = inputView(unit, 2);
    double phase = unit->phase;
    double previousRaw = zap(unit->previousRaw);

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double gain = feedback.at(index);
        if (!finiteValue(f) || !finiteValue(gain)) {
            out[index] = 0.0f;
            previousRaw = 0.0;
            if (finiteValue(f))
                advanceCarrier(phase, f, unit->radiansPerSample);
            continue;
        }

        const double fraction = transitionAt(index, inNumSamples);
        const double inverseN = transition
            ? lerp(1.0 / static_cast<double>(previousHarmonics),
                   1.0 / static_cast<double>(nextHarmonics), fraction)
            : 1.0 / static_cast<double>(nextHarmonics);
        const double displaced =
            wrapPhase(phase + wrapPhase(gain * previousRaw * inverseN));
        double raw = dirichletCosSum(displaced, nextHarmonics);
        if (transition)
            raw = lerp(
                dirichletCosSum(displaced, previousHarmonics),
                raw, fraction);
        previousRaw = zap(raw);
        out[index] = static_cast<float>(previousRaw);
        advanceCarrier(phase, f, unit->radiansPerSample);
    }
    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
    unit->previousRaw = previousRaw;
}

void BlipFB_Ctor(BlipFB* unit) {
    SETCALC(BlipFB_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    unit->previousRaw = 0.0;
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0
            && finiteValue(static_cast<double>(ZIN0(2)))
        ? 1.0f : 0.0f;
}

// BlipWidth ------------------------------------------------------------------

struct BlipWidth : BasicPhaseUnit {
    double state;
    double previousWidth;
    int needsAnchor;
};

inline double widthPhaseDerivative(double phase, int harmonics,
                                   double width) noexcept {
    const double offset = kPi * width;
    return (2.0 * static_cast<double>(harmonics) / kPi)
        * (dirichletCosSum(phase + offset, harmonics)
           - dirichletCosSum(phase - offset, harmonics));
}

inline double widthParameterDerivative(double phase, int harmonics,
                                       double width) noexcept {
    const double offset = kPi * width;
    return 2.0 * static_cast<double>(harmonics)
        * (dirichletCosSum(phase + offset, harmonics)
           + dirichletCosSum(phase - offset, harmonics));
}

inline void updateWidthStateForParameter(double& state, double phase,
                                         int harmonics, double oldWidth,
                                         double newWidth) noexcept {
    const double difference = newWidth - oldWidth;
    if (difference == 0.0)
        return;
    const double midpoint = 0.5 * (oldWidth + newWidth);
    state += difference
        * widthParameterDerivative(phase, harmonics, midpoint);
}

inline void advanceWidthState(double& state, double& phase, int harmonics,
                              double width, double frequency,
                              double radiansPerSample) noexcept {
    const double increment = frequency * radiansPerSample;
    if (!finiteValue(increment))
        return;

    const double nextPhase = wrapPhase(phase + increment);
    const bool crossed = phase + increment >= kPi
        || phase + increment < -kPi
        || std::abs(increment) >= kTwoPi;

    // Midpoint integration is accurate for ordinary oscillator increments.
    // Large increments are rare after harmonic limiting and are cheaper and
    // safer to re-anchor exactly than to integrate across a wrapped cycle.
    if (crossed || std::abs(increment) > 0.25) {
        phase = nextPhase;
        state = widthSeries(phase, harmonics, width);
        return;
    }

    const double midpointPhase = wrapPhase(phase + 0.5 * increment);
    state += increment
        * widthPhaseDerivative(midpointPhase, harmonics, width);
    phase = nextPhase;
}

void BlipWidth_next(BlipWidth* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        unit->needsAnchor = 1;
        return;
    }

    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    const InputView widthInput = inputView(unit, 2);
    double phase = unit->phase;
    double oldState = unit->state;
    double newState = transition
        ? widthSeries(phase, nextHarmonics, unit->previousWidth)
        : oldState;
    double previousWidth = unit->previousWidth;
    int needsAnchor = unit->needsAnchor;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        const double widthValue = widthInput.at(index);
        if (!finiteValue(f) || !finiteValue(widthValue)) {
            out[index] = 0.0f;
            if (finiteValue(f))
                advancePhase(phase, f * unit->radiansPerSample);
            needsAnchor = 1;
            continue;
        }

        const double width =
            clamp(widthValue, kWidthEpsilon, 1.0 - kWidthEpsilon);
        if (needsAnchor != 0) {
            oldState = widthSeries(phase, previousHarmonics, width);
            if (transition)
                newState = widthSeries(phase, nextHarmonics, width);
            previousWidth = width;
            needsAnchor = 0;
        }

        updateWidthStateForParameter(
            oldState, phase, previousHarmonics, previousWidth, width);
        if (transition)
            updateWidthStateForParameter(
                newState, phase, nextHarmonics, previousWidth, width);

        double raw = transition
            ? lerp(oldState, newState, transitionAt(index, inNumSamples))
            : oldState;
        raw *= widthPeakScale(width);
        out[index] = finiteValue(raw) ? static_cast<float>(raw) : 0.0f;

        double oldPhase = phase;
        advanceWidthState(oldState, oldPhase, previousHarmonics, width,
                          f, unit->radiansPerSample);
        if (transition) {
            double newPhase = phase;
            advanceWidthState(newState, newPhase, nextHarmonics, width,
                              f, unit->radiansPerSample);
            phase = newPhase;
        } else {
            phase = oldPhase;
        }
        previousWidth = width;
    }

    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
    unit->state = zap(transition ? newState : oldState);
    unit->previousWidth = previousWidth;
    unit->needsAnchor = needsAnchor;
}

void BlipWidth_Ctor(BlipWidth* unit) {
    SETCALC(BlipWidth_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    const double widthInput = static_cast<double>(ZIN0(2));
    unit->previousWidth = finiteValue(widthInput)
        ? clamp(widthInput, kWidthEpsilon, 1.0 - kWidthEpsilon)
        : 0.1;
    unit->state =
        widthSeries(0.0, unit->currentHarmonics, unit->previousWidth);
    unit->needsAnchor = 0;
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0 && finiteValue(widthInput)
        ? static_cast<float>(
              unit->state * widthPeakScale(unit->previousWidth))
        : 0.0f;
}

// BlipAD ---------------------------------------------------------------------

struct BlipAD : BasicPhaseUnit {
    double attackState;
    double decayState;
    double previousAttackCoefficient;
    double previousDecayCoefficient;
    double previousNormalization;
};

inline double timeCoefficient(double seconds, double sampleRate) noexcept {
    seconds = std::max(seconds, 1.0 / sampleRate);
    return std::exp(-1.0 / (seconds * sampleRate));
}

inline double impulseValue(double phase, int harmonics) noexcept {
    return fullDirichlet(phase, harmonics)
        / static_cast<double>(2 * harmonics + 1);
}

void BlipAD_next(BlipAD* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    const double attackInput = static_cast<double>(ZIN0(2));
    const double decayInput = static_cast<double>(ZIN0(3));
    if (requested == 0 || !finiteValue(attackInput) || !finiteValue(decayInput)) {
        clearOutput(unit, inNumSamples);
        return;
    }

    const double sampleRate = static_cast<double>(SAMPLERATE);
    const double attack = std::max(attackInput, 1.0 / sampleRate);
    const double decay =
        std::max(decayInput, attack + 1.0 / sampleRate);
    const double targetAttackCoefficient =
        timeCoefficient(attack, sampleRate);
    const double targetDecayCoefficient =
        timeCoefficient(decay, sampleRate);
    const double targetNormalization =
        onePoleDifferenceNormalization(targetAttackCoefficient,
                                       targetDecayCoefficient);

    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    double phase = unit->phase;
    double attackState = unit->attackState;
    double decayState = unit->decayState;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        if (!finiteValue(f)) {
            out[index] = 0.0f;
            continue;
        }
        const double fraction = transitionAt(index, inNumSamples);
        const double attackCoefficient =
            lerp(unit->previousAttackCoefficient,
                 targetAttackCoefficient, fraction);
        const double decayCoefficient =
            lerp(unit->previousDecayCoefficient,
                 targetDecayCoefficient, fraction);
        const double normalization =
            lerp(unit->previousNormalization,
                 targetNormalization, fraction);

        double excitation = impulseValue(phase, nextHarmonics);
        if (transition)
            excitation = lerp(
                impulseValue(phase, previousHarmonics), excitation,
                fraction);
        attackState = attackCoefficient * attackState
            + (1.0 - attackCoefficient) * excitation;
        decayState = decayCoefficient * decayState
            + (1.0 - decayCoefficient) * excitation;
        attackState = zap(attackState);
        decayState = zap(decayState);
        const double value =
            normalization * (decayState - attackState);
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }

    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
    unit->attackState = attackState;
    unit->decayState = decayState;
    unit->previousAttackCoefficient = targetAttackCoefficient;
    unit->previousDecayCoefficient = targetDecayCoefficient;
    unit->previousNormalization = targetNormalization;
}

void BlipAD_Ctor(BlipAD* unit) {
    SETCALC(BlipAD_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    const double sampleRate = static_cast<double>(SAMPLERATE);
    const double attackInput = static_cast<double>(ZIN0(2));
    const double decayInput = static_cast<double>(ZIN0(3));
    const double attack = finiteValue(attackInput)
        ? std::max(attackInput, 1.0 / sampleRate)
        : 1.0 / sampleRate;
    const double decay = finiteValue(decayInput)
        ? std::max(decayInput, attack + 1.0 / sampleRate)
        : attack + 1.0 / sampleRate;
    unit->previousAttackCoefficient =
        timeCoefficient(attack, sampleRate);
    unit->previousDecayCoefficient =
        timeCoefficient(decay, sampleRate);
    unit->previousNormalization =
        onePoleDifferenceNormalization(
            unit->previousAttackCoefficient,
            unit->previousDecayCoefficient);
    unit->attackState = 0.0;
    unit->decayState = 0.0;
    const double excitation =
        impulseValue(0.0, unit->currentHarmonics);
    const double initialAttack =
        (1.0 - unit->previousAttackCoefficient) * excitation;
    const double initialDecay =
        (1.0 - unit->previousDecayCoefficient) * excitation;
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0 && finiteValue(attackInput)
            && finiteValue(decayInput)
        ? static_cast<float>(
              unit->previousNormalization
              * (initialDecay - initialAttack))
        : 0.0f;
}

// BlipDisp -------------------------------------------------------------------

struct BlipDisp : BasicPhaseUnit {
    double* allocation;
    double* currentCosine;
    double* currentSine;
    double* targetCosine;
    double* targetSine;
    double currentDispersion;
};

inline void normalizeComplex(double& real, double& imag) noexcept {
    const double magnitudeSquared = real * real + imag * imag;
    if (magnitudeSquared <= std::numeric_limits<double>::min()) {
        real = 1.0;
        imag = 0.0;
        return;
    }
    const double inverseMagnitude = 1.0 / std::sqrt(magnitudeSquared);
    real *= inverseMagnitude;
    imag *= inverseMagnitude;
}

void fillDispersionCoefficients(double* cosine, double* sine,
                                double dispersion) noexcept {
    dispersion = wrapPhase(dispersion);
    ComplexPair value = cis(dispersion);
    ComplexPair step = cis(3.0 * dispersion);
    const ComplexPair stepStep = cis(2.0 * dispersion);

    for (int index = 0; index < kMaxHarmonics; ++index) {
        cosine[index] = value.real;
        sine[index] = value.imag;
        value = complexMultiply(value, step);
        step = complexMultiply(step, stepStep);
        if ((index & 63) == 63) {
            normalizeComplex(value.real, value.imag);
            normalizeComplex(step.real, step.imag);
        }
    }
}

inline double dispersionValue(double phase, int harmonics,
                              const double* cosineOffsets,
                              const double* sineOffsets) noexcept {
    double sine;
    double cosine;
    sincosPair(phase, sine, cosine);
    const double sineStep = sine;
    const double cosineStep = cosine;
    double sum = 0.0;
    for (int index = 0; index < harmonics; ++index) {
        sum += cosine * cosineOffsets[index]
            - sine * sineOffsets[index];
        const double nextCosine =
            cosine * cosineStep - sine * sineStep;
        sine = sine * cosineStep + cosine * sineStep;
        cosine = nextCosine;
    }
    return sum / static_cast<double>(harmonics);
}

void BlipDisp_next(BlipDisp* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    const double dispersionInput = static_cast<double>(ZIN0(2));
    if (requested == 0 || !finiteValue(dispersionInput)) {
        clearOutput(unit, inNumSamples);
        return;
    }

    const double nextDispersion = wrapPhase(dispersionInput);
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition =
        previousHarmonics != nextHarmonics
        || nextDispersion != unit->currentDispersion;
    if (transition)
        fillDispersionCoefficients(
            unit->targetCosine, unit->targetSine, nextDispersion);

    const InputView frequency = inputView(unit, 0);
    double phase = unit->phase;
    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        if (!finiteValue(f)) {
            out[index] = 0.0f;
            continue;
        }
        double value = transition
            ? dispersionValue(phase, nextHarmonics,
                              unit->targetCosine, unit->targetSine)
            : dispersionValue(phase, nextHarmonics,
                              unit->currentCosine, unit->currentSine);
        if (transition) {
            const double oldValue =
                dispersionValue(phase, previousHarmonics,
                                unit->currentCosine,
                                unit->currentSine);
            value = lerp(oldValue, value,
                         transitionAt(index, inNumSamples));
        }
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }

    if (transition) {
        std::swap(unit->currentCosine, unit->targetCosine);
        std::swap(unit->currentSine, unit->targetSine);
    }
    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
    unit->currentDispersion = nextDispersion;
}

void BlipDisp_Dtor(BlipDisp* unit) {
    if (unit->allocation != nullptr)
        RTFree(unit->mWorld, unit->allocation);
}

void BlipDisp_Ctor(BlipDisp* unit) {
    unit->allocation = nullptr;
    unit->currentCosine = nullptr;
    unit->currentSine = nullptr;
    unit->targetCosine = nullptr;
    unit->targetSine = nullptr;
    SETCALC(BlipDisp_next);
    const double request = static_cast<double>(ZIN0(1));
    const int parsedRequest = requestedHarmonics(request);
    initializeBasic(unit, std::max(1, parsedRequest));
    unit->allocation = static_cast<double*>(
        RTAlloc(unit->mWorld,
                static_cast<std::size_t>(4 * kMaxHarmonics)
                    * sizeof(double)));
    ClearUnitIfMemFailed(unit->allocation);
    unit->currentCosine = unit->allocation;
    unit->currentSine =
        unit->currentCosine + kMaxHarmonics;
    unit->targetCosine =
        unit->currentSine + kMaxHarmonics;
    unit->targetSine =
        unit->targetCosine + kMaxHarmonics;

    const double dispersionInput = static_cast<double>(ZIN0(2));
    unit->currentDispersion =
        finiteValue(dispersionInput) ? wrapPhase(dispersionInput) : 0.0;
    fillDispersionCoefficients(
        unit->currentCosine, unit->currentSine,
        unit->currentDispersion);
    fillDispersionCoefficients(
        unit->targetCosine, unit->targetSine,
        unit->currentDispersion);
    ZOUT0(0) = finiteValue(static_cast<double>(ZIN0(0)))
            && parsedRequest != 0 && finiteValue(dispersionInput)
        ? static_cast<float>(
              dispersionValue(0.0, unit->currentHarmonics,
                              unit->currentCosine,
                              unit->currentSine))
        : 0.0f;
}

// BlipInt --------------------------------------------------------------------

double inverseHarmonics[kMaxHarmonics + 1];
double harmonicNumbers[kMaxHarmonics + 1];

struct BlipInt : BasicPhaseUnit {};

inline void inverseSums(double phase, int firstCount, int secondCount,
                        double& first, double& second) noexcept {
    const int maximum = std::max(firstCount, secondCount);
    double sine;
    double cosine;
    sincosPair(phase, sine, cosine);
    const double sineStep = sine;
    const double cosineStep = cosine;
    double sum = 0.0;
    first = 0.0;
    second = 0.0;

    for (int k = 1; k <= maximum; ++k) {
        sum += sine * inverseHarmonics[k];
        if (k == firstCount)
            first = sum / harmonicNumbers[firstCount];
        if (k == secondCount)
            second = sum / harmonicNumbers[secondCount];
        const double nextCosine =
            cosine * cosineStep - sine * sineStep;
        sine = sine * cosineStep + cosine * sineStep;
        cosine = nextCosine;
    }
}

void BlipInt_next(BlipInt* unit, int inNumSamples) {
    float* out = OUT(0);
    const int requested = requestedHarmonics(static_cast<double>(ZIN0(1)));
    if (requested == 0) {
        clearOutput(unit, inNumSamples);
        return;
    }
    const int nextHarmonics =
        blockHarmonics(unit, 0, requested, inNumSamples);
    const int previousHarmonics = unit->currentHarmonics;
    const bool transition = previousHarmonics != nextHarmonics;
    const InputView frequency = inputView(unit, 0);
    double phase = unit->phase;

    for (int index = 0; index < inNumSamples; ++index) {
        const double f = frequency.at(index);
        if (!finiteValue(f)) {
            out[index] = 0.0f;
            continue;
        }
        double oldValue;
        double newValue;
        inverseSums(phase, previousHarmonics, nextHarmonics,
                    oldValue, newValue);
        const double value = transition
            ? lerp(oldValue, newValue,
                   transitionAt(index, inNumSamples))
            : newValue;
        out[index] = finiteValue(value) ? static_cast<float>(value) : 0.0f;
        advanceCarrier(phase, f, unit->radiansPerSample);
    }
    unit->phase = phase;
    unit->currentHarmonics = nextHarmonics;
}

void BlipInt_Ctor(BlipInt* unit) {
    SETCALC(BlipInt_next);
    const double request = static_cast<double>(ZIN0(1));
    initializeBasic(unit,
                    std::max(1, requestedHarmonics(request)));
    ZOUT0(0) = 0.0f;
}

} // namespace

PluginLoad(BlipExtensions) {
    ft = inTable;

    inverseHarmonics[0] = 0.0;
    harmonicNumbers[0] = 0.0;
    for (int k = 1; k <= kMaxHarmonics; ++k) {
        inverseHarmonics[k] = 1.0 / static_cast<double>(k);
        harmonicNumbers[k] =
            harmonicNumbers[k - 1] + inverseHarmonics[k];
    }

    DefineSimpleUnit(BlipSeries);
    DefineSimpleUnit(BlipKernel);
    DefineSimpleUnit(BlipPeak);
    DefineSimpleUnit(BlipDecay);
    DefineSimpleUnit(BlipFrac);
    DefineSimpleUnit(BlipWidth);
    DefineDtorCantAliasUnit(BlipDisp);
    DefineSimpleUnit(BlipAD);
    DefineSimpleUnit(BlipDiff);
    DefineSimpleUnit(BlipInt);
    DefineSimpleUnit(BlipPM);
    DefineSimpleUnit(BlipFB);
}

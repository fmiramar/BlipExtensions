#!/usr/bin/env python3
"""Analyze the deterministic render without third-party Python packages."""

from __future__ import annotations

import array
import math
import pathlib
import sys
import wave


ROOT = pathlib.Path(__file__).resolve().parent
RENDER = ROOT / "renders" / "blip_extensions_spectral.wav"
SAMPLE_RATE = 48_000
FUNDAMENTAL = 375.0
HARMONICS = 20
CHANNELS = 14


def fail(message: str) -> None:
    raise AssertionError(message)


def read_render() -> list[list[float]]:
    if not RENDER.exists():
        fail(f"missing render: {RENDER}; run render_tests.scd first")
    with wave.open(str(RENDER), "rb") as source:
        if source.getframerate() != SAMPLE_RATE:
            fail(f"unexpected sample rate {source.getframerate()}")
        if source.getnchannels() != CHANNELS:
            fail(f"unexpected channel count {source.getnchannels()}")
        if source.getsampwidth() != 2:
            fail("analyzer expects the int16 reference render")
        samples = array.array("h")
        samples.frombytes(source.readframes(source.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    frames = len(samples) // CHANNELS
    return [
        [samples[frame * CHANNELS + channel] / 32768.0 for frame in range(frames)]
        for channel in range(CHANNELS)
    ]


def goertzel(signal: list[float], frequency: float) -> float:
    omega = 2.0 * math.pi * frequency / SAMPLE_RATE
    coefficient = 2.0 * math.cos(omega)
    previous = 0.0
    previous_two = 0.0
    last = len(signal) - 1
    for index, sample in enumerate(signal):
        window = 0.5 - 0.5 * math.cos(2.0 * math.pi * index / last)
        current = sample * window + coefficient * previous - previous_two
        previous_two = previous
        previous = current
    power = (
        previous * previous
        + previous_two * previous_two
        - coefficient * previous * previous_two
    )
    return math.sqrt(max(0.0, power))


def harmonic_magnitudes(signal: list[float], maximum: int = 50) -> list[float]:
    # Discard constructor/transient samples. The Hann window keeps the check
    # valid if a backend returns one frame more or less than requested.
    region = signal[16384:49152]
    return [
        goertzel(region, FUNDAMENTAL * harmonic)
        for harmonic in range(1, maximum + 1)
    ]


def normalized(values: list[float]) -> list[float]:
    maximum = max(values)
    return [value / maximum for value in values] if maximum > 0.0 else values


def maximum_error(actual: list[float], expected: list[float]) -> float:
    return max(abs(a - b) for a, b in zip(actual, expected))


def main() -> int:
    data = read_render()
    if any(not math.isfinite(value) for channel in data for value in channel):
        fail("render contains NaN or infinity")

    spectra = [harmonic_magnitudes(channel) for channel in data]
    rect = normalized(spectra[1][:HARMONICS])
    rect_ripple = maximum_error(rect, [1.0] * HARMONICS)
    if rect_ripple > 0.025:
        fail("rectangular kernel harmonic magnitudes are not equal")

    series = normalized(spectra[0][:HARMONICS])
    if maximum_error(series, rect) > 0.025:
        fail("BlipSeries 1,1 does not match the rectangular spectrum")

    triangle_expected = normalized(
        [1.0 - (k - 1) / HARMONICS for k in range(1, HARMONICS + 1)]
    )
    triangle = normalized(spectra[2][:HARMONICS])
    if maximum_error(triangle, triangle_expected) > 0.035:
        fail("triangular kernel does not follow its linear weight law")

    cosine_expected = normalized(
        [
            math.cos(math.pi * (k - 1) / (2.0 * HARMONICS)) ** 4
            for k in range(1, HARMONICS + 1)
        ]
    )
    cosine = normalized(spectra[3][:HARMONICS])
    if maximum_error(cosine, cosine_expected) > 0.035:
        fail("cosine kernel does not follow its fourth-power law")

    decay_expected = normalized([0.8 ** (k - 1) for k in range(1, 21)])
    decay = normalized(spectra[5][:20])
    if maximum_error(decay, decay_expected) > 0.035:
        fail("BlipDecay does not follow its geometric weight law")

    frac = normalized(spectra[6][:15])
    if maximum_error(frac[:12], [1.0] * 12) > 0.035:
        fail("BlipFrac full partials are not equal")
    if not 0.45 <= frac[12] <= 0.55:
        fail(f"BlipFrac final partial was {frac[12]:.3f}, expected 0.5")
    if max(frac[13:]) > 0.035:
        fail("BlipFrac emitted partials above the fractional final partial")

    width_expected = normalized(
        [
            abs(math.sin(math.pi * k * 0.25) / k)
            for k in range(1, HARMONICS + 1)
        ]
    )
    width = normalized(spectra[7][:HARMONICS])
    active_errors = [
        abs(actual - expected)
        for actual, expected in zip(width, width_expected)
        if expected > 1.0e-6
    ]
    spectral_zeros = [
        actual
        for actual, expected in zip(width, width_expected)
        if expected <= 1.0e-6
    ]
    if max(active_errors) > 0.07:
        fail("BlipWidth does not follow the finite sinc envelope")
    if max(spectral_zeros) > 0.05:
        fail("BlipWidth expected spectral zeros are too large")

    dispersion = normalized(spectra[8][:HARMONICS])
    if maximum_error(dispersion, rect) > 0.035:
        fail("BlipDisp changed harmonic magnitudes")

    diff_expected = normalized([float(k) for k in range(1, HARMONICS + 1)])
    diff = normalized(spectra[9][:HARMONICS])
    if maximum_error(diff, diff_expected) > 0.04:
        fail("BlipDiff does not follow k weighting")

    integral_expected = normalized(
        [1.0 / float(k) for k in range(1, HARMONICS + 1)]
    )
    integral = normalized(spectra[10][:HARMONICS])
    if maximum_error(integral, integral_expected) > 0.04:
        fail("BlipInt does not follow inverse-harmonic weighting")

    pm = normalized(spectra[11][:HARMONICS])
    feedback_zero = normalized(spectra[12][:HARMONICS])
    if maximum_error(pm, rect) > 0.035:
        fail("static BlipPM changed carrier magnitudes")
    if maximum_error(feedback_zero, rect) > 0.035:
        fail("BlipFB feedback zero changed the carrier spectrum")

    out_of_band = [
        max(normalized(spectrum)[HARMONICS:])
        for spectrum in spectra[:13]
    ]
    if max(out_of_band) > 0.04:
        fail(
            "strong unexpected partial above harmonic limit: "
            f"{max(out_of_band):.4f}"
        )

    power_above_base = max(normalized(spectra[13])[HARMONICS:])
    if power_above_base < 0.005:
        fail("BlipPower did not demonstrate nonlinear harmonics")

    print("BLIP_EXTENSIONS_SPECTRAL_OK")
    print(f"rectangular ripple={rect_ripple:.6f}")
    print(f"maximum limited out-of-band ratio={max(out_of_band):.6f}")
    print(f"BlipPower above-base ratio={power_above_base:.6f}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"BLIP_EXTENSIONS_SPECTRAL_FAILED: {error}", file=sys.stderr)
        sys.exit(1)

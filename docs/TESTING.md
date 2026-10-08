# Verification

The project separates formula tests from installed-server tests.

## Automated native checks

`tests/reference/BlipDSPTests.cpp` compares every shared closed form against
direct additive evaluation, including counts through 2048, phase-wrap limits,
geometric ratios near one, weight laws, normalization bounds, signed
frequency limiting, and explicit-series finiteness.

The same test runs in Release, strict-warning, AddressSanitizer, and
UndefinedBehaviorSanitizer builds.

## Installed server checks

- `tests/compile_check.scd`: constructs every class, all audio-rate `mul` and
  `add` paths, selector validation, and discrete-rate rejection.
- `tests/BlipExtensionsTests.scd`: captures all classes from scsynth, checks
  finite/non-silent output, verifies `BlipPM(phase: 0)` against the
  rectangular kernel, and proves `BlipFB` recursion is unchanged by
  language-side `mul` and `add`.
- `tests/supernova_smoke.scd`: captures all thirteen public classes using the
  supernova binary built against the exact target SuperCollider plugin API.
- `tests/nonfinite_inputs.scd`: injects NaN and positive/negative infinity
  into every native family and into `BlipPower`.
- `tests/prototype_check.scd`: loads all thirteen mathematical prototypes,
  builds one SynthDef from each, and exercises the offline scalar `BlipFB`
  reference.
- `tests/script_parse_check.scd`: compiles every shipped sclang prototype,
  example, benchmark, and test script without executing it.
- `tests/block_sample_rate_matrix.scd`: renders block sizes 1, 16, 64, and 128
  at 44.1, 48, 96, and 192 kHz with signed sweeps, rapid topology changes,
  maximum requests, extreme phase/feedback, and edge widths.
- `tests/long_run_stateful.scd`: opt-in ten-minute render for `BlipWidth`,
  `BlipAD`, and `BlipFB`, followed by finite-value, peak, and mean checks.
- `tests/verify_scdoc.scd`: fully reindexes SCDoc and renders all thirteen
  class pages plus the project guide.

## Spectral checks

`tests/render_tests.scd` writes a deterministic 48 kHz NRT file.
`tests/analyze_renders.py` uses only the Python standard library and a
Hann-windowed Goertzel bank to check:

- equal rectangular weights and the `BlipSeries(1, 1)` match;
- triangular, cosine-power, and exponential laws;
- the half-amplitude final `BlipFrac` partial;
- `BlipWidth`'s finite sinc envelope and spectral zeros;
- unchanged magnitudes under quadratic dispersion and static PM;
- derivative and inverse-harmonic laws;
- feedback-zero equivalence;
- absence of strong content above the carrier limit;
- expected nonlinear harmonics from non-oversampled `BlipPower`.

The verified reference render measured 0.000243 maximum normalized ripple in
the rectangular harmonics and 0.000181 maximum normalized content above the
limited carrier set.

## Verified local run

The final 2026-07-30 build passed strict Release and sanitizer reference
tests, scsynth and supernova live captures, deterministic-zero non-finite
input checks, all 16 sample-rate/block-size renders, prototype compilation,
script parsing, a full PE32+ x86-64 MinGW-w64 plugin cross-build, and spectral
analysis. The ten-minute stateful render contained no non-finite samples and
measured a peak of 1.3339675664902 and mean of 0.00013580576154152. All
fourteen SCDoc pages indexed and rendered without a warning or error.

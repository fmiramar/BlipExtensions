# BlipExtensions benchmark results

## Native DSP microbenchmark

Measured on:

- macOS 15.7.2, x86_64;
- Intel Core i5, 4.1 GHz, 6 cores;
- AppleClang 17.0.0;
- Release build;
- 200,000 calls per constant-cost case;
- steady-clock wall time, reported as nanoseconds per generated sample.

The benchmark consumes every result through a volatile sink. It excludes UGen
buffer access, graph scheduling, `.madd`, audio-driver work, and topology
changes, so it is a scaling test rather than a promise of server CPU.

| DSP path | N=8 | N=32 | N=128 | N=512 | N=2048 |
|---|---:|---:|---:|---:|---:|
| arithmetic series | 26.46 | 27.44 | 27.60 | 27.43 | 27.42 |
| rectangular kernel / PM base | 32.95 | 34.07 | 34.70 | 34.03 | 34.09 |
| triangular kernel | 33.86 | 34.70 | 35.16 | 34.60 | 35.00 |
| cosine-power-4 kernel | 183.33 | 184.14 | 181.17 | 179.26 | 179.79 |
| Fejér peak | 29.86 | 30.62 | 30.96 | 30.23 | 31.04 |
| decay, block-static coefficient | 38.63 | 39.34 | 39.94 | 55.85 | 39.35 |
| decay, audio-rate coefficient | 103.74 | 104.87 | 99.68 | 119.52 | 97.11 |
| fractional count | 40.18 | 41.94 | 41.39 | 42.02 | 41.88 |
| derivative kernel | 39.31 | 39.70 | 40.12 | 41.10 | 39.84 |
| width phase derivative | 58.99 | 61.46 | 62.27 | 61.03 | 60.85 |
| attack-decay state step | 43.46 | 44.54 | 45.00 | 44.53 | 44.89 |
| feedback state step | 40.10 | 42.01 | 41.98 | 41.39 | 41.56 |
| dispersion recurrence, O(N) | 70.31 | 293.37 | 1,159.88 | 4,713.57 | 18,779.66 |
| inverse-harmonic recurrence, O(N) | 36.84 | 102.67 | 405.37 | 1,584.78 | 6,341.54 |

All values are ns/sample. The isolated 55.85 ns static-decay value at N=512
is measurement noise: this path has the same operation count for every N, and
the neighboring counts return to roughly 39 ns.

### Interpretation

The intended O(1) paths remain essentially flat from 8 to 2048 harmonics.
Cosine power is also constant in N but is deliberately heavier: power four
evaluates five shifted finite kernels. Cached per-topology normalization cut
its measured cost from about 750 ns/sample in the first implementation to
about 180 ns/sample.

Static exponential decay caches `r**N` and the finite normalization once per
block. This reduced its steady path from roughly 83 ns/sample to about
39 ns/sample. Audio-rate decay must recompute the finite geometric
coefficients and measures around 100 ns/sample.

The two explicit recurrences show the intended scaling. From N=8 to N=2048,
dispersion grows from 70.31 to 18,779.66 ns/sample and integration from 36.84
to 6,341.54 ns/sample. At 48 kHz, the N=2048 microbenchmark corresponds to
roughly 90% of one core for dispersion and 30% for integration before server
overhead, so those extreme settings are reference/installation-scale cases,
not sensible high-polyphony defaults.

At the same 48 kHz rate, the rectangular path represents about 0.16% of one
core per instance in this isolated test, cosine-power four about 0.86%, and
audio-rate exponential decay about 0.47%.

## Real-time server benchmark

`benchmarks/benchmark.scd` measures absolute scsynth average and peak CPU for
one, 100, and 1000 instances, harmonic counts 8, 32, 128, 512, and 2048, and
static versus modulated cases. It includes `Blip`, `Pulse`, and `Mix.fill`
comparisons and prints the server, sample rate, block size, operating system,
architecture, compiler label, and build type with the results.

Real-time results are intentionally not normalized into percentages against a
single baseline: server CPU depends strongly on audio hardware, background
load, graph scheduling, and whether an O(N) case misses its deadline. Preserve
the complete printed header when publishing a run.

# Benchmarks

`benchmark.scd` measures real-time scsynth CPU for one, 100, and 1000
instances at harmonic counts 8, 32, 128, 512, and 2048 where graph size is
practical. It includes `Blip`, `Pulse`, and an additive `Mix.fill` baseline.
It also includes a static `Klang` bank where graph size remains practical.
Run it on an otherwise idle audio system and preserve the printed environment
header with results.

`dsp_benchmark.cpp` is a native formula microbenchmark. Configure with
`-DBLIP_EXTENSIONS_BUILD_BENCHMARKS=ON`, build Release, and run
`blip_extensions_dsp_benchmark`. It is intended to verify scaling:
constant-cost rows should remain nearly flat across harmonic counts, while
`dispersion-O(N)` and `integral-O(N)` should grow approximately linearly.

Microbenchmarks are not substitutes for server measurements: they exclude
graph scheduling, input buffers, `.madd` operators, audio I/O, and topology
changes.

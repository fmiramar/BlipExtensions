# BlipExtensions

BlipExtensions is an experimental oscillator family built around finite harmonic kernels, extending SuperCollider's `Blip` architecture with new spectral weighting, pulse shapes, and phase topologies. The code does not replace core oscillators but provides double-precision, analytically bounded variants for experimental synthesis.

## UGens

- **BlipSeries**: Places partials at an arithmetic series of frequency ratios using independent phase accumulators to preserve continuity.
- **BlipKernel**: Selects rectangular, triangular, or cosine-tapered harmonic weights using a bounded expansion.
- **BlipPeak**: Produces a nonnegative Fejér pulse with an analytic phase-wrap limit.
- **BlipDecay**: Applies a finite exponential spectral envelope using a stable complex geometric series.
- **BlipFrac**: Makes harmonic count sample-continuous by fading the final partial.
- **BlipWidth**: Reconstructs a bandlimited rectangular pulse with an independent harmonic limit by integrating exact phase derivatives.
- **BlipDisp**: Applies quadratic phase to the partials using an O(N) constructor-allocated recurrence.
- **BlipAD**: Excites two one-pole responses with a normalized finite impulse train.
- **BlipDiff**: Computes the bound-normalized phase derivative of the rectangular kernel.
- **BlipInt**: Applies inverse-harmonic weighting to produce a broad, ramp-like signal via O(N) recursion.
- **BlipPower**: A language-side pseudo-UGen that power-shapes `BlipKernel` for nonlinear harmonics.
- **BlipPM**: Applies sample-accurate phase displacement to the finite kernel.
- **BlipFB**: Applies true one-sample raw-output phase feedback.

## Build and Install

Requires CMake 3.15+, C++17, and a SuperCollider source tree:

```sh
cmake -S . -B build -DSC_PATH=/path/to/supercollider -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix "/path/to/SuperCollider/Extensions"
```

Recompile the SuperCollider class library after installation.

## Known Limits

`BlipDisp` and `BlipInt` are O(N) (up to 2048 partials). Modulations in `BlipPower`, `BlipPM`, and `BlipFB` can create unbandlimited sidebands.

## License and Origins

BlipExtensions is licensed under GPL-3.0-or-later. The architecture is heavily informed by James McCartney's `Blip`, `Saw`, `Pulse`, and `SinOscFB` implementations from SuperCollider's core `server/plugins/OscUGens.cpp`. BlipExtensions retains the finite-kernel concepts but uses independent double-precision phase and direct analytic evaluators. See `ORIGINS.md` for explicit upstream references.

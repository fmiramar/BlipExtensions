# Origins and design notes

## Immediate references

- SuperCollider `server/plugins/OscUGens.cpp`, current `develop` and version
  3.14.1: `Blip`, `Saw`, `Pulse`, `SinOscFB`, phase tables, harmonic limiting,
  and one-block topology transitions.
- SuperCollider `SCClassLibrary/Common/Audio/FSinOsc.sc`: public `Blip`,
  `Saw`, and `Pulse` wrapper conventions.
- SuperCollider plugin interface headers: registration, real-time allocation,
  calculation-rate inspection, and destructor conventions.

Official project: <https://github.com/supercollider/supercollider>

## Historical audit

The implementation review followed `OscUGens.cpp` history to its 2002 source.
Particularly relevant fixes were:

- 2008: deterministic initial state for `Saw` and `Pulse`;
- 2009: force `Blip` harmonic count to at least one;
- 2009: initialize `Pulse` phase-offset state;
- 2022: restore `Blip` phase and topology state after calculating the
  constructor sample.

BlipExtensions initializes every field explicitly and computes constructor
samples without mutating the state used by the first processing block.

## What was retained

- finite Dirichlet-kernel evaluation as the baseline;
- Nyquist-derived effective harmonic counts;
- smooth transitions when a discrete harmonic topology changes;
- a two-edge interpretation for finite-width pulses;
- raw one-sample oscillator output as recursive phase-feedback state.

## What was changed

- double-precision radian phase replaces the signed fixed-point accumulator;
- standard trigonometric functions and analytic sinc limits replace the
  global sine/cosecant table and sentinel branches;
- negative and zero frequency are first-class cases;
- audio-rate inputs are read sample accurately rather than collapsed to the
  first value of a block;
- invalid inputs produce deterministic zero samples and cannot poison state;
- `BlipWidth` integrates exact finite-series derivatives and re-anchors to an
  O(N) reference at wraps instead of using an uncorrected leaky integrator;
- O(N) recurrence storage is allocated once and never changed by the audio
  callback.

No third-party plugin code is included.

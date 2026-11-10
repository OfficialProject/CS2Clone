# CS2Clone

A purpose-built competitive 5v5 FPS engine project. The long-term goal is a mechanically precise, server-authoritative tactical shooter; the current repository is an early engineering prototype, not a playable game.

## Current state

The first implementation milestone is a headless movement simulation, independent of graphics, audio, windowing, and networking.

Implemented:
- Fixed-step horizontal movement with acceleration, natural friction, active counter-strafing, and direction reversal.
- Ground/air movement modes, a minimal jump arc, velocity caps, and a crouch stance transition.
- Crouching lowers the target speed and gradually brakes existing momentum instead of snapping velocity to a lower cap.
- A provisional movement-accuracy signal derived from actual horizontal speed; no weapon-specific recovery timer.
- A command-line movement laboratory with 128/256 Hz and release/counter-strafe/crouch scenarios, plus dependency-free regression tests.

Not implemented:
- World geometry, player/world collision, slopes, steps, or authoritative hitboxes.
- Weapons, ballistics, networking, prediction/reconciliation, rendering, audio, maps, or game rules.
- Final movement tuning. Parameters are experimental, not balanced gameplay values.

## Build and test

Requires a C++23 compiler and CMake 3.25 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/movement_lab --rate 128 --scenario release
./build/movement_lab --rate 256 --scenario counter-strafe
```

## Principles

- Server simulation owns gameplay outcomes; clients may predict but never decide authority.
- Animation and rendering never define collision or hit registration.
- Competitive mechanics must arise from documented simulation rules, not mismatches or exploits.
- Measure before optimizing; keep simulation independently testable.
- Third-party dependencies require version, license, platform, and maintenance review before adoption.

See the docs directory for the architecture baseline and movement experiment notes. The current prototype is not evidence that the full engine exists.

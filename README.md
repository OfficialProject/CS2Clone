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

## Interactive movement playtest (Windows)

The optional **Movement Feel Lab** is an early first-person, mouse-and-keyboard test scene. It uses the repository's C++ fixed-step movement simulation at 128 Hz, with a simple 3D environment, basic wall blocking, five shootable targets, speed readout, and reset. This is **not yet** a networked game or authoritative Jolt collision integration; hits are currently visual target checks without weapon accuracy/recoil modeling. Collision is a deliberately simplified flat-wall prototype.

To download: open [Playtest build Actions](https://github.com/OfficialProject/CS2Clone/actions/workflows/playtest.yml), select a successful run, and download the **CS2Clone-Feel-Lab-Windows** artifact. Unzip both layers if necessary and run `feel_lab.exe`. GitHub may require sign-in to download artifacts.

Controls: WASD move, mouse look, left-click shoot, Space jump, Ctrl/C crouch, R reset, Esc toggle mouse capture, F5 record scripted movement drills. Compare stopping, counter-strafing and crouch momentum against your expectations. Values are provisional.

Local build (optional): `cmake -S . -B build -DCS2CLONE_BUILD_PLAYTEST=ON`, then `cmake --build build --config Release --target feel_lab`. This optional target fetches [raylib 5.5](https://github.com/raysan5/raylib) (zlib/libpng license); the default headless engine build remains dependency-free.

## Offline CS2 demo analysis

[Demo movement analysis](tools/demo_movement/README.md) documents the Python parser, input audit, CSV/JSON outputs, plots and tests. It runs independently of the C++ engine.

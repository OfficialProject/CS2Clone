# Dependency evaluation: Jolt Physics

**Status:** candidate only; not integrated or adopted.

## Why it is under consideration

We need robust static-world collision, shape casts, overlap queries, and geometry handling without outsourcing the game's movement rules. Jolt is a C++ collision and rigid-body library with a standalone API, rather than a complete game engine. The official repository describes support for Windows, Linux, macOS, iOS, Android, FreeBSD, and additional targets; x86/x64 requires SSE2 at minimum and ARM64 uses NEON. It documents Visual Studio 2022+, Clang 16+, and GCC 12+ as supported compiler families. The project is C++17 and MIT licensed.

Official sources:
- Repository and platform/compiler requirements: https://github.com/jrouwe/JoltPhysics
- Current stable release: https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0
- API changes: https://jrouwe.github.io/JoltPhysics/md__docs_2_a_p_i_changes.html

## Version policy

The latest stable release endpoint currently identifies v5.6.0, published 2026-07-11. The moving development branch contains API and serialized-state changes dated after that release, including changes in November 2026. We must pin a release tag or exact commit, never consume an unpinned master branch in the game build.

Jolt is MIT licensed. The license notice must be retained in distributions and included in the project's third-party notices if the dependency is adopted.

## Intended boundary

Jolt may provide:
- Static and dynamic world collision.
- Broad-phase and narrow-phase collision queries.
- Shape casts, overlap tests, and contact information.
- Optional rigid-body simulation for non-player world objects if later required.

The game must continue to own:
- Player acceleration, friction, counter-strafing, air control, stance transitions, and movement accuracy.
- Input command semantics and fixed-step simulation timing.
- Authoritative hitbox definitions and the mapping from simulation state to presentation.
- Network prediction, reconciliation, lag compensation, and server authority.

Jolt types should remain behind a narrow collision adapter. No Jolt type should leak into weapon, game-rule, network protocol, or presentation interfaces.

## Controller approaches to compare

**A. Custom kinematic controller using Jolt queries**

The game owns the entire player movement state machine and uses Jolt for collision queries and shape casts. This gives us direct control over movement and collision response but requires us to implement robust step handling, slopes, ground detection, sliding, contact resolution, and moving-platform behavior.

**B. Jolt CharacterVirtual**

Evaluate Jolt's virtual character controller as a collision/movement substrate. It must not be adopted merely because it exists: its update order, collision response, state capture, stance shape changes, and behavior at competitive movement edge cases must be measured against approach A.

The final choice is deferred until both approaches can run the same geometry and movement test suite.

## Adoption gates

- Pin exact source tag/commit and record a reproducible build configuration.
- Review the exact release license and all included notices.
- Build and run the official unit tests on Windows, Linux, and macOS.
- Add a minimal collision adapter and measure query cost at realistic player and map complexity.
- Test corners, narrow passages, steps, slopes, ledges, ceilings, crouch clearance, moving platforms, and overlapping player bodies.
- Verify identical-build replay and document any cross-platform determinism limits.
- Compare custom kinematic movement against CharacterVirtual using the same movement inputs and collision scenes.
- Confirm the library does not introduce unwanted gameplay semantics or force the player controller to inherit rigid-body behavior.

**Decision:** keep Jolt as the leading collision-library candidate, but do not add it to the dependency graph until the controller comparison and geometry tests are implemented.

# Dependency evaluation: Jolt Physics

**Status:** leading candidate; not integrated or adopted.

## Why it is under consideration

We need robust static-world collision, shape casts, overlap queries, and geometry handling without outsourcing the game's movement rules. Jolt is a C++ collision and rigid-body library with a standalone API, rather than a complete game engine. The official repository documents support for Windows, Linux, macOS, iOS, Android, FreeBSD, and additional targets; x86/x64 requires SSE2 at minimum and ARM64 uses NEON. It documents Visual Studio 2022+, Clang 16+, and GCC 12+ as supported compiler families. The library is C++17 and MIT licensed.

Official sources:
- Repository and platform/compiler requirements: https://github.com/jrouwe/JoltPhysics
- Current stable release: https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0
- API changes: https://jrouwe.github.io/JoltPhysics/md__docs_2_a_p_i_changes.html
- CharacterVirtual API at v5.6.0: https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/Character/CharacterVirtual.h

## Version policy

The latest stable release endpoint currently identifies v5.6.0, published 2026-07-11. The moving development branch contains API and serialized-state changes dated after that release, including changes in November 2026. We must pin a release tag or exact commit, never consume an unpinned master branch in the game build.

The v5.6.0 LICENSE file grants commercial use, modification, distribution, and sublicensing under the MIT terms. The copyright and permission notices must be retained in copies or substantial portions of the library and included in third-party notices if adopted.

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

CharacterVirtual is not a rigid body in the main physics simulation. The application chooses when to update it and supplies its movement velocity; Jolt performs collision checks and constrains movement. Its API includes explicit stair walking, floor sticking, and shape switching with a penetration check, which could help with stance transitions.

Important trade-offs from the v5.6.0 API:
- CharacterVirtual is not tracked by PhysicsSystem; the application must update it.
- PhysicsSystem::SaveState does not capture it. CharacterVirtual has separate SaveState and RestoreState methods, so replay/prediction must capture both.
- The application remains responsible for applying gravity to its velocity.
- Other physics objects do not automatically see a CharacterVirtual as a rigid body. Player-to-player collision requires a separate CharacterVsCharacterCollision path or an inner rigid-body proxy.
- ExtendedUpdate combines movement, floor sticking, and stair walking, but its defaults and behavior must be tested against our movement rules rather than accepted blindly.

The final choice is deferred until both approaches can run the same geometry and movement test suite.

## Adoption gates

- Pin exact source tag/commit and record a reproducible build configuration.
- Review the exact release license and all included notices.
- Build and run the official unit tests on Windows, Linux, and macOS.
- Add a minimal collision adapter and measure query cost at realistic player and map complexity.
- Test corners, narrow passages, steps, slopes, ledges, ceilings, crouch clearance, moving platforms, and overlapping player bodies.
- Verify identical-build replay and document any cross-platform determinism limits.
- Compare custom kinematic movement against CharacterVirtual using the same movement inputs and collision scenes.
- Verify CharacterVirtual state capture and restore alongside the authoritative simulation state.
- Confirm the library does not introduce unwanted gameplay semantics or force the player controller to inherit rigid-body behavior.

**Decision:** keep Jolt v5.6.0 as the leading collision-library candidate, but do not add it to the dependency graph until the controller comparison and geometry tests are implemented.

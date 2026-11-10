# Jolt collision laboratory v0.1

**Status:** isolated experiment on the experiment/jolt-character-controller branch; not merged into the main engine.

## Purpose

Evaluate Jolt Physics 5.6.0 CharacterVirtual as a possible collision substrate while keeping the game's movement rules outside the physics library. The main build remains dependency-free by default. The optional laboratory fetches one pinned Jolt release and is built separately in CI.

## What the experiment owns

- A minimal Jolt world containing static test geometry.
- A CharacterVirtual capsule with explicit ground support and slope settings.
- Application-controlled horizontal velocity and gravity integration.
- Jolt's ExtendedUpdate path for collision resolution, floor sticking, and step traversal.
- Standing/crouched capsule shapes with penetration-checked shape changes.
- Explicit CharacterVirtual state save/restore and replay checks.

The experiment does not use Jolt to decide acceleration, friction, counter-strafing, air control, weapon accuracy, hitboxes, or networking.

## Automated cases

1. Falling onto a static floor and verifying supported ground state.
2. Driving into a wall and verifying the capsule cannot pass through it.
3. Traversing a three-step staircase using a bounded step-up setting.
4. Rejecting a standing-shape transition beneath a low ceiling while retaining the crouched shape.
5. Saving, restoring, and replaying CharacterVirtual state in a static world.

## Compatibility and deterministic-build choices

The experiment pins Jolt v5.6.0 and enables Jolt's CROSS_PLATFORM_DETERMINISTIC option. Jolt's optional SSE4.1, SSE4.2, AVX, AVX2, AVX512, LZCNT, TZCNT, F16C, and FMADD paths are disabled so the test build remains compatible with the SSE2 baseline on x86. This is a compatibility baseline, not a final performance configuration. Runtime CPU dispatch or multiple optimized builds can be evaluated later without raising the minimum CPU requirement.

## Known limitations

- This is not yet the game's player controller and does not replace the existing 2D movement prototype.
- Only static box geometry is used. No BSP/mesh map collision, dynamic doors, moving platforms, player-to-player collision, or map streaming is covered.
- The current tests do not establish cross-platform bit-identical CharacterVirtual replay. They only test save/restore within one build.
- CharacterVirtual is not a rigid body in the main PhysicsSystem. It requires application-managed updates and explicit state capture; other rigid bodies do not automatically collide with it unless a separate character-collision path or inner-body proxy is used.
- The lab has not yet been benchmarked for CPU cost, memory, contact count, or worst-case map complexity.

## Decision gate

Do not merge the Jolt adapter into the main engine until the optional CI build is green, the geometry cases are stable, and a custom kinematic controller using Jolt queries has been compared against CharacterVirtual on the same scenes. The main branch continues to build and test without fetching Jolt.

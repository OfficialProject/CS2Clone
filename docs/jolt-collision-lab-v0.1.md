# Jolt collision laboratory v0.1

**Status:** isolated experiment on the experiment/jolt-character-controller branch; not merged into the main engine.

## Purpose

Evaluate Jolt Physics 5.6.0 CharacterVirtual as a possible collision substrate while keeping the game's movement rules outside the physics library. The main build remains dependency-free by default. The optional laboratory fetches one pinned Jolt release and is built separately in CI.

## What the experiment owns

- A minimal Jolt world containing static test geometry.
- A CharacterVirtual capsule with explicit ground support, slope settings, and a 0.02-unit collision padding.
- Application-controlled horizontal velocity and gravity integration.
- Jolt's ExtendedUpdate path for collision resolution, floor sticking, and step traversal.
- Standing/crouched capsule shapes with penetration-checked shape changes.
- A prototype symmetric capsule-pair resolver, compared against Jolt's sequential CharacterVirtual collision interface.
- Explicit CharacterVirtual state save/restore and replay checks.

The experiment does not use Jolt to decide acceleration, friction, counter-strafing, air control, weapon accuracy, hitboxes, or networking.

## Automated cases

1. Falling onto a static floor and verifying supported ground state.
2. Driving into a wall and verifying the capsule cannot pass through it.
3. Traversing a three-step staircase using a bounded step-up setting.
4. Rejecting a standing-shape transition beneath a low ceiling while retaining the crouched shape.
5. Comparing Jolt's sequential CharacterVirtual collision interface with an engine-owned symmetric capsule-pair resolver under reversed update order:
   - Equal head-on movement (2, -2 simulation units/s).
   - One character moving into a stationary character (2, 0).
   - Unequal head-on movement (1, -3).
6. Stress-testing ten players in a line, with five moving toward five, under forward and reverse update order.
7. Saving, restoring, and replaying CharacterVirtual state in a static world.

## First measured player-collision baseline

The pair scenarios ran at 128 Hz. The ten-player case also ran for 128 ticks. The same measurements were observed on Windows, Linux, and macOS.

| Scenario | Jolt simple interface: left/right order delta | Symmetric resolver: left/right order delta |
|---|---:|---:|
| Equal head-on | 0.000625253 / 0.000625253 | 0 / 0 |
| Moving into stationary | 0 / 0.015625 | 0 / 0 |
| Unequal head-on | 0.0156381 / 0.000013113 | 0 / 0 |
| Ten-player group, maximum delta | 0.0166194 | 0 |

For the ten-player group, minimum capsule separation was 0.64 simulation units with Jolt's simple interface and 0.639712 with the symmetric resolver. The resolver uses the same 0.02-unit padding and 32 deterministic pair-resolution passes; its remaining difference from the measured Jolt separation is 0.000288 simulation units.

The Jolt interface prevents overlap and produces identical measurements across the three tested platforms. However, asymmetric two-player cases and the ten-player group expose a small, repeatable update-order difference. In the moving-versus-stationary case, 0.015625 simulation units equals one 2-unit/s movement step at 128 Hz. A fixed player update order would make this repeatable, but it would also give a systematic priority to whichever player is updated first or last.

The prototype symmetric post-step resolver removes the measured update-order difference in these open-floor tests and keeps the ten-player group within 0.000288 units of the Jolt interface's minimum separation. This does **not** establish that it is ready for gameplay: it has not yet been tested against walls, stairs, moving geometry, or a full world-collision projection pass.

## Compatibility and deterministic-build choices

The experiment pins Jolt v5.6.0 and enables Jolt's CROSS_PLATFORM_DETERMINISTIC option. Jolt's optional SSE4.1, SSE4.2, AVX, AVX2, AVX512, LZCNT, TZCNT, F16C, and FMADD paths are disabled so the test build remains compatible with the SSE2 baseline on x86. Unused DirectX, Vulkan, Metal, CPU-compute, and ObjectStream features are disabled. The Jolt library uses the same dynamic MSVC runtime as the engine to avoid mixing CRT heaps across static-library boundaries. The optional collision lab builds and passes CI on Windows, Linux, and macOS. This is a compatibility baseline, not a final performance configuration. Runtime CPU dispatch or multiple optimized builds can be evaluated later without raising the minimum CPU requirement.

## Known limitations

- This is not yet the game's player controller and does not replace the existing 2D movement prototype.
- Only static box geometry is used. No BSP/mesh map collision, dynamic doors, moving platforms, ramps/slopes, or map streaming is covered.
- The current tests do not establish cross-platform bit-identical CharacterVirtual replay. They only test save/restore within one build; the matching player-collision measurements are a narrower cross-platform check.
- CharacterVirtual is not a rigid body in the main PhysicsSystem. It requires application-managed updates and explicit state capture; other rigid bodies do not automatically collide with it unless a separate character-collision path or inner-body proxy is used.
- The custom pair resolver has been exercised with two-player and ten-player open-floor scenarios. It has not been tested with walls, stairs, moving geometry, or network prediction.
- The lab has not yet been benchmarked for CPU cost, memory, contact count, or worst-case map complexity.

## Decision gate

Do not merge the Jolt adapter into the main engine until the optional CI build is green on Windows, Linux, and macOS, the geometry cases are stable, the custom player-collision policy is validated against world geometry and moving geometry, and a custom kinematic controller using Jolt queries has been compared against CharacterVirtual on the same scenes. The main branch continues to build and test without fetching Jolt.

# Engine architecture baseline v0.1

**Status:** proposed architecture; only the headless movement experiment is implemented.

## Objective

Build a purpose-designed competitive 5v5 bomb-defusal FPS. Priorities are responsive and predictable control, trustworthy collision and hit registration, high mechanical skill expression, server-authoritative outcomes, stable frame pacing, broad PC compatibility, and reproducible testing.

A custom engine provides control, not automatic correctness or performance. We own game-specific simulation and renderer architecture while reusing mature libraries where they do not compromise the design.

## Non-negotiable boundaries

1. Simulation must run headlessly without a GPU, renderer, audio device, or window.
2. The server owns accepted world state, collision, weapon outcomes, and game rules.
3. Client prediction improves responsiveness but never becomes authority.
4. Animation and rendered poses derive from simulation; they never define collision.
5. Competitive advantages must not come from hitbox, animation, or networking mismatches.
6. Dependencies stay behind narrow interfaces and require license, platform, maintenance, and build review.
7. Performance decisions require representative measurements.

## Provisional technology choices

| Area | Current position |
|---|---|
| Language | C++23 provisional |
| Build | CMake provisional |
| Graphics API | Vulkan selected as the initial target |
| Graphics abstraction | Diligent Core candidate; must remain behind our renderer interface |
| Collision/physics | Jolt candidate; custom kinematic movement rules remain game-owned |
| Network transport | Valve GameNetworkingSockets candidate; gameplay netcode remains custom |
| Shader authoring | HLSL provisional |
| ECS / job system | Deferred until workload evidence exists |

No candidate dependency is considered adopted until its version, transitive licenses, maintenance, platform support, build reproducibility, and replacement path are checked.

## System boundaries

- **Core:** time, diagnostics, memory and later task scheduling.
- **Simulation:** fixed-step clock, movement, collision, stance, weapons, rules, replay.
- **Networking:** transport adapter, input sequencing, snapshots, prediction, reconciliation, authority, lag compensation.
- **Presentation:** renderer backend, animation, audio, UI.
- **Content/tools:** asset processing, movement lab, network impairment lab, replay inspection, profiling.

## Simulation and movement hypotheses

Start with a fixed-step simulation and compare 128 Hz with 256 Hz. Ground release uses gradual friction; opposite input actively brakes; direction reversal crosses zero velocity consistently. Air control and velocity remain bounded. Crouching is a real stance transition with costs and collision consequences, not a magic accuracy switch or hardcoded special move.

Movement accuracy must derive from actual simulation state rather than key-up events or animation. A low-velocity threshold for full standing accuracy is a hypothesis to test. No arbitrary weapon-specific recovery timer is assumed. Weapon-specific intrinsic precision, recoil, and spread remain separate.

The current executable prototype is narrower than this design: it has no world collision, slopes, stairs, hitboxes, networking, or weapon simulation. See movement-model-v0.1.md.

## Networking hypothesis

Use server authority with client prediction and reconciliation. Commands are sequenced; snapshots identify simulation time; unacknowledged inputs are replayed after reconciliation. Lag compensation has explicit rewind and timestamp bounds. Latency, jitter, packet loss, and reordering must be testable. Transport choice does not determine the gameplay protocol.

## Renderer hypothesis

Vulkan is the initial graphics API. The game owns a narrow renderer interface, so Diligent types cannot leak into gameplay or broad engine code. Actual GPU and driver capabilities must be checked at runtime. Older-PC support is an explicit compatibility target, not an assumption based only on GPU age.

## First milestones

1. Headless movement simulation and regression tests.
2. Collision-backed player controller and geometry test suite.
3. Movement laboratory with parameter sweeps and replay visualization.
4. Authoritative networking prototype with prediction/reconciliation and impairment testing.
5. Renderer, content pipeline, audio, and tools.
6. Weapons, economy, utility, maps, bomb-defusal rules, and competitive services.

This document is an engineering baseline, not evidence that the planned systems have been built or benchmarked.

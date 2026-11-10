# Movement model experiment v0.1

**Status:** first executable hypothesis; not final gameplay tuning.

## Scope

This module is a headless, fixed-step, two-dimensional horizontal movement experiment with a minimal vertical jump arc. It is intended to make the first movement hypotheses executable and regression-testable. It is not yet a complete player controller: there is no world geometry, collision resolution, slopes, stairs, moving platforms, player-to-player collision, stance clearance, animation, weapon model, or networking.

## Rules currently implemented

1. Inputs are clamped to the signed unit range. Diagonal input is normalized so it does not provide a larger wish speed.
2. Ground acceleration approaches the input direction's target speed, subject to a configurable acceleration limit.
3. Releasing directional input applies proportional friction, producing gradual deceleration rather than an immediate stop.
4. Opposite directional input actively accelerates against the existing velocity. In the current parameter set this brakes faster than passive friction.
5. Direction reversal passes through zero velocity through the same acceleration rule; there is no special reversal teleport or hidden state.
6. Ground and air horizontal speeds are capped. Air acceleration is lower than ground acceleration in the initial experiment.
7. Crouch is represented as a time-based stance fraction and reduces the target ground speed. It does not currently alter collision geometry or grant a special accuracy bonus.
8. The movement-accuracy output is a provisional continuous signal based on horizontal speed only. It is not a complete weapon-accuracy calculation and does not yet account for airborne state, stance, weapon characteristics, recoil, spread, or animation.
9. Jump is a per-step command supplied by the caller. A caller must not hold jump_pressed true across multiple simulation steps unless repeated jump attempts are intended.

## Initial parameters

All values live in MovementConfig and are experimental:

| Parameter | Initial value | Purpose |
|---|---:|---|
| Fixed step | 1/128 s | First comparison baseline |
| Ground maximum speed | 5.8 units/s | Horizontal ground cap |
| Crouch speed multiplier | 0.72 | Crouched target speed ratio |
| Ground acceleration | 38 units/s² | Active acceleration and braking |
| Ground friction | 8 s⁻¹ | Passive release deceleration |
| Air acceleration | 9 units/s² | Air control |
| Air maximum speed | 6.2 units/s | Airborne horizontal cap |
| Gravity | 20 units/s² | Prototype vertical arc |
| Jump speed | 6.5 units/s | Prototype initial vertical speed |
| Crouch transition | 0.12 s | Stance transition duration |
| Full-accuracy speed | 0.12 units/s | Lower threshold for provisional signal |
| Inaccurate speed | 1.8 units/s | Upper threshold for provisional signal |

These are not claims about real-world units, CS:GO/CS2 values, or a balanced final design. Parameters must be swept and evaluated using measured stopping distance, time-to-stop, reversal time, acceleration curves, air-control behavior, and competitive test scenarios.

## Known limitations and next work

- The controller currently has no collision interface. Collision must be introduced behind a deterministic query/response boundary and tested against a documented geometry suite.
- Crouch transitions are not collision-safe. A future shape transition must query available clearance and never allow visual pose and authoritative collision volume to disagree.
- The vertical jump is a minimal prototype and uses a flat ground plane. Landing and collision need a proper world model.
- The current same-build replay test is not cross-platform determinism evidence. Floating-point behavior and square-root implementations may vary across toolchains and architectures.
- No network prediction, reconciliation, rewind, or server-authority tests exist yet.
- The accuracy signal is diagnostic only and must not be wired into production weapons until grounded/airborne/stance semantics are specified and validated.

## Required experiments before tuning is accepted

- Compare 128 Hz and 256 Hz using equivalent real-time inputs; report position/velocity error, stop time, stop distance, CPU cost, and replay consistency.
- Sweep friction and acceleration independently; compare natural release against active counter-strafe for multiple initial speeds and directions.
- Test diagonal-to-cardinal and cardinal-to-diagonal changes to detect speed exploits or abrupt caps.
- Test crouch while moving, braking, and reversing. Measure the movement benefit and cost without adding a special-case move.
- Add collision geometry tests before evaluating any corner-peek or crouch-slide advantage.
- Instrument movement state and accuracy inputs so any future exploit can be attributed to actual simulation state rather than animation or hitbox mismatch.

# CS:GO movement model

The shared flat-ground simulator now implements Source-style friction and projection-based acceleration. The parameters come from the linked CS:GO snapshot's movevars_shared.cpp, gamemovement.cpp, cs_gamemovement.cpp and cs_shareddefs.cpp.

One Source distance unit maps to 5.8 / 250 scene units. Defaults use the snapshot's 250-unit reference running speed, ground acceleration 5.5, friction 5.2, stop-speed floor 80, air acceleration 12, air wish-component cap 30, gravity 800 and jump impulse 301.993377. Walking uses a 0.52 speed multiplier and crouching 0.34. Ground friction applies before input acceleration. Jump takeoff skips ground friction. Gravity uses symmetric half steps. The air cap applies to the velocity projection in the wish direction, allowing orthogonal momentum and air-strafe speed accumulation.

Shift walks by default, and Walk is rebindable in Settings. Walking includes the near-cap acceleration taper and speed-crop threshold from the snapshot. Crouch smoothly changes the requested maximum speed using the existing stance transition. The lab's temporary air-acceleration override has been removed.

## Validation and remaining scope

Simulation tests cover the stop-speed floor, preservation of orthogonal air momentum, the directional air-speed cap and airborne reversal, alongside existing movement regression tests.

This is the flat-floor locomotion model supported by the current scene. Full CS:GO movement also depends on a swept player hull, slopes, steps, ladders, water, surface properties, weapon-specific speed and scope state, stamina and duck fatigue. Those systems require further implementation. The current flat-wall collision and crouch transition remain prototype approximations, so this build is not a complete parity claim.

The earlier CS2 Inferno reports remain historical comparison measurements. CS:GO is now the movement reference.

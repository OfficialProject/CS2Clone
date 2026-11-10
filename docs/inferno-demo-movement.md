# Inferno demo movement findings

The offline pipeline analyzed `vitality-vs-mouz-m2-inferno.dem` from the supplied HLTV download. The header identifies a SourceTV replay on `de_inferno`. The SHA-256 is `c0af8117653be5876393e59448a0c352aeb9cd19e977eb3aad58e0896397ab8f`. The run used demoparser2 0.42.1, 64 Hz snapshots and a 5 demo units/s stop threshold.

| Measurement | Count | Median |
| --- | ---: | ---: |
| Acceleration intervals | 227,868 | +144.23 units/s² |
| Passive intervals | 12,308 | −416.35 units/s² |
| Counter-strafe intervals | 26,557 | −1,591.05 units/s² |
| Passive snapshot-to-threshold observations | 1,185 of 2,653 episodes | 31.25 ms |
| Counter snapshot-to-threshold observations | 1,546 of 5,125 episodes | 46.875 ms |

There were 19,805 strict world-axis sign crossings and 2,717 counter episodes with a crossing or observed zero along the initial velocity vector. These events describe different projections of motion. The pooled stop times combine weapons and starting speeds, and include only episodes that reached the threshold while still classifiable.

For AK-47 episodes starting at 100–150 demo units/s (upper bound exclusive), counter input reached the threshold in 240 of 723 episodes, with a median of 62.5 ms among hits. Release reached it in 15 of 120 episodes, with a median of 109.375 ms among hits. Censoring differs substantially, so this comparison is descriptive. The machine-readable summary includes other weapon and speed bins and the denominator for each.

## Input availability

The parser returned both velocity axes for 1,615,910 of 1,616,780 player snapshots. The entity button field was absent. Decoded command held masks were populated on 1,413,753 rows, with nonzero pressed and released masks on 56,546 and 60,157 rows respectively. View-angle input history was nonempty on 1,424,996 rows. The separate subtick movement field contained 1,034,189 directional button entries with valid fractions, counting repeated rows. For example, the same left-button press at fraction 0.578125 appeared in consecutive snapshots. Those repeated entries prevent treating each row as a fresh command. The results give input-to-stop time on the observed tick labels and preserve velocity-crossing interpolation separately.

## Implications for the clone

- The current grounded model in `src/simulation/movement.cpp` applies proportional friction only during neutral input and adds a constant acceleration increment when input is present. The existing `movement_lab` release and counter scenarios start at top speed and expose velocity at 128 or 256 Hz. Extend that lab with matched weapon and initial-speed cohorts, a rest-to-run case, and the same observation and censoring rules before comparing curve shapes. Normalize speed to cohort top speed or establish a unit conversion.
- Compare passive rate against speed within weapon cohorts. If the residuals show stronger braking at low speed than a proportional model can explain, test a low-speed floor in the friction calculation in the lab. Measure that candidate against the entire release curve, including its tail.
- Compare active reversal using the projected zero crossing and the full horizontal stop as separate outcomes. Evaluate the constant acceleration model against speed-binned counter rates and the observed crossing distribution before testing speed- or weapon-dependent braking. Keep accuracy thresholds separate from the 5 units/s analysis cutoff.
- Before experimenting with fractional-step input, capture commands with unique IDs and a known simulation clock in a controlled recording. Re-sample the clone to the same 64 Hz observation grid to compare its higher-rate internal simulation with demo observations.

The run leaves gameplay constants and runtime code unchanged.

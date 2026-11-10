# Movement baseline results v0.1

**Build:** local GCC 14.2 Release build of the initial headless movement prototype  
**Purpose:** compare the same scripted input duration at 128 Hz and 256 Hz  
**Status:** preliminary simulation output, not a gameplay balance or hardware-performance benchmark

## Configuration

- Ground maximum speed: 5.8 prototype units/s
- Ground acceleration: 38 prototype units/s²
- Ground friction: 8 s⁻¹
- Fixed step: 1/128 s or 1/256 s
- Input: hold forward for exactly one second, then either release or hold the opposite direction for exactly one second
- All coordinates and speeds are internal prototype units, not claimed real-world or CS units

## Natural release

| Metric | 128 Hz | 256 Hz |
|---|---:|---:|
| Speed at end of acceleration phase | 5.80000 | 5.80000 |
| Total forward displacement after 2 s | 6.05925 | 6.07081 |
| Final horizontal speed after 1 s of friction | 0.001499 | 0.001712 |
| Time after release to reach 0.12 speed | 0.46875 s | 0.47656 s |
| Additional displacement after release to reach 0.12 | 0.66642 | 0.68820 |

## Counter-strafe

| Metric | 128 Hz | 256 Hz |
|---|---:|---:|
| Speed at direction-change point | 5.80000 | 5.80000 |
| Time after reverse input to cross zero velocity | 0.14844 s | 0.15234 s |
| Time after reverse input to reach -5.0 speed | 0.28125 s | 0.28125 s |
| Net forward displacement after 2 s | 1.30503 | 1.31658 |

## Interpretation and limitations

- The two rates produce close results for these two scripted cases. The measured displacement difference is about 0.0116 prototype units over two seconds; this is not enough evidence to select a production simulation rate.
- Natural friction takes roughly 0.47 seconds to reach the current provisional 0.12 movement-accuracy threshold from the configured top speed. That threshold is a test parameter, not a confirmed weapon rule.
- Counter-strafing crosses zero in roughly 0.15 seconds and reaches -5.0 speed in roughly 0.28 seconds under the current acceleration setting.
- These numbers describe only the current mathematical prototype. They do not include collision, slopes, stairs, player contact, weapon accuracy, animation, network delay, input sampling, server load, or actual hardware performance.
- Do not tune the final game from these numbers alone. Use them as a repeatable baseline for later experiments and compare against measured player expectations and networked playtests.

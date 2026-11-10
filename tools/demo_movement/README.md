# CS2 demo movement analysis

This offline tool lives outside the C++ build and runtime. It reads a local CS2 demo with [LaihoE/demoparser](https://github.com/LaihoE/demoparser) and writes CSV, JSON and SVG. The demo is supplied by the operator.

## Reproduce

Use CPython 3.12 and a new output directory. The commands below use PowerShell from the repository root.

```powershell
py -3.12 -m venv .venv
& .venv/Scripts/python.exe -m pip install -r tools/demo_movement/requirements.txt
& .venv/Scripts/python.exe -m unittest discover -s tools/demo_movement -v
& .venv/Scripts/python.exe tools/demo_movement/analyze.py 'path/to/vitality-vs-mouz-m2-inferno.dem' --tick-rate 64 --out demo-results
```

The tick rate is explicit. demoparser2 derives its game-time field from net tick / 64. The tool checks that adjacent demo rows also advance by one net tick at the selected rate. Inspect the demo header and SHA-256 in the summary when comparing runs.

## Measurements

- Horizontal velocity is the change in X/Y position over one adjacent tick, timestamped at the later snapshot. The parser's own velocity fields are audited separately because upstream derives them from position history.
- Eligibility requires a live grounded player, outside freeze and warmup, with stable weapon, no walking, duck amount at most 0.01 and velocity modifier at least 0.99. Segments over 350 demo units/s are excluded to reduce discontinuities. Collision, slope and moving-platform effects can remain.
- Input classification uses same-source adjacent samples. It prefers the decoded command held mask, then raw command state 1, then the entity's previous-button-down mask. Missing values stay unknown. Yaw rotates the digital input direction into world space. An alignment cosine of at least 0.8 with prior motion and increasing speed identifies acceleration. No direction buttons during motion identifies passive intervals. A cosine of at most -0.8 identifies counter intervals, even if the player reverses between observations. The reported rate is the signed change in speed magnitude, so a reversal can yield a positive value.
- A stop episode starts at a sampled direction-mask change from nonneutral input into neutral or opposed input. Episodes end at the first observed horizontal speed at or below 5 demo units/s (configurable), a change in classification or button chord, a gap, or an eligibility boundary. Censored episodes have blank stop fields. The elapsed time is the difference between the input and stop **snapshot** labels, so zero milliseconds means both were observed at the same tick label. It cannot establish physical keypress latency.
- X/Y crossings are strictly opposite signs in adjacent velocity estimates. Counter episodes also track the first crossing or zero on the projection along starting motion. Interpolation assumes a linear change between two observations. An axis crossing can occur while total speed is nonzero. A reversal can cross zero between snapshots without a sampled threshold hit.
- View-angle input history and subtick movement are distinct parser properties. The former contains player/render tick indices, fractions and view angles. A directional subtick entry requires a single movement button bit, boolean pressed state and a fraction in [0, 1). Entity-held transitions provide tick-level evidence only. User command properties can persist across rows, so coverage counts are rows or entries observed, and may include repeated commands. Timing from subtick fractions requires validated command identity and alignment to server snapshots.

## Outputs

| File | Content |
| --- | --- |
| summary.json | Demo identity, parser version, coverage, classifications and median/count summaries |
| intervals.csv | Classified adjacent velocity pairs and signed speed-change rates |
| episodes.csv | Release and opposition episodes, threshold observations and censoring |
| crossings.csv | Strict X/Y sign changes and interpolated demo ticks |
| speed_delta_per_s.svg | Median signed speed change by classification |
| tick_delta_ms.svg | Median elapsed snapshot time among threshold hits |

Demo distances use demo units. The clone uses prototype units, and the analysis stop threshold is independent of weapon accuracy.

The [Inferno analysis notes](../../docs/inferno-demo-movement.md) record the measured HLTV results and proposed movement lab experiments. The JSON summary includes stop counts and medians by weapon and 50-unit/s starting-speed bands.

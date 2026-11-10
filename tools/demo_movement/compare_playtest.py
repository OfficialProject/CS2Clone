"""Score Movement Feel Lab traces on the demo observation grid."""
import argparse
import json
from pathlib import Path

import pandas as pd

from analyze import finite, snapshot_velocities, stop_cohorts, summarize, write_measurements

RATE = 64
TRACES = {("rest_to_run", 0)} | {
    (drill, percent)
    for drill in ("release", "counter_strafe")
    for percent in (50, 70, 90, 100)
}


def load_trace(path, reference_speed, weapon):
    frame = pd.read_csv(path)
    frame = frame.sort_values(["drill", "start_pct", "tick"])
    groups = frame.groupby(["drill", "start_pct"])
    if set(groups.groups) != TRACES:
        raise ValueError("Expected the nine Movement Feel Lab drills")
    for (drill, _), group in groups:
        if group["tick"].tolist() != list(range(-1, RATE + 2)):
            raise ValueError(f"Missing or duplicated snapshot in {drill}")
    top_speeds = frame["top_speed"].unique()
    if len(top_speeds) != 1 or not finite(top_speeds[0]) or top_speeds[0] <= 0:
        raise ValueError("Expected a single positive prototype top speed")
    scale = reference_speed / top_speeds[0]
    frame["X"], frame["Y"] = frame["x"] * scale, frame["y"] * scale
    frame["steamid"] = frame["drill"] + "_" + frame["start_pct"].astype(str)
    frame["game_time"] = frame["tick"] / RATE
    frame["yaw"] = frame["duck_amount"] = 0.0
    frame["velo_modifier"] = 1.0
    frame["is_alive"] = True
    for field in ("is_airborne", "is_warmup_period", "is_freeze_period", "is_walking"):
        frame[field] = False
    frame["active_weapon_name"] = weapon
    snapshot_velocities(frame, RATE)
    return frame, float(scale)


def compare_cohorts(clone_cohorts, demo, weapon):
    matching = {
        (group["kind"], group["start_speed_min"]): group
        for group in demo["stop_cohorts"] if group["weapon"] == weapon
    }
    return [
        {"playtest": group, "demo": matching.get((group["kind"], group["start_speed_min"]))}
        for group in clone_cohorts
    ]


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("trace", type=Path)
    cli.add_argument("--out", required=True, type=Path)
    cli.add_argument("--demo-summary", required=True, type=Path)
    cli.add_argument("--weapon", required=True, help="Reference demo weapon cohort")
    cli.add_argument("--reference-top-speed", required=True, type=float,
                     help="Explicit normalization anchor in demo units/s")
    args = cli.parse_args()
    if not finite(args.reference_top_speed) or args.reference_top_speed <= 0:
        cli.error("reference top speed must be finite and positive")
    demo = json.loads(args.demo_summary.read_text(encoding="utf-8"))
    if demo["tick_rate"] != RATE:
        cli.error("comparison requires a 64 Hz demo summary")
    if not any(group["weapon"] == args.weapon for group in demo["stop_cohorts"]):
        cli.error(f"no demo stop cohorts for {args.weapon}")
    frame, scale = load_trace(args.trace, args.reference_top_speed, args.weapon)
    counts = write_measurements(args.out, frame, RATE, demo["stop_speed"])
    result = {
        "schema_version": 1, "demo_sha256": demo["sha256"], "weapon": args.weapon,
        "tick_rate": RATE, "stop_speed": demo["stop_speed"],
        "reference_top_speed": args.reference_top_speed, "position_scale": scale,
        "counts": dict(counts), "summaries": summarize(args.out),
        "stop_cohorts": compare_cohorts(stop_cohorts(args.out), demo, args.weapon),
    }
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n",
                                          encoding="utf-8")


if __name__ == "__main__":
    main()

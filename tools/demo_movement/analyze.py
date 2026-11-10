"""Measure grounded movement from Counter-Strike 2 demo snapshots."""
import argparse
import csv
import hashlib
import html
import json
import math
from collections import Counter
from importlib.metadata import version
from itertools import groupby
from pathlib import Path
from statistics import median

FORWARD, BACK, LEFT, RIGHT = (1 << bit for bit in (3, 4, 9, 10))
MOVEMENT = FORWARD | BACK | LEFT | RIGHT
STANCE = (1 << 1) | (1 << 2) | (1 << 18)
STATE = ("velocity_X", "velocity_Y", "yaw", "is_alive", "is_airborne",
         "is_warmup_period", "is_freeze_period", "active_weapon_name")
POSITIONS = ("X", "Y", "game_time", "duck_amount", "is_walking", "velo_modifier")
OPTIONAL = ("buttons", "usercmd_buttonstate_1", "usercmd_buttonstate_2",
            "usercmd_buttonstate_3", "usercmd_buttons_held",
            "usercmd_buttons_pressed", "usercmd_buttons_released",
            "usercmd_forward_move", "usercmd_left_move", "usercmd_input_history",
            "usercmd_subtick_moves")
KINDS = ("acceleration", "passive", "counter")


def finite(value):
    try:
        return math.isfinite(value)
    except (TypeError, ValueError):
        return False


def mask(row):
    for field in ("usercmd_buttons_held", "usercmd_buttonstate_1", "buttons"):
        if finite(row.get(field)):
            return field, int(row[field])
    return None, 0


def eligible(row):
    return (row["is_alive"] == True and row["is_airborne"] == False
            and row["is_warmup_period"] == False and row["is_freeze_period"] == False
            and isinstance(row["active_weapon_name"], str)
            and all(finite(row[key]) for key in STATE[:3]))


def wish(bits, yaw):
    forward = bool(bits & FORWARD) - bool(bits & BACK)
    left = bool(bits & LEFT) - bool(bits & RIGHT)
    length = math.hypot(forward, left)
    if not length:
        return None
    angle = math.radians(yaw)
    c, s = math.cos(angle), math.sin(angle)
    return ((forward * c - left * s) / length, (forward * s + left * c) / length)


def measure(rows, hz, stop_speed, samples, episodes, crossings):
    """Rows for one player, in tick order. Write only classified intervals."""
    previous = None
    episode = None
    coverage = Counter()

    def finish(reason):
        if episode is not None:
            episodes.writerow({**episode, "end_reason": reason})

    for row in rows:
        if not eligible(row):
            finish("ineligible")
            previous = episode = None
            continue
        velocity = row["velocity_X"], row["velocity_Y"]
        speed = math.hypot(*velocity)
        source, bits = mask(row)
        tick, weapon = row["tick"], row["active_weapon_name"]
        current = (tick, weapon, source, bits, velocity, speed)
        if previous is None:
            previous = current
            continue
        old_tick, old_weapon, old_source, old_bits, old_velocity, old_speed = previous
        previous = current
        if tick != old_tick + 1 or weapon != old_weapon:
            finish("gap_or_weapon_change")
            episode = None
            continue
        player = row["steamid"]
        for axis, before, after in zip(("x", "y"), old_velocity, velocity):
            if before * after < 0:
                crossings.writerow((player, old_tick, tick, axis, before, after,
                                    old_tick - before / (after - before)))
                coverage["axis_crossings"] += 1
        if not source or source != old_source or (bits | old_bits) & STANCE:
            finish("input_or_stance")
            episode = None
            continue

        coverage["same_source_intervals"] += 1
        if (bits ^ old_bits) & MOVEMENT:
            coverage["directional_snapshot_changes"] += 1
        direction = wish(bits, row["yaw"])
        dot = (sum(a * b for a, b in zip(direction, old_velocity))
               if direction else 0)
        kind = None
        if old_speed > stop_speed and not bits & MOVEMENT:
            kind = "passive"
        elif old_speed > stop_speed and dot <= -0.8 * old_speed:
            kind = "counter"
        elif direction and (old_speed <= stop_speed or dot >= 0.8 * old_speed):
            if speed > old_speed:
                kind = "acceleration"

        if episode is not None:
            if kind != episode["kind"] or (bits & MOVEMENT) != episode["buttons"]:
                finish("classification_changed")
                episode = None
        if kind in ("passive", "counter") and episode is None:
            if bits & MOVEMENT != old_bits & MOVEMENT and old_bits & MOVEMENT:
                episode = dict(player=player, weapon=weapon, kind=kind, source=source,
                               buttons=bits & MOVEMENT, before_tick=old_tick,
                               input_tick=tick, start_speed=old_speed,
                               last_tick=tick, stop_tick=None, tick_delta_ms=None,
                               projected_zero_tick=None)
                initial_velocity = old_velocity
        if kind:
            samples.writerow((player, tick, weapon, kind, source, old_speed, speed,
                              (speed - old_speed) * hz))
            coverage[kind] += 1
        if episode is not None:
            episode["last_tick"] = tick
            before = sum(a * b for a, b in zip(initial_velocity, old_velocity))
            after = sum(a * b for a, b in zip(initial_velocity, velocity))
            if episode["projected_zero_tick"] is None and before > 0 >= after:
                episode["projected_zero_tick"] = old_tick + before / (before - after)
            if speed <= stop_speed:
                episode["stop_tick"] = tick
                episode["tick_delta_ms"] = 1000 * (tick - episode["input_tick"]) / hz
                finish("threshold")
                episode = None
    finish("end_of_player")
    return coverage


def entries(value):
    if isinstance(value, str):
        try:
            value = json.loads(value)
        except json.JSONDecodeError:
            return []
    if hasattr(value, "tolist"):
        value = value.tolist()
    return value if isinstance(value, (list, tuple)) else []


def audit(frame):
    result = {}
    for field in OPTIONAL:
        if field not in frame:
            result[field] = {"available": False}
            continue
        col = frame[field].dropna()
        record = {"available": True, "non_null_rows": len(col)}
        if field in ("usercmd_input_history", "usercmd_subtick_moves"):
            populated = transitions = 0
            examples = []
            for index, value in col.items():
                history = entries(value)
                populated += bool(history)
                if field == "usercmd_subtick_moves":
                    matched = [item for item in history if
                        isinstance(item, dict) and isinstance(item.get("button"), int)
                        and item["button"] > 0 and not item["button"] & (item["button"] - 1)
                        and bool(item["button"] & MOVEMENT)
                        and isinstance(item.get("pressed"), bool)
                        and finite(item.get("when")) and 0 <= item["when"] < 1
                    ]
                    transitions += len(matched)
                    if matched and len(examples) < 5:
                        examples.append({"tick": int(frame.at[index, "tick"]),
                                         "button": matched[0]["button"],
                                         "pressed": matched[0]["pressed"],
                                         "when": matched[0]["when"]})
            record["nonempty_rows"] = populated
            if field == "usercmd_subtick_moves":
                record["directional_button_entries"] = transitions
                record["examples"] = examples
        else:
            record["nonzero_rows"] = int((col != 0).sum())
        result[field] = record
    return result


def parse(path):
    from demoparser2 import DemoParser

    parser = DemoParser(str(path))
    try:
        frame = parser.parse_ticks(list(STATE + POSITIONS + OPTIONAL))
    except (ValueError, KeyError):
        frame = parser.parse_ticks(list(STATE + POSITIONS))
        for field in OPTIONAL:
            try:
                extra = parser.parse_ticks([field])
            except (ValueError, KeyError):
                continue
            if field in extra:
                keys = ["tick", "steamid"]
                frame = frame.merge(extra[keys + [field]], on=keys,
                                    how="left", validate="one_to_one")
    missing = set(STATE + POSITIONS) - set(frame)
    if missing:
        raise ValueError(f"Missing required demo fields: {sorted(missing)}")
    frame = frame.dropna(subset=["steamid", "tick"]).copy()
    # A repeated player/tick has ambiguous snapshot order. Drop the whole tick.
    frame = frame.drop_duplicates(["steamid", "tick"], keep=False)
    return parser.parse_header(), frame.sort_values(["steamid", "tick"])


SAMPLE_HEADER = ("player", "tick", "weapon", "kind", "source", "before_speed",
                 "speed", "speed_delta_per_s")
EPISODE_HEADER = ("player", "weapon", "kind", "source", "buttons", "before_tick",
                  "input_tick", "start_speed", "last_tick", "stop_tick",
                  "tick_delta_ms", "projected_zero_tick", "end_reason")
CROSSING_HEADER = ("player", "before_tick", "after_tick", "axis", "before",
                   "after", "interpolated_tick")


def summarize(out):
    results = {kind: {} for kind in KINDS}
    for filename, field, unit in (
        ("intervals.csv", "speed_delta_per_s", "demo units/s²"),
        ("episodes.csv", "tick_delta_ms", "ms"),
    ):
        values = {kind: [] for kind in KINDS}
        with (out / filename).open(encoding="utf-8", newline="") as handle:
            for row in csv.DictReader(handle):
                if row[field]:
                    values[row["kind"]].append(float(row[field]))
        for kind, series in values.items():
            results[kind][field] = {
                "count": len(series), "median": median(series) if series else None,
            }
        plot(out / f"{field}.svg", field.replace("_", " "), unit, values)
    return results


def stop_cohorts(out):
    cohorts = {}
    with (out / "episodes.csv").open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            start_bin = int(float(row["start_speed"]) // 50) * 50
            key = (row["weapon"], row["kind"], start_bin)
            group = cohorts.setdefault(key, {"episodes": 0, "stops": [], "projected_crossings": 0})
            group["episodes"] += 1
            if row["tick_delta_ms"]:
                group["stops"].append(float(row["tick_delta_ms"]))
            group["projected_crossings"] += bool(row["projected_zero_tick"])
    return [
        dict(weapon=weapon, kind=kind, start_speed_min=low, start_speed_max_exclusive=low + 50,
             episodes=group["episodes"], projected_crossings=group["projected_crossings"],
             threshold_hits=len(group["stops"]),
             median_stop_ms=median(group["stops"]) if group["stops"] else None)
        for (weapon, kind, low), group in sorted(cohorts.items())
    ]


def plot(path, title, unit, values):
    maximum = max((abs(median(series)) for series in values.values() if series),
                  default=1) or 1
    lines = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="940" height="220" '
        'viewBox="0 0 940 220" role="img">',
        '<rect width="940" height="220" fill="white"/>',
        f'<text x="20" y="30" font-size="19">{html.escape(title)} (median)</text>',
        '<path d="M400 53V195" stroke="#334155"/>',
    ]
    for index, (kind, series) in enumerate(values.items()):
        y = 78 + index * 49
        label = "no observations"
        if series:
            value = median(series)
            width = 224 * abs(value) / maximum
            x = 400 if value >= 0 else 400 - width
            lines.append(f'<rect x="{x:.2f}" y="{y-14}" width="{width:.2f}" '
                         'height="19" fill="#2563eb"/>')
            label = f"{value:.2f} {unit}, n={len(series):,}"
        lines.append(f'<text x="20" y="{y}">{kind}</text>')
        lines.append(f'<text x="640" y="{y}">{html.escape(label)}</text>')
    path.write_text("\n".join([*lines, "</svg>"]), encoding="utf-8")


def snapshot_velocities(frame, hz):
    players = frame.groupby("steamid", sort=False)
    contiguous = (players["tick"].diff().eq(1)
                  & players["game_time"].diff().mul(hz).sub(1).abs().lt(0.01))
    clean = (frame["is_airborne"].eq(False) & frame["is_alive"].eq(True)
             & frame["is_warmup_period"].eq(False) & frame["is_freeze_period"].eq(False)
             & frame["is_walking"].eq(False) & frame["duck_amount"].le(0.01)
             & frame["velo_modifier"].ge(0.99)
             & frame["active_weapon_name"].eq(players["active_weapon_name"].shift()))
    vx, vy = (players[axis].diff().mul(hz) for axis in ("X", "Y"))
    usable = (contiguous & clean & clean.groupby(frame["steamid"]).shift(fill_value=False)
              & vx.pow(2).add(vy.pow(2)).le(350 ** 2))
    frame["velocity_X"] = vx.where(usable)
    frame["velocity_Y"] = vy.where(usable)


def write_measurements(out, frame, hz, stop_speed):
    out.mkdir(parents=True, exist_ok=False)
    counts = Counter()
    with ((out / "intervals.csv").open("w", newline="", encoding="utf-8") as intervals,
          (out / "episodes.csv").open("w", newline="", encoding="utf-8") as episodes,
          (out / "crossings.csv").open("w", newline="", encoding="utf-8") as crossings):
        samples, stops, zeros = csv.writer(intervals), csv.DictWriter(episodes, EPISODE_HEADER), csv.writer(crossings)
        samples.writerow(SAMPLE_HEADER)
        stops.writeheader()
        zeros.writerow(CROSSING_HEADER)
        columns = list(frame.columns)
        rows = (dict(zip(columns, row)) for row in frame.itertuples(index=False, name=None))
        for _, player in groupby(rows, key=lambda row: row["steamid"]):
            counts.update(measure(player, hz, stop_speed, samples, stops, zeros))
    return counts


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("demo", type=Path)
    cli.add_argument("--out", type=Path, required=True, help="New output directory")
    cli.add_argument("--tick-rate", type=float, required=True,
                     help="Verified demo tick rate in Hz")
    cli.add_argument("--stop-speed", type=float, default=5,
                     help="Horizontal stop threshold in demo units/s (default 5)")
    args = cli.parse_args()
    if any(not finite(value) or value <= 0 for value in (args.tick_rate, args.stop_speed)):
        cli.error("tick rate and stop speed must be finite and positive")
    with args.demo.open("rb") as handle:
        digest = hashlib.file_digest(handle, "sha256").hexdigest()
    header, frame = parse(args.demo)
    capabilities = audit(frame)
    capabilities["parser_velocity_xy_rows"] = int(frame[["velocity_X", "velocity_Y"]].notna().all(axis=1).sum())
    snapshot_velocities(frame, args.tick_rate)
    if not frame[["velocity_X", "velocity_Y"]].notna().all(axis=1).any():
        cli.error("no adjacent grounded position samples at the specified tick rate")
    counts = write_measurements(args.out, frame, args.tick_rate, args.stop_speed)
    result = dict(schema_version=1, demo=args.demo.name, sha256=digest, header=dict(sorted(header.items())),
                  parser=version("demoparser2"), tick_rate=args.tick_rate,
                  stop_speed=args.stop_speed, player_ticks=len(frame),
                  capabilities=capabilities, counts=dict(counts), summaries=summarize(args.out),
                  stop_cohorts=stop_cohorts(args.out),
                  timing="Adjacent demo snapshots. No calibrated subtick-to-snapshot alignment.")
    (args.out / "summary.json").write_text(json.dumps(result, indent=2, default=str) + "\n",
                                          encoding="utf-8")


if __name__ == "__main__":
    main()




import csv
import tempfile
import unittest
from pathlib import Path

from compare_playtest import RATE, TRACES, compare_cohorts, drill_results, load_trace


class PlaytestTraceTests(unittest.TestCase):
    def test_trace_scale_and_grid(self):
        with tempfile.TemporaryDirectory() as tmp:
            trace = Path(tmp) / "trace.csv"
            with trace.open("w", newline="", encoding="utf-8") as output:
                writer = csv.writer(output)
                writer.writerow(("drill", "start_pct", "tick", "x", "y", "vx", "vy",
                                 "buttons", "top_speed"))
                for drill, percent in sorted(TRACES):
                    speed = 5 * percent / 100
                    for tick in range(-1, RATE + 2):
                        writer.writerow((drill, percent, tick, tick * speed / RATE, 0,
                                         speed, 0, 8, 5))
            frame, scale = load_trace(trace, 200, "AK-47")
            self.assertEqual(scale, 40)
            release = frame[frame["steamid"] == "release_50"]
            self.assertEqual(release.loc[release["tick"] == 1, "velocity_X"].item(), 100)
            self.assertEqual(release["active_weapon_name"].unique().tolist(), ["AK-47"])

    def test_projected_zero_can_follow_a_sampled_stop(self):
        import pandas as pd

        frame = pd.DataFrame({
            "steamid": ["counter_strafe_50"] * 3, "drill": ["counter_strafe"] * 3,
            "start_pct": [50] * 3, "tick": [1, 2, 3], "velocity_X": [30, 4, -4],
        })
        with tempfile.TemporaryDirectory() as tmp:
            episodes = Path(tmp) / "episodes.csv"
            episodes.write_text("player,tick_delta_ms\ncounter_strafe_50,0\n", encoding="utf-8")
            results = drill_results(frame, episodes, 215)
        self.assertEqual(results[0]["sampled_stop_ms"], 0)
        self.assertEqual(results[0]["projected_zero_ms"], 7.8125)

    def test_comparison_matches_weapon_kind_and_bin(self):
        playtest = [{"kind": "counter", "start_speed_min": 100}]
        demo = {"stop_cohorts": [
            {"weapon": "Glock-18", "kind": "counter", "start_speed_min": 100},
            {"weapon": "AK-47", "kind": "counter", "start_speed_min": 100},
            {"weapon": "AK-47", "kind": "passive", "start_speed_min": 100},
        ]}
        self.assertEqual(compare_cohorts(playtest, demo, "AK-47"),
                         [{"playtest": playtest[0], "demo": demo["stop_cohorts"][1]}])


if __name__ == "__main__":
    unittest.main()

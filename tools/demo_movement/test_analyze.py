import csv
import io
import unittest

from analyze import (BACK, FORWARD, LEFT, RIGHT, EPISODE_HEADER, audit, measure, snapshot_velocities, wish)


def run(ticks):
    sample, stop, cross = (io.StringIO() for _ in range(3))
    stops = csv.DictWriter(stop, EPISODE_HEADER)
    stops.writeheader()
    rows = [
        dict(tick=tick, steamid=42, velocity_X=speed, velocity_Y=0, yaw=0,
             is_alive=True, is_airborne=False, is_warmup_period=False,
             is_freeze_period=False, active_weapon_name="AK-47", buttons=bits)
        for tick, speed, bits in ticks
    ]
    counts = measure(rows, 64, 5, csv.writer(sample), stops, csv.writer(cross))
    stop.seek(0)
    cross.seek(0)
    return counts, list(csv.DictReader(stop)), list(csv.reader(cross))


class MovementTests(unittest.TestCase):
    def test_subtick_history_does_not_count_as_button_input(self):
        import pandas as pd

        frame = pd.DataFrame({
            "tick": [10, 11],
            "usercmd_input_history": [[{"player_tick_fraction": 0.1}], []],
            "usercmd_subtick_moves": [[{"button": LEFT, "pressed": True, "when": 0.5}],
                                      [{"button": RIGHT, "pressed": True, "when": 1.1}]],
        })
        result = audit(frame)
        self.assertEqual(result["usercmd_input_history"]["nonempty_rows"], 1)
        self.assertEqual(result["usercmd_subtick_moves"]["directional_button_entries"], 1)
        self.assertEqual(result["usercmd_subtick_moves"]["examples"][0]["tick"], 10)
        self.assertFalse(result["buttons"]["available"])

    def test_velocity_does_not_bridge_missing_demo_or_net_ticks(self):
        import pandas as pd

        frame = pd.DataFrame({
            "steamid": [42] * 5, "tick": [1, 2, 3, 5, 6],
            "game_time": [1/64, 2/64, 3/64, 5/64, 8/64],
            "X": [0, 1, 2, 3, 4], "Y": [0] * 5,
            "active_weapon_name": ["AK-47"] * 5, "is_alive": [True] * 5,
            "is_airborne": [False] * 5, "is_warmup_period": [False] * 5,
            "is_freeze_period": [False] * 5, "is_walking": [False] * 5,
            "duck_amount": [0] * 5, "velo_modifier": [1] * 5,
        })
        snapshot_velocities(frame, 64)
        self.assertEqual(frame["velocity_X"].dropna().to_dict(), {2: 64.0})

    def test_axes(self):
        self.assertEqual(wish(LEFT, 0), (0, 1))
        self.assertEqual(wish(RIGHT, 0), (0, -1))
        self.assertIsNone(wish(FORWARD | BACK, 0))

    def test_passive_stop(self):
        counts, episodes, _ = run([(0, 0, FORWARD), (1, 80, FORWARD),
                                   (2, 40, 0), (3, 4, 0)])
        self.assertEqual(counts["acceleration"], 1)
        self.assertEqual(counts["passive"], 2)
        self.assertEqual(episodes[0]["tick_delta_ms"], "15.625")

    def test_counter_crossing_without_full_stop(self):
        counts, episodes, crossings = run([(0, 80, FORWARD), (1, 30, BACK),
                                          (2, -10, BACK), (3, -40, BACK)])
        self.assertEqual(counts["counter"], 2)
        self.assertEqual(episodes[0]["projected_zero_tick"], "1.75")
        self.assertEqual(episodes[0]["stop_tick"], "")
        self.assertEqual(crossings[0][-1], "1.75")

    def test_missing_buttons_are_not_neutral(self):
        counts, episodes, _ = run([(0, 80, FORWARD), (1, 4, None)])
        self.assertFalse(counts)
        self.assertFalse(episodes)

    def test_gap_censors_episode(self):
        _, episodes, _ = run([(0, 80, FORWARD), (1, 40, 0), (3, 0, 0)])
        self.assertEqual(episodes[0]["end_reason"], "gap_or_weapon_change")
        self.assertEqual(episodes[0]["tick_delta_ms"], "")


if __name__ == "__main__":
    unittest.main()


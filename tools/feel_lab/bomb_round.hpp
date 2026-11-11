#pragma once

#include <algorithm>

namespace feel_lab {

enum class GameMode { Practice, Plant, Defuse, OfflineMatch };
enum class MatchLength { Short, Classic };

struct BombRound {
    enum class Phase { Live, Planted, Detonated, Defused, Expired };
    static constexpr double round_seconds = 115;
    static constexpr double plant_seconds = 3;
    static constexpr double fuse_seconds = 40;
    static constexpr double defuse_seconds = 10;

    GameMode mode = GameMode::Practice;
    Phase phase = Phase::Live;
    double remaining = round_seconds;
    double progress = 0;

    void start(GameMode selected) {
        mode = selected;
        phase = mode == GameMode::Defuse ? Phase::Planted : Phase::Live;
        remaining = phase == Phase::Planted ? fuse_seconds : round_seconds;
        progress = 0;
    }

    void step(double seconds, bool interacting) {
        if (mode == GameMode::Practice || finished()) return;
        const bool can_plant = mode == GameMode::Plant && phase == Phase::Live;
        const bool can_defuse = mode == GameMode::Defuse && phase == Phase::Planted;
        const double duration = can_plant ? plant_seconds : defuse_seconds;
        progress = interacting && (can_plant || can_defuse) ? progress + seconds : 0;
        const double time_before_step = remaining;
        remaining = std::max(0.0, remaining - seconds);
        if (progress >= duration && progress - duration >= seconds - time_before_step) {
            progress = 0;
            if (can_plant) {
                phase = Phase::Planted;
                remaining = fuse_seconds;
            } else {
                phase = Phase::Defused;
            }
        } else if (remaining == 0) {
            phase = phase == Phase::Planted ? Phase::Detonated : Phase::Expired;
        }
    }

    bool finished() const {
        return phase == Phase::Detonated || phase == Phase::Defused || phase == Phase::Expired;
    }

    bool won() const {
        return (mode == GameMode::Plant && phase == Phase::Detonated) || phase == Phase::Defused;
    }
};

} // namespace feel_lab

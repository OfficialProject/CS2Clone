#pragma once

#include "game/simulation/movement.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string_view>

namespace feel_lab {

inline bool record_drills(const std::filesystem::path& path,
                          const game::simulation::MovementSimulator& simulation) {
    using namespace game::simulation;
    constexpr int observation_rate = 64;
    constexpr unsigned forward_button = 1u << 3;
    constexpr unsigned back_button = 1u << 4;
    constexpr std::array start_percentages{50, 70, 90, 100};
    const auto& config = simulation.config();
    const double steps = 1.0 / (observation_rate * config.fixed_dt);
    if (steps < 1 || std::abs(steps - std::round(steps)) > 1e-9) return false;
    const auto steps_per_sample = static_cast<int>(std::round(steps));

    std::ofstream output(path);
    if (!output) return false;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << "drill,start_pct,tick,x,y,vx,vy,buttons,top_speed\n";

    const auto record = [&](std::string_view drill, int percent, double input) {
        MovementState state;
        state.velocity.x = config.ground_max_speed * percent / 100.0;
        const auto sample = [&](int tick, unsigned buttons) {
            output << drill << ',' << percent << ',' << tick << ','
                   << state.position.x << ',' << state.position.y << ','
                   << state.velocity.x << ',' << state.velocity.y << ','
                   << buttons << ',' << config.ground_max_speed << '\n';
        };

        // Seed one known pre-input velocity interval without an acceleration step.
        state.position.x = -2 * state.velocity.x / observation_rate;
        sample(-1, percent ? forward_button : 0u);
        state.position.x = -state.velocity.x / observation_rate;
        sample(0, percent ? forward_button : 0u);
        state.position.x = 0;
        sample(1, percent ? forward_button : 0u);
        const unsigned buttons = input > 0 ? forward_button : input < 0 ? back_button : 0u;
        for (int tick = 2; tick <= observation_rate + 1; ++tick) {
            for (int step = 0; step < steps_per_sample; ++step) {
                (void)simulation.step(state, {.right = input});
            }
            sample(tick, buttons);
        }
    };

    record("rest_to_run", 0, 1.0);
    for (int percent : start_percentages) {
        record("release", percent, 0.0);
        record("counter_strafe", percent, -1.0);
    }
    output.close();
    return static_cast<bool>(output);
}

} // namespace feel_lab

#include "game/simulation/movement.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

using game::simulation::MovementConfig;
using game::simulation::MovementInput;
using game::simulation::MovementMode;
using game::simulation::MovementSimulator;
using game::simulation::MovementState;

enum class Scenario { Release, CounterStrafe, CrouchTransition };

namespace {
void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable << " [--rate 128|256] "
              << "[--scenario release|counter-strafe|crouch]\n";
}

bool parse_rate(const std::string_view value, int& rate) {
    const auto result = std::from_chars(value.data(), value.data() + value.size(), rate);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
           (rate == 128 || rate == 256);
}

bool parse_scenario(const std::string_view value, Scenario& scenario) {
    if (value == "release") scenario = Scenario::Release;
    else if (value == "counter-strafe") scenario = Scenario::CounterStrafe;
    else if (value == "crouch") scenario = Scenario::CrouchTransition;
    else return false;
    return true;
}

MovementInput input_for(const Scenario scenario, const int tick, const int rate) {
    if (tick < rate) return {.forward = 1.0};
    if (scenario == Scenario::Release) return {};
    if (scenario == Scenario::CounterStrafe) return {.forward = -1.0};
    if (tick < rate * 2) return {.forward = 1.0, .crouch_held = true};
    return {.crouch_held = true};
}
} // namespace

int main(int argc, char** argv) {
    int rate = 128;
    Scenario scenario = Scenario::Release;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument == "--rate" && i + 1 < argc) {
            if (!parse_rate(argv[++i], rate)) {
                print_usage(argv[0]);
                return 2;
            }
        } else if (argument == "--scenario" && i + 1 < argc) {
            if (!parse_scenario(argv[++i], scenario)) {
                print_usage(argv[0]);
                return 2;
            }
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    MovementConfig config;
    config.fixed_dt = 1.0 / static_cast<double>(rate);
    const MovementSimulator simulator{config};
    MovementState state;
    const int total_ticks = scenario == Scenario::CrouchTransition ? rate * 3 : rate * 2;

    std::cout << "tick,time,mode,x,y,vx,vy,speed,crouch,accuracy\n";
    for (int tick = 0; tick < total_ticks; ++tick) {
        const auto output = simulator.step(state, input_for(scenario, tick, rate));
        const auto mode = state.mode == MovementMode::Grounded ? "ground" : "air";
        std::cout << state.tick << ',' << static_cast<double>(state.tick) * config.fixed_dt << ','
                  << mode << ',' << state.position.x << ',' << state.position.y << ','
                  << state.velocity.x << ',' << state.velocity.y << ','
                  << output.horizontal_speed << ',' << state.crouch_fraction << ','
                  << output.movement_accuracy << '\n';
    }
    return 0;
}

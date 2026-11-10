#include "game/simulation/movement.hpp"

#include <iomanip>
#include <iostream>

using game::simulation::MovementInput;
using game::simulation::MovementSimulator;
using game::simulation::MovementState;

int main() {
    const MovementSimulator simulator;
    MovementState state;
    constexpr int ticks = 128;
    std::cout << "Headless movement laboratory | 128 Hz | 1 second forward, then release\n";
    std::cout << "tick,speed,x,y,accuracy\n";
    for (int i = 0; i < ticks * 2; ++i) {
        const MovementInput input{.forward = i < ticks ? 1.0 : 0.0};
        const auto output = simulator.step(state, input);
        if (i % 16 == 15 || i == ticks * 2 - 1) {
            std::cout << state.tick << ',' << std::fixed << std::setprecision(4)
                      << output.horizontal_speed << ',' << state.position.x << ','
                      << state.position.y << ',' << output.movement_accuracy << '\n';
        }
    }
    std::cout << "\nRun ctest --test-dir build --output-on-failure for regression checks.\n";
    return 0;
}

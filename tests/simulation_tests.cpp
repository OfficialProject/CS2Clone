#include "game/simulation/movement.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace game::simulation;

namespace {
int failures = 0;

void check(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

MovementState run(const MovementSimulator& simulator, MovementState state,
                  const MovementInput& input, const int ticks) {
    for (int i = 0; i < ticks; ++i) {
        (void)simulator.step(state, input);
    }
    return state;
}

void test_acceleration_and_speed_cap() {
    const MovementSimulator simulator;
    MovementState state;
    const auto moving = run(simulator, state, {.forward = 1.0}, 128);
    check(moving.velocity.y > 0.0, "forward input accelerates forward");
    check(moving.velocity.y <= simulator.config().ground_max_speed + 1.0e-9,
          "ground speed does not exceed configured cap");
}

void test_natural_friction_is_gradual() {
    const MovementSimulator simulator;
    MovementState moving;
    moving.velocity.y = 5.0;
    const auto first = simulator.step(moving, {});
    check(first.horizontal_speed > 0.0, "release does not instantly stop movement");
    check(first.horizontal_speed < 5.0, "natural friction reduces speed");
    const auto after = run(simulator, moving, {}, 30);
    check(std::abs(after.velocity.y) < first.horizontal_speed,
          "natural friction continues to decelerate");
}

void test_counter_strafe_beats_release() {
    const MovementSimulator simulator;
    MovementState passive;
    passive.velocity.y = 5.0;
    MovementState active = passive;
    const auto passive_result = run(simulator, passive, {}, 12);
    const auto active_result = run(simulator, active, {.forward = -1.0}, 12);
    check(std::abs(active_result.velocity.y) < std::abs(passive_result.velocity.y),
          "opposite input brakes faster than releasing input");
}

void test_reversal_crosses_zero_consistently() {
    const MovementSimulator simulator;
    MovementState state;
    state.velocity.y = 3.0;
    bool crossed = false;
    for (int i = 0; i < 100; ++i) {
        (void)simulator.step(state, {.forward = -1.0});
        if (state.velocity.y < 0.0) {
            crossed = true;
            break;
        }
    }
    check(crossed, "sustained reverse input transitions through zero velocity");
}

void test_ground_speed_cap_after_direction_change() {
    MovementState state;
    state.velocity = {5.0, 0.0};
    const MovementSimulator simulator;
    const auto result = run(simulator, state, {.forward = 1.0}, 80);
    const double speed = std::sqrt(result.velocity.x * result.velocity.x +
                                   result.velocity.y * result.velocity.y);
    check(speed <= simulator.config().ground_max_speed + 1.0e-8,
          "ground speed remains capped during direction changes");
}

void test_crouch_does_not_erase_existing_momentum() {
    const MovementSimulator simulator;
    MovementState state;
    state.velocity.y = simulator.config().ground_max_speed;
    (void)simulator.step(state, {.forward = 1.0, .crouch_held = true});
    check(state.velocity.y < simulator.config().ground_max_speed &&
          state.velocity.y > simulator.config().ground_max_speed - 0.5,
          "crouching brakes existing momentum gradually rather than clipping it");
}

void test_crouched_acceleration_uses_lower_target_speed() {
    const MovementSimulator simulator;
    MovementState state;
    state = run(simulator, state, {.crouch_held = true}, 128);
    state = run(simulator, state, {.forward = 1.0, .crouch_held = true}, 128);
    check(state.velocity.y <= simulator.config().ground_max_speed *
          simulator.config().crouch_speed_multiplier + 1.0e-8,
          "crouched acceleration respects its lower target speed");
}

void test_jump_is_reported_once() {
    const MovementSimulator simulator;
    MovementState state;
    const auto first = simulator.step(state, {.jump_pressed = true});
    const auto second = simulator.step(state, {.jump_pressed = false});
    check(first.jumped, "grounded jump input reports a jump");
    check(!second.jumped, "jump event is not repeated on following tick");
}

void test_air_speed_cap() {
    MovementState state;
    state.mode = MovementMode::Airborne;
    state.vertical_position = 1.0;
    state.velocity = {6.0, 0.0};
    const MovementSimulator simulator;
    const auto result = run(simulator, state, {.forward = 1.0, .right = 1.0}, 20);
    const double speed = std::sqrt(result.velocity.x * result.velocity.x +
                                   result.velocity.y * result.velocity.y);
    check(speed <= simulator.config().air_max_speed + 1.0e-8,
          "airborne horizontal speed remains capped");
}

void test_crouch_transition_is_bounded() {
    const MovementSimulator simulator;
    MovementState state;
    (void)simulator.step(state, {.crouch_held = true});
    check(state.crouch_fraction > 0.0 && state.crouch_fraction < 1.0,
          "crouch transition is gradual");
    state = run(simulator, state, {.crouch_held = true}, 128);
    check(std::abs(state.crouch_fraction - 1.0) < 1.0e-9,
          "crouch transition reaches full stance");
}

MovementState run_release_scenario(const int rate) {
    MovementConfig config;
    config.fixed_dt = 1.0 / static_cast<double>(rate);
    const MovementSimulator simulator{config};
    MovementState state;
    const int ticks_per_second = rate;
    for (int i = 0; i < ticks_per_second; ++i) {
        (void)simulator.step(state, {.forward = 1.0});
    }
    for (int i = 0; i < ticks_per_second; ++i) {
        (void)simulator.step(state, {});
    }
    return state;
}

void test_128_and_256_hz_are_comparable() {
    const auto at_128 = run_release_scenario(128);
    const auto at_256 = run_release_scenario(256);
    check(std::abs(at_128.position.y - at_256.position.y) < 0.03,
          "128 Hz and 256 Hz produce close displacement for equal real-time inputs");
    check(std::abs(at_128.velocity.y - at_256.velocity.y) < 0.03,
          "128 Hz and 256 Hz produce close final speed for equal real-time inputs");
}

void test_replay_is_repeatable_with_same_build() {
    const MovementSimulator simulator;
    MovementState first;
    MovementState second;
    for (int i = 0; i < 400; ++i) {
        const MovementInput input{
            .forward = i < 100 ? 1.0 : (i < 220 ? -1.0 : 0.0),
            .right = i >= 250 && i < 330 ? 0.5 : 0.0,
            .jump_pressed = i == 140,
            .crouch_held = i >= 300 && i < 360,
        };
        (void)simulator.step(first, input);
        (void)simulator.step(second, input);
    }
    check(first.tick == second.tick && first.position.x == second.position.x &&
          first.position.y == second.position.y && first.velocity.x == second.velocity.x &&
          first.velocity.y == second.velocity.y && first.vertical_position == second.vertical_position &&
          first.crouch_fraction == second.crouch_fraction,
          "identical input stream produces identical state in the same build");
}

void test_invalid_state_rejected() {
    bool threw = false;
    try {
        MovementState state;
        state.crouch_fraction = 1.5;
        (void)MovementSimulator{}.step(state, {});
    } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "invalid simulation state is rejected");
}

void test_invalid_input_rejected() {
    bool threw = false;
    try {
        MovementState state;
        (void)MovementSimulator{}.step(state, {.forward = std::numeric_limits<double>::quiet_NaN()});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "non-finite movement input is rejected");
}

void test_invalid_configuration_rejected() {
    bool threw = false;
    try {
        MovementConfig config;
        config.fixed_dt = 0.0;
        (void)MovementSimulator{config};
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "invalid movement configuration is rejected");
}
} // namespace

int main() {
    test_acceleration_and_speed_cap();
    test_natural_friction_is_gradual();
    test_counter_strafe_beats_release();
    test_reversal_crosses_zero_consistently();
    test_ground_speed_cap_after_direction_change();
    test_crouch_does_not_erase_existing_momentum();
    test_crouched_acceleration_uses_lower_target_speed();
    test_jump_is_reported_once();
    test_air_speed_cap();
    test_crouch_transition_is_bounded();
    test_128_and_256_hz_are_comparable();
    test_replay_is_repeatable_with_same_build();
    test_invalid_configuration_rejected();
    test_invalid_state_rejected();
    test_invalid_input_rejected();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All simulation tests passed\n";
    return EXIT_SUCCESS;
}

#include "game/simulation/movement.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::simulation {
namespace {
constexpr double kEpsilon = 1.0e-9;

[[nodiscard]] double clamp_axis(const double value) noexcept {
    return std::clamp(value, -1.0, 1.0);
}

[[nodiscard]] double length(const Vec2 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

void clamp_speed(Vec2& velocity, const double maximum) noexcept {
    const double speed = length(velocity);
    if (speed > maximum) {
        const double scale = maximum / speed;
        velocity.x *= scale;
        velocity.y *= scale;
    }
}

[[nodiscard]] Vec2 normalized_input(const double forward, const double right) noexcept {
    Vec2 result{clamp_axis(right), clamp_axis(forward)};
    const double magnitude = length(result);
    if (magnitude > 1.0) {
        result.x /= magnitude;
        result.y /= magnitude;
    }
    return result;
}

void accelerate_toward(Vec2& velocity, const Vec2 wish_direction,
                       const double wish_speed, const double acceleration,
                       const double dt) noexcept {
    if (wish_speed <= 0.0) return;
    const double current_speed = velocity.x * wish_direction.x + velocity.y * wish_direction.y;
    const double add_speed = wish_speed - current_speed;
    if (add_speed <= 0.0) return;
    const double acceleration_speed = std::min(add_speed, acceleration * dt);
    velocity.x += acceleration_speed * wish_direction.x;
    velocity.y += acceleration_speed * wish_direction.y;
}

void apply_friction(Vec2& velocity, const double friction, const double dt) noexcept {
    const double speed = length(velocity);
    if (speed <= kEpsilon) {
        velocity = {};
        return;
    }
    const double new_speed = std::max(0.0, speed - speed * friction * dt);
    const double scale = new_speed / speed;
    velocity.x *= scale;
    velocity.y *= scale;
}
} // namespace

MovementSimulator::MovementSimulator(MovementConfig config) : config_(config) {
    if (!std::isfinite(config_.fixed_dt) || !std::isfinite(config_.ground_max_speed) ||
        !std::isfinite(config_.crouch_speed_multiplier) ||
        !std::isfinite(config_.ground_acceleration) || !std::isfinite(config_.ground_friction) ||
        !std::isfinite(config_.air_acceleration) || !std::isfinite(config_.air_max_speed) ||
        !std::isfinite(config_.gravity) || !std::isfinite(config_.jump_speed) ||
        !std::isfinite(config_.crouch_transition_seconds) ||
        !std::isfinite(config_.full_accuracy_speed) ||
        !std::isfinite(config_.inaccurate_speed) ||
        !(config_.crouch_speed_multiplier > 0.0 && config_.crouch_speed_multiplier <= 1.0) ||
        !(config_.fixed_dt > 0.0) || !(config_.ground_max_speed > 0.0) ||
        !(config_.ground_acceleration > 0.0) || !(config_.ground_friction >= 0.0) ||
        !(config_.air_acceleration > 0.0) || !(config_.air_max_speed > 0.0) ||
        !(config_.gravity > 0.0) || !(config_.jump_speed > 0.0) ||
        !(config_.crouch_transition_seconds > 0.0) ||
        !(config_.full_accuracy_speed >= 0.0) ||
        !(config_.inaccurate_speed > config_.full_accuracy_speed)) {
        throw std::invalid_argument("Invalid movement configuration");
    }
}

MovementOutput MovementSimulator::step(MovementState& state, const MovementInput& input) const {
    if (!std::isfinite(input.forward) || !std::isfinite(input.right)) {
        throw std::invalid_argument("Movement input axes must be finite");
    }
    const double dt = config_.fixed_dt;
    const double crouch_target = input.crouch_held ? 1.0 : 0.0;
    const double stance_step = dt / config_.crouch_transition_seconds;
    if (state.crouch_fraction < crouch_target) {
        state.crouch_fraction = std::min(crouch_target, state.crouch_fraction + stance_step);
    } else if (state.crouch_fraction > crouch_target) {
        state.crouch_fraction = std::max(crouch_target, state.crouch_fraction - stance_step);
    }

    const Vec2 wish = normalized_input(input.forward, input.right);
    bool jumped = false;
    const double crouch_multiplier = 1.0 - state.crouch_fraction * (1.0 - config_.crouch_speed_multiplier);

    if (state.mode == MovementMode::Grounded) {
        if (std::abs(wish.x) <= kEpsilon && std::abs(wish.y) <= kEpsilon) {
            apply_friction(state.velocity, config_.ground_friction, dt);
        } else {
            accelerate_toward(state.velocity, wish,
                              config_.ground_max_speed * crouch_multiplier,
                              config_.ground_acceleration, dt);
        }
        clamp_speed(state.velocity, config_.ground_max_speed * crouch_multiplier);
        if (input.jump_pressed) {
            state.mode = MovementMode::Airborne;
            state.vertical_velocity = config_.jump_speed;
            jumped = true;
        }
    } else {
        accelerate_toward(state.velocity, wish,
                          config_.air_max_speed * crouch_multiplier,
                          config_.air_acceleration, dt);
        clamp_speed(state.velocity, config_.air_max_speed);
        state.vertical_velocity -= config_.gravity * dt;
        state.vertical_position += state.vertical_velocity * dt;
        if (state.vertical_position <= 0.0 && state.vertical_velocity < 0.0) {
            state.vertical_position = 0.0;
            state.vertical_velocity = 0.0;
            state.mode = MovementMode::Grounded;
        }
    }

    state.position.x += state.velocity.x * dt;
    state.position.y += state.velocity.y * dt;
    ++state.tick;

    const double speed = length(state.velocity);
    double accuracy = 1.0;
    if (speed >= config_.inaccurate_speed) {
        accuracy = 0.0;
    } else if (speed > config_.full_accuracy_speed) {
        const double range = config_.inaccurate_speed - config_.full_accuracy_speed;
        accuracy = 1.0 - (speed - config_.full_accuracy_speed) / range;
    }
    return MovementOutput{speed, std::clamp(accuracy, 0.0, 1.0), jumped};
}
} // namespace game::simulation

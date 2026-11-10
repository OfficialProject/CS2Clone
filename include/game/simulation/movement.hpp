#pragma once

#include <cstdint>

namespace game::simulation {

struct Vec2 {
    double x{};
    double y{};
};

enum class MovementMode : std::uint8_t { Grounded, Airborne };

struct MovementInput {
    double forward{};
    double right{};
    bool jump_pressed{};
    bool crouch_held{};
};

struct MovementConfig {
    double fixed_dt{1.0 / 128.0};
    double ground_max_speed{5.8};
    double crouch_speed_multiplier{0.72};
    double ground_acceleration{38.0};
    double ground_friction{8.0};
    double air_acceleration{9.0};
    double air_max_speed{6.2};
    double gravity{20.0};
    double jump_speed{6.5};
    double crouch_transition_seconds{0.12};
    double full_accuracy_speed{0.12};
    double inaccurate_speed{1.8};
};

struct MovementState {
    Vec2 position{};
    Vec2 velocity{};
    double vertical_position{};
    double vertical_velocity{};
    double crouch_fraction{};
    MovementMode mode{MovementMode::Grounded};
    std::uint64_t tick{};
};

struct MovementOutput {
    double horizontal_speed{};
    double movement_accuracy{1.0};
    bool jumped{};
};

class MovementSimulator {
public:
    explicit MovementSimulator(MovementConfig config = {});
    [[nodiscard]] const MovementConfig& config() const noexcept { return config_; }
    [[nodiscard]] MovementOutput step(MovementState& state, const MovementInput& input) const;

private:
    MovementConfig config_;
};

} // namespace game::simulation

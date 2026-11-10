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
    bool walk_held{};
};

struct MovementConfig {
    // World scale preserves the existing scene: 250 Source units = 5.8 scene units.
    static constexpr double source_unit = 5.8 / 250.0;
    bool flat_ground{true};
    double fixed_dt{1.0 / 128.0};
    double ground_max_speed{5.8};
    double crouch_speed_multiplier{0.34};
    double walk_speed_multiplier{0.52};
    double ground_acceleration{5.5};
    double ground_friction{5.2};
    double stop_speed{80.0 * source_unit};
    double surface_friction{1.0};
    double air_acceleration{12.0};
    double air_wish_speed_cap{30.0 * source_unit};
    double gravity{800.0 * source_unit};
    double jump_speed{301.993377 * source_unit};
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
    bool on_ladder{};
    Vec2 ladder_normal{};
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

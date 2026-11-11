#pragma once

#include "game/simulation/movement.hpp"
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace feel_lab {

struct AccuracyProfile {
    float standing;
    float moving;
    float airborne;
    float per_shot;
    float recovery_seconds;
    float recoil;
};

// Angular offsets in radians. Tune these against the practice range.
inline constexpr AccuracyProfile rifle_accuracy{0.004f,0.075f,0.10f,0.009f,0.36f,0.027f};
inline constexpr AccuracyProfile pistol_accuracy{0.008f,0.044f,0.072f,0.008f,0.27f,0.016f};

struct Gunplay {
    float penalty = 0;
    float recoil_index = 0;
    double since_shot = 0;

    void advance(const AccuracyProfile& profile, double interval, double dt, bool landed) {
        const float decay = std::pow(0.1f,static_cast<float>(dt)/profile.recovery_seconds);
        penalty *= decay;
        if (landed) penalty = std::max(penalty,profile.airborne*0.4f);
        since_shot += dt;
        // Allow one simulation tick of scheduling delay between automatic shots.
        if (since_shot > interval+dt) recoil_index *= decay;
    }

    float spread(const AccuracyProfile& profile, const game::simulation::MovementState& state) const {
        using namespace game::simulation;
        const float speed = static_cast<float>(std::hypot(state.velocity.x,state.velocity.y) /
                                               MovementConfig::source_unit);
        const float moving = std::clamp((speed-75.0f)/150.0f,0.0f,1.0f);
        const bool grounded = state.mode == MovementMode::Grounded;
        const float stance = grounded ? 1.0f-0.35f*static_cast<float>(state.crouch_fraction) : 1.0f;
        const float air = grounded ? 0.0f : profile.airborne;
        return profile.standing*stance + profile.moving*moving*moving + air + penalty;
    }

    Ray shoot(Ray aim, const AccuracyProfile& profile, const game::simulation::MovementState& state) {
        const float cone = spread(profile,state);
        const float climb = profile.recoil*std::min(recoil_index/7.0f,1.0f);
        const float drift = climb*0.55f*std::sin(recoil_index*0.55f);
        const auto right = Vector3Normalize(Vector3CrossProduct(aim.direction,{0,1,0}));
        const auto up = Vector3Normalize(Vector3CrossProduct(right,aim.direction));
        const float angle = static_cast<float>(GetRandomValue(0,4095))*(2.0f*PI/4096.0f);
        const float radius = cone*std::sqrt(static_cast<float>(GetRandomValue(0,4095))/4095.0f);
        aim.direction = Vector3Normalize(Vector3Add(aim.direction,Vector3Add(
            Vector3Scale(right,drift+radius*std::cos(angle)),
            Vector3Scale(up,climb+radius*std::sin(angle)))));
        penalty = std::min(penalty+profile.per_shot,profile.per_shot*5.0f);
        recoil_index = std::min(recoil_index+1.0f,20.0f);
        since_shot = 0;
        return aim;
    }
};

} // namespace feel_lab

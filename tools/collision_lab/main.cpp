#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <cstdarg>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

JPH_SUPPRESS_WARNINGS

namespace {

namespace Layers {
constexpr JPH::ObjectLayer NonMoving = 0;
constexpr JPH::ObjectLayer Moving = 1;
}

namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
}

void trace_impl(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    std::cerr << buffer << '\n';
}

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == Layers::NonMoving ? BroadPhaseLayers::NonMoving : BroadPhaseLayers::Moving;
    }
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object_layer,
                                     JPH::BroadPhaseLayer broad_phase_layer) const override {
        if (object_layer == Layers::NonMoving) return broad_phase_layer == BroadPhaseLayers::Moving;
        return true;
    }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first,
                                     JPH::ObjectLayer second) const override {
        if (first == Layers::NonMoving) return second == Layers::Moving;
        return true;
    }
};

void initialize_jolt() {
    static bool initialized = false;
    if (initialized) return;
    JPH::RegisterDefaultAllocator();
    JPH::Trace = trace_impl;
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    initialized = true;
}

class JoltWorld {
public:
    static constexpr float FixedDt = 1.0f / 128.0f;

    JoltWorld() {
        initialize_jolt();
        allocator_ = std::make_unique<JPH::TempAllocatorImpl>(4U * 1024U * 1024U);
        physics_.Init(4096, 0, 4096, 4096,
                      broad_phase_layers_, object_vs_broad_phase_filter_, object_layer_pair_filter_);
        physics_.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    }

    ~JoltWorld() {
        JPH::BodyInterface& bodies = physics_.GetBodyInterface();
        for (const JPH::BodyID id : body_ids_) {
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
        }
    }

    JPH::BodyID add_box(const JPH::RVec3& center, const JPH::Vec3& half_extents,
                        const JPH::Quat& rotation = JPH::Quat::sIdentity()) {
        JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(half_extents);
        JPH::BodyCreationSettings settings(shape, center, rotation,
                                           JPH::EMotionType::Static, Layers::NonMoving);
        const JPH::BodyID id = physics_.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);
        if (id.IsInvalid()) throw std::runtime_error("Jolt failed to create a static test body");
        body_ids_.push_back(id);
        return id;
    }

    [[nodiscard]] JPH::PhysicsSystem& physics() noexcept { return physics_; }
    [[nodiscard]] JPH::TempAllocatorImpl& allocator() noexcept { return *allocator_; }
    [[nodiscard]] JPH::CharacterVsCharacterCollisionSimple& character_collision() noexcept {
        return character_collision_;
    }

private:
    BroadPhaseLayerInterfaceImpl broad_phase_layers_;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter_;
    ObjectLayerPairFilterImpl object_layer_pair_filter_;
    std::unique_ptr<JPH::TempAllocatorImpl> allocator_;
    JPH::PhysicsSystem physics_;
    JPH::CharacterVsCharacterCollisionSimple character_collision_;
    std::vector<JPH::BodyID> body_ids_;
};

JPH::RefConst<JPH::Shape> make_capsule(const float cylinder_height, const float radius) {
    return JPH::RotatedTranslatedShapeSettings(
        JPH::Vec3(0.0f, 0.5f * cylinder_height + radius, 0.0f),
        JPH::Quat::sIdentity(),
        new JPH::CapsuleShape(0.5f * cylinder_height, radius)).Create().Get();
}

class VirtualCharacter {
public:
    static constexpr float CharacterPadding = 0.02f;

    explicit VirtualCharacter(JoltWorld& world, const JPH::RVec3& position = JPH::RVec3::sZero(),
                              const bool start_crouched = false, const std::uint32_t stable_id = 0,
                              const bool use_jolt_character_collision = true)
        : world_(world),
          stable_id_(stable_id),
          use_jolt_character_collision_(use_jolt_character_collision),
          standing_shape_(make_capsule(1.0f, 0.3f)),
          crouched_shape_(make_capsule(0.4f, 0.3f)) {
        JPH::CharacterVirtualSettings settings;
        settings.mShape = start_crouched ? crouched_shape_ : standing_shape_;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mCharacterPadding = CharacterPadding;
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.3f);
        character_ = new JPH::CharacterVirtual(&settings, position, JPH::Quat::sIdentity(),
                                                &world_.physics());
        if (use_jolt_character_collision_) {
            character_->SetCharacterVsCharacterCollision(&world_.character_collision());
            world_.character_collision().Add(character_.GetPtr());
        }
        update_settings_.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
        update_settings_.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);
    }

    ~VirtualCharacter() {
        if (use_jolt_character_collision_) {
            world_.character_collision().Remove(character_.GetPtr());
        }
    }

    void step(const JPH::Vec3& desired_horizontal_velocity, const bool jump = false) {
        JPH::PhysicsSystem& physics = world_.physics();
        character_->UpdateGroundVelocity();

        const JPH::Vec3 up = character_->GetUp();
        const JPH::Vec3 current_vertical_velocity = up * character_->GetLinearVelocity().Dot(up);
        const JPH::Vec3 ground_velocity = character_->GetGroundVelocity();
        JPH::Vec3 new_velocity;
        if (character_->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround &&
            (current_vertical_velocity.Dot(up) - ground_velocity.Dot(up)) < 0.1f) {
            new_velocity = ground_velocity;
            if (jump) new_velocity += up * 6.5f;
        } else {
            new_velocity = current_vertical_velocity;
        }

        new_velocity += physics.GetGravity() * JoltWorld::FixedDt;
        new_velocity += desired_horizontal_velocity;
        character_->SetLinearVelocity(new_velocity);
        character_->ExtendedUpdate(
            JoltWorld::FixedDt, physics.GetGravity(), update_settings_,
            physics.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
            physics.GetDefaultLayerFilter(Layers::Moving), {}, {}, world_.allocator());
    }

    [[nodiscard]] JPH::RVec3 position() const { return character_->GetPosition(); }
    [[nodiscard]] JPH::Vec3 velocity() const { return character_->GetLinearVelocity(); }
    [[nodiscard]] JPH::CharacterBase::EGroundState ground_state() const {
        return character_->GetGroundState();
    }
    [[nodiscard]] const JPH::Shape* shape() const { return character_->GetShape(); }
    [[nodiscard]] JPH::AABox world_bounds() const {
        return character_->GetShape()->GetWorldSpaceBounds(
            character_->GetCenterOfMassTransform(), JPH::Vec3::sReplicate(1.0f));
    }
    [[nodiscard]] std::uint32_t stable_id() const noexcept { return stable_id_; }
    [[nodiscard]] float character_padding() const { return character_->GetCharacterPadding(); }
    void set_position(const JPH::RVec3& position) { character_->SetPosition(position); }
    void project_world() {
        const JPH::Vec3 saved_velocity = character_->GetLinearVelocity();
        character_->SetLinearVelocity(JPH::Vec3::sZero());
        character_->Update(JoltWorld::FixedDt, JPH::Vec3::sZero(),
                           world_.physics().GetDefaultBroadPhaseLayerFilter(Layers::Moving),
                           world_.physics().GetDefaultLayerFilter(Layers::Moving), {}, {}, world_.allocator());
        character_->SetLinearVelocity(saved_velocity);
    }

    [[nodiscard]] bool set_crouched(const bool crouched) {
        const JPH::Shape* target = crouched ? crouched_shape_.GetPtr() : standing_shape_.GetPtr();
        JPH::PhysicsSystem& physics = world_.physics();
        return character_->SetShape(
            target, 0.0f,
            physics.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
            physics.GetDefaultLayerFilter(Layers::Moving), {}, {}, world_.allocator());
    }

    void save_state(JPH::StateRecorderImpl& recorder) const { character_->SaveState(recorder); }
    void restore_state(JPH::StateRecorderImpl& recorder) { character_->RestoreState(recorder); }

    [[nodiscard]] const JPH::Shape* crouched_shape() const { return crouched_shape_.GetPtr(); }

private:
    JoltWorld& world_;
    std::uint32_t stable_id_;
    bool use_jolt_character_collision_;
    JPH::RefConst<JPH::Shape> standing_shape_;
    JPH::RefConst<JPH::Shape> crouched_shape_;
    JPH::Ref<JPH::CharacterVirtual> character_;
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings_;
};

int failures = 0;

void check(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] float distance(const JPH::RVec3& first, const JPH::RVec3& second) {
    return (first - second).Length();
}

void test_fall_to_floor() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    VirtualCharacter character(world, JPH::RVec3(0.0f, 2.0f, 0.0f));
    for (int i = 0; i < 256; ++i) character.step(JPH::Vec3::sZero());
    check(character.ground_state() == JPH::CharacterBase::EGroundState::OnGround,
          "virtual character lands on the static floor");
    check(std::abs(static_cast<float>(character.position().GetY())) < 0.02f,
          "virtual character rests at the floor surface");
}

void test_wall_collision() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    world.add_box(JPH::RVec3(0.0f, 2.0f, 4.0f), JPH::Vec3(2.0f, 2.0f, 0.1f));
    VirtualCharacter character(world);
    for (int i = 0; i < 128; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 8.0f));
    const float z = static_cast<float>(character.position().GetZ());
    check(z > 3.0f && z < 3.8f,
          "shape cast prevents the character capsule from passing through a wall");
}

void test_stair_traversal() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    world.add_box(JPH::RVec3(0.0f, 0.075f, 1.5f), JPH::Vec3(2.0f, 0.075f, 0.5f));
    world.add_box(JPH::RVec3(0.0f, 0.15f, 2.5f), JPH::Vec3(2.0f, 0.15f, 0.5f));
    world.add_box(JPH::RVec3(0.0f, 0.225f, 3.5f), JPH::Vec3(2.0f, 0.225f, 0.5f));
    VirtualCharacter character(world);
    for (int i = 0; i < 160; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 3.0f));
    check(character.position().GetZ() > 3.2,
          "virtual character traverses a three-step staircase");
    check(character.position().GetY() > 0.35,
          "virtual character reaches the raised stair surface");
}

void test_crouch_shape_clearance() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(5.0f, 0.5f, 5.0f));
    world.add_box(JPH::RVec3(0.0f, 1.45f, 0.0f), JPH::Vec3(2.0f, 0.05f, 2.0f));
    VirtualCharacter character(world, JPH::RVec3::sZero(), true);
    check(character.shape() == character.crouched_shape(),
          "character begins with the crouched collision shape");
    check(!character.set_crouched(false),
          "standing shape change is rejected when ceiling clearance is insufficient");
    check(character.shape() == character.crouched_shape(),
          "failed stance transition preserves the valid crouched shape");
}


struct PairResult {
    JPH::RVec3 left;
    JPH::RVec3 right;
};

void resolve_player_pair_symmetrically(VirtualCharacter& first, VirtualCharacter& second) {
    const JPH::RVec3 first_position = first.position();
    const JPH::RVec3 second_position = second.position();
    const JPH::AABox first_bounds = first.world_bounds();
    const JPH::AABox second_bounds = second.world_bounds();

    const float first_shape_radius = std::max({
        static_cast<float>(first_bounds.mMax.GetX() - first_position.GetX()),
        static_cast<float>(first_position.GetX() - first_bounds.mMin.GetX()),
        static_cast<float>(first_bounds.mMax.GetZ() - first_position.GetZ()),
        static_cast<float>(first_position.GetZ() - first_bounds.mMin.GetZ())});
    const float second_shape_radius = std::max({
        static_cast<float>(second_bounds.mMax.GetX() - second_position.GetX()),
        static_cast<float>(second_position.GetX() - second_bounds.mMin.GetX()),
        static_cast<float>(second_bounds.mMax.GetZ() - second_position.GetZ()),
        static_cast<float>(second_position.GetZ() - second_bounds.mMin.GetZ())});
    const float first_radius = first_shape_radius + first.character_padding();
    const float second_radius = second_shape_radius + second.character_padding();

    const float first_segment_min = static_cast<float>(first_bounds.mMin.GetY()) + first_shape_radius;
    const float first_segment_max = static_cast<float>(first_bounds.mMax.GetY()) - first_shape_radius;
    const float second_segment_min = static_cast<float>(second_bounds.mMin.GetY()) + second_shape_radius;
    const float second_segment_max = static_cast<float>(second_bounds.mMax.GetY()) - second_shape_radius;

    float vertical_delta = 0.0f;
    if (first_segment_max < second_segment_min) {
        vertical_delta = second_segment_min - first_segment_max;
    } else if (second_segment_max < first_segment_min) {
        vertical_delta = second_segment_max - first_segment_min;
    }

    const float dx = static_cast<float>(second_position.GetX() - first_position.GetX());
    const float dz = static_cast<float>(second_position.GetZ() - first_position.GetZ());
    const float separation = std::sqrt(dx * dx + vertical_delta * vertical_delta + dz * dz);
    const float penetration = first_radius + second_radius - separation;
    if (penetration <= 0.0f) return;

    float normal_x = 0.0f;
    float normal_y = 0.0f;
    float normal_z = 0.0f;
    if (separation > 1.0e-6f) {
        normal_x = dx / separation;
        normal_y = vertical_delta / separation;
        normal_z = dz / separation;
    } else {
        normal_x = first.stable_id() < second.stable_id() ? 1.0f : -1.0f;
    }

    const float correction = 0.5f * penetration;
    first.set_position(JPH::RVec3(
        first_position.GetX() - normal_x * correction,
        first_position.GetY() - normal_y * correction,
        first_position.GetZ() - normal_z * correction));
    second.set_position(JPH::RVec3(
        second_position.GetX() + normal_x * correction,
        second_position.GetY() + normal_y * correction,
        second_position.GetZ() + normal_z * correction));
}


void resolve_player_set_symmetrically(std::vector<VirtualCharacter*>& players) {
    std::sort(players.begin(), players.end(), [](const VirtualCharacter* a, const VirtualCharacter* b) {
        return a->stable_id() < b->stable_id();
    });
    for (int iteration = 0; iteration < 32; ++iteration) {
        for (std::size_t first = 0; first < players.size(); ++first) {
            for (std::size_t second = first + 1; second < players.size(); ++second) {
                resolve_player_pair_symmetrically(*players[first], *players[second]);
            }
        }
    }
}

struct PlayerGroupResult {
    std::vector<JPH::RVec3> positions;
    float minimum_separation = std::numeric_limits<float>::max();
};

PlayerGroupResult run_ten_player_group(const bool use_jolt_collision, const bool reverse_order) {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));

    std::vector<std::unique_ptr<VirtualCharacter>> owned_players;
    std::vector<VirtualCharacter*> players;
    std::vector<JPH::Vec3> inputs;
    owned_players.reserve(10);
    players.reserve(10);
    inputs.reserve(10);

    for (std::uint32_t index = 0; index < 10; ++index) {
        const float x = (static_cast<float>(index) - 4.5f) * 0.65f;
        owned_players.push_back(std::make_unique<VirtualCharacter>(
            world, JPH::RVec3(x, 0.0f, 0.0f), false, index + 1, use_jolt_collision));
        players.push_back(owned_players.back().get());
        inputs.emplace_back(index < 5 ? JPH::Vec3(2.0f, 0.0f, 0.0f)
                                      : JPH::Vec3(-2.0f, 0.0f, 0.0f));
    }

    for (int tick = 0; tick < 128; ++tick) {
        if (reverse_order) {
            for (std::size_t i = players.size(); i-- > 0;) players[i]->step(inputs[i]);
        } else {
            for (std::size_t i = 0; i < players.size(); ++i) players[i]->step(inputs[i]);
        }
        if (!use_jolt_collision) resolve_player_set_symmetrically(players);
    }

    PlayerGroupResult result;
    result.positions.reserve(players.size());
    for (const VirtualCharacter* player : players) result.positions.push_back(player->position());
    for (std::size_t first = 0; first < result.positions.size(); ++first) {
        for (std::size_t second = first + 1; second < result.positions.size(); ++second) {
            const float separation = distance(result.positions[first], result.positions[second]);
            result.minimum_separation = std::min(result.minimum_separation, separation);
        }
    }
    return result;
}

void test_ten_player_collision() {
    const PlayerGroupResult jolt_forward = run_ten_player_group(true, false);
    const PlayerGroupResult jolt_reverse = run_ten_player_group(true, true);
    const PlayerGroupResult symmetric_forward = run_ten_player_group(false, false);
    const PlayerGroupResult symmetric_reverse = run_ten_player_group(false, true);

    float jolt_max_order_error = 0.0f;
    float symmetric_max_order_error = 0.0f;
    for (std::size_t i = 0; i < jolt_forward.positions.size(); ++i) {
        jolt_max_order_error = std::max(
            jolt_max_order_error, distance(jolt_forward.positions[i], jolt_reverse.positions[i]));
        symmetric_max_order_error = std::max(
            symmetric_max_order_error, distance(symmetric_forward.positions[i], symmetric_reverse.positions[i]));
    }

    std::cout << "10-player Jolt simple max update-order delta: " << jolt_max_order_error << '\n';
    std::cout << "10-player symmetric resolver max update-order delta: "
              << symmetric_max_order_error << '\n';
    std::cout << "10-player minimum separation: Jolt=" << jolt_forward.minimum_separation
              << " symmetric=" << symmetric_forward.minimum_separation << '\n';
    check(jolt_forward.minimum_separation > 0.5f && jolt_reverse.minimum_separation > 0.5f,
          "Jolt character collision keeps ten players from overlapping");
    check(jolt_max_order_error < 0.05f,
          "Jolt ten-player collision is sufficiently stable under reversed update order");
    check(symmetric_forward.minimum_separation > 0.639f && symmetric_reverse.minimum_separation > 0.639f,
          "symmetric resolver respects capsule padding in a ten-player group");
    check(symmetric_max_order_error < 0.001f,
          "symmetric ten-player collision is independent of update order");
}


PairResult run_player_pair(const bool use_jolt_collision, const bool right_first,
                           const JPH::Vec3& left_input, const JPH::Vec3& right_input) {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    VirtualCharacter left(world, JPH::RVec3(-1.0f, 0.0f, 0.0f), false, 1, use_jolt_collision);
    VirtualCharacter right(world, JPH::RVec3(1.0f, 0.0f, 0.0f), false, 2, use_jolt_collision);
    for (int i = 0; i < 128; ++i) {
        if (right_first) {
            right.step(right_input);
            left.step(left_input);
        } else {
            left.step(left_input);
            right.step(right_input);
        }
        if (!use_jolt_collision) resolve_player_pair_symmetrically(left, right);
    }
    return PairResult{left.position(), right.position()};
}

void test_player_pair_scenario(const std::string_view label,
                               const JPH::Vec3& left_input,
                               const JPH::Vec3& right_input) {
    const PairResult jolt_left_first = run_player_pair(true, false, left_input, right_input);
    const PairResult jolt_right_first = run_player_pair(true, true, left_input, right_input);
    const float jolt_left_order_error = distance(jolt_left_first.left, jolt_right_first.left);
    const float jolt_right_order_error = distance(jolt_left_first.right, jolt_right_first.right);

    const PairResult symmetric_left_first = run_player_pair(false, false, left_input, right_input);
    const PairResult symmetric_right_first = run_player_pair(false, true, left_input, right_input);
    const float symmetric_left_order_error = distance(symmetric_left_first.left, symmetric_right_first.left);
    const float symmetric_right_order_error = distance(symmetric_left_first.right, symmetric_right_first.right);
    const float symmetric_separation = static_cast<float>(
        symmetric_left_first.right.GetX() - symmetric_left_first.left.GetX());
    const float jolt_left_first_separation = static_cast<float>(
        jolt_left_first.right.GetX() - jolt_left_first.left.GetX());
    const float jolt_right_first_separation = static_cast<float>(
        jolt_right_first.right.GetX() - jolt_right_first.left.GetX());

    std::cout << label << " / CharacterVirtual simple pair order delta: left=" << jolt_left_order_error
              << " right=" << jolt_right_order_error << '\n';
    std::cout << label << " / symmetric pair solver order delta: left=" << symmetric_left_order_error
              << " right=" << symmetric_right_order_error << '\n';
    check(jolt_left_first_separation > 0.5f && jolt_right_first_separation > 0.5f,
          "Jolt simple character collision prevents capsules from passing through each other");
    check(jolt_left_order_error < 0.05f && jolt_right_order_error < 0.05f,
          "Jolt simple character collision is independent of update order");
    check(symmetric_separation > 0.639f,
          "symmetric player collision respects capsule padding and prevents overlap");
    check(symmetric_left_order_error < 0.05f && symmetric_right_order_error < 0.05f,
          "symmetric player collision is independent of character update order");
}

void test_player_player_collision() {
    test_player_pair_scenario("equal head-on",
                              JPH::Vec3(2.0f, 0.0f, 0.0f),
                              JPH::Vec3(-2.0f, 0.0f, 0.0f));
    test_player_pair_scenario("moving into stationary",
                              JPH::Vec3(2.0f, 0.0f, 0.0f),
                              JPH::Vec3::sZero());
    test_player_pair_scenario("unequal head-on",
                              JPH::Vec3(1.0f, 0.0f, 0.0f),
                              JPH::Vec3(-3.0f, 0.0f, 0.0f));
}


struct WallPairResult {
    JPH::RVec3 left;
    JPH::RVec3 right;
    float wall_clearance = 0.0f;
    float player_separation = 0.0f;
};

WallPairResult run_wall_pinned_pair(const bool use_jolt_collision, const bool right_first,
                                    const bool project_world) {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    constexpr float wall_right_face = -0.6f;
    world.add_box(JPH::RVec3(-0.7f, 1.0f, 0.0f), JPH::Vec3(0.1f, 1.0f, 5.0f));

    VirtualCharacter left(world, JPH::RVec3(-0.28f, 0.0f, 0.0f), false, 1, use_jolt_collision);
    VirtualCharacter right(world, JPH::RVec3(0.5f, 0.0f, 0.0f), false, 2, use_jolt_collision);
    for (int tick = 0; tick < 128; ++tick) {
        if (right_first) {
            right.step(JPH::Vec3(-2.0f, 0.0f, 0.0f));
            left.step(JPH::Vec3::sZero());
        } else {
            left.step(JPH::Vec3::sZero());
            right.step(JPH::Vec3(-2.0f, 0.0f, 0.0f));
        }
        if (!use_jolt_collision) {
            resolve_player_pair_symmetrically(left, right);
            if (project_world) {
                left.project_world();
                right.project_world();
            }
        }
    }

    const JPH::AABox left_bounds = left.world_bounds();
    const float wall_clearance = static_cast<float>(left_bounds.mMin.GetX()) -
                                 left.character_padding() - wall_right_face;
    const float player_separation = distance(left.position(), right.position());
    return WallPairResult{left.position(), right.position(), wall_clearance, player_separation};
}

void test_player_collision_against_wall() {
    const WallPairResult jolt_forward = run_wall_pinned_pair(true, false, false);
    const WallPairResult jolt_reverse = run_wall_pinned_pair(true, true, false);
    const WallPairResult raw_forward = run_wall_pinned_pair(false, false, false);
    const WallPairResult projected_forward = run_wall_pinned_pair(false, false, true);
    const WallPairResult projected_reverse = run_wall_pinned_pair(false, true, true);

    const float projected_order_error = std::max(
        distance(projected_forward.left, projected_reverse.left),
        distance(projected_forward.right, projected_reverse.right));

    std::cout << "Wall-pinned pair clearance: Jolt=" << jolt_forward.wall_clearance
              << " raw symmetric=" << raw_forward.wall_clearance
              << " projected symmetric=" << projected_forward.wall_clearance << '\n';
    std::cout << "Wall-pinned pair separation: Jolt=" << jolt_forward.player_separation
              << " projected symmetric=" << projected_forward.player_separation << '\n';
    std::cout << "Wall-pinned symmetric solver update-order delta: " << projected_order_error << '\n';

    check(jolt_forward.wall_clearance > -0.001f && jolt_reverse.wall_clearance > -0.001f,
          "Jolt character collision keeps the wall-pinned player outside the wall");
    check(projected_forward.wall_clearance > -0.001f && projected_reverse.wall_clearance > -0.001f,
          "world projection prevents symmetric player resolution from pushing through a wall");
    check(projected_forward.player_separation > 0.63f && projected_reverse.player_separation > 0.63f,
          "world-projected symmetric solver keeps players separated");
    check(projected_order_error < 0.001f,
          "world-projected wall collision is independent of update order");
}


void test_character_state_replay() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    VirtualCharacter character(world);
    for (int i = 0; i < 40; ++i) character.step(JPH::Vec3(2.0f, 0.0f, 0.0f));

    JPH::StateRecorderImpl recorder;
    character.save_state(recorder);
    for (int i = 0; i < 20; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 2.0f));
    const JPH::RVec3 expected_position = character.position();
    const JPH::Vec3 expected_velocity = character.velocity();

    recorder.Rewind();
    character.restore_state(recorder);
    for (int i = 0; i < 20; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 2.0f));
    check(distance(character.position(), expected_position) < 1.0e-5f,
          "CharacterVirtual position is restored and replayed consistently");
    check((character.velocity() - expected_velocity).Length() < 1.0e-5f,
          "CharacterVirtual velocity is restored and replayed consistently");
}

} // namespace

int main() {
    initialize_jolt();
    test_fall_to_floor();
    test_wall_collision();
    test_stair_traversal();
    test_crouch_shape_clearance();
    test_player_player_collision();
    test_ten_player_collision();
    test_player_collision_against_wall();
    test_character_state_replay();

    if (failures != 0) {
        std::cerr << failures << " Jolt collision test(s) failed\n";
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
        return EXIT_FAILURE;
    }
    std::cout << "All Jolt collision tests passed\n";
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    return EXIT_SUCCESS;
}

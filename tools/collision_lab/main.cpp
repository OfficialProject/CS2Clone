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
    void set_position(const JPH::RVec3& position) { character_->SetPosition(position); }

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

    const float first_radius = std::max({
        static_cast<float>(first_bounds.mMax.GetX() - first_position.GetX()),
        static_cast<float>(first_position.GetX() - first_bounds.mMin.GetX()),
        static_cast<float>(first_bounds.mMax.GetZ() - first_position.GetZ()),
        static_cast<float>(first_position.GetZ() - first_bounds.mMin.GetZ())});
    const float second_radius = std::max({
        static_cast<float>(second_bounds.mMax.GetX() - second_position.GetX()),
        static_cast<float>(second_position.GetX() - second_bounds.mMin.GetX()),
        static_cast<float>(second_bounds.mMax.GetZ() - second_position.GetZ()),
        static_cast<float>(second_position.GetZ() - second_bounds.mMin.GetZ())});

    const float first_segment_min = static_cast<float>(first_bounds.mMin.GetY()) + first_radius;
    const float first_segment_max = static_cast<float>(first_bounds.mMax.GetY()) - first_radius;
    const float second_segment_min = static_cast<float>(second_bounds.mMin.GetY()) + second_radius;
    const float second_segment_max = static_cast<float>(second_bounds.mMax.GetY()) - second_radius;

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

PairResult run_player_pair(const bool use_jolt_collision, const bool right_first) {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0f, -0.5f, 0.0f), JPH::Vec3(20.0f, 0.5f, 20.0f));
    VirtualCharacter left(world, JPH::RVec3(-1.0f, 0.0f, 0.0f), false, 1, use_jolt_collision);
    VirtualCharacter right(world, JPH::RVec3(1.0f, 0.0f, 0.0f), false, 2, use_jolt_collision);
    for (int i = 0; i < 128; ++i) {
        if (right_first) {
            right.step(JPH::Vec3(-2.0f, 0.0f, 0.0f));
            left.step(JPH::Vec3(2.0f, 0.0f, 0.0f));
        } else {
            left.step(JPH::Vec3(2.0f, 0.0f, 0.0f));
            right.step(JPH::Vec3(-2.0f, 0.0f, 0.0f));
        }
        if (!use_jolt_collision) resolve_player_pair_symmetrically(left, right);
    }
    return PairResult{left.position(), right.position()};
}

void test_player_player_collision() {
    const PairResult jolt_left_first = run_player_pair(true, false);
    const PairResult jolt_right_first = run_player_pair(true, true);
    const float jolt_left_order_error = distance(jolt_left_first.left, jolt_right_first.left);
    const float jolt_right_order_error = distance(jolt_left_first.right, jolt_right_first.right);

    const PairResult symmetric_left_first = run_player_pair(false, false);
    const PairResult symmetric_right_first = run_player_pair(false, true);
    const float symmetric_left_order_error = distance(symmetric_left_first.left, symmetric_right_first.left);
    const float symmetric_right_order_error = distance(symmetric_left_first.right, symmetric_right_first.right);
    const float symmetric_separation = static_cast<float>(
        symmetric_left_first.right.GetX() - symmetric_left_first.left.GetX());
    const float jolt_left_first_separation = static_cast<float>(
        jolt_left_first.right.GetX() - jolt_left_first.left.GetX());
    const float jolt_right_first_separation = static_cast<float>(
        jolt_right_first.right.GetX() - jolt_right_first.left.GetX());

    std::cout << "CharacterVirtual simple pair order delta: left=" << jolt_left_order_error
              << " right=" << jolt_right_order_error << '\n';
    std::cout << "Symmetric pair solver order delta: left=" << symmetric_left_order_error
              << " right=" << symmetric_right_order_error << '\n';
    check(jolt_left_first_separation > 0.5f && jolt_right_first_separation > 0.5f,
          "Jolt simple character collision prevents capsules from passing through each other");
    check(jolt_left_order_error < 0.05f && jolt_right_order_error < 0.05f,
          "Jolt simple character collision is independent of update order");
    check(symmetric_separation > 0.5f,
          "symmetric player collision prevents two capsules from passing through each other");
    check(symmetric_left_order_error < 0.05f && symmetric_right_order_error < 0.05f,
          "symmetric player collision is independent of character update order");
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

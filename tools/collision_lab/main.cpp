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

private:
    BroadPhaseLayerInterfaceImpl broad_phase_layers_;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter_;
    ObjectLayerPairFilterImpl object_layer_pair_filter_;
    std::unique_ptr<JPH::TempAllocatorImpl> allocator_;
    JPH::PhysicsSystem physics_;
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
                              const bool start_crouched = false)
        : world_(world),
          standing_shape_(make_capsule(1.0f, 0.3f)),
          crouched_shape_(make_capsule(0.4f, 0.3f)) {
        JPH::CharacterVirtualSettings settings;
        settings.mShape = start_crouched ? crouched_shape_ : standing_shape_;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -0.3f);
        character_ = new JPH::CharacterVirtual(&settings, position, JPH::Quat::sIdentity(),
                                                &world_.physics());
        update_settings_.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
        update_settings_.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.4f, 0.0f);
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

    [[nodiscard]] bool set_crouched(const bool crouched) {
        const JPH::Shape* target = crouched ? crouched_shape_.Get() : standing_shape_.Get();
        JPH::PhysicsSystem& physics = world_.physics();
        return character_->SetShape(
            target, 0.0f,
            physics.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
            physics.GetDefaultLayerFilter(Layers::Moving), {}, {}, world_.allocator());
    }

    void save_state(JPH::StateRecorderImpl& recorder) const { character_->SaveState(recorder); }
    void restore_state(JPH::StateRecorderImpl& recorder) { character_->RestoreState(recorder); }

    [[nodiscard]] const JPH::Shape* crouched_shape() const { return crouched_shape_.Get(); }

private:
    JoltWorld& world_;
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
    world.add_box(JPH::RVec3(0.0, -0.5, 0.0), JPH::Vec3(20.0f, 0.5f, 20.0f));
    VirtualCharacter character(world, JPH::RVec3(0.0, 2.0, 0.0));
    for (int i = 0; i < 256; ++i) character.step(JPH::Vec3::sZero());
    check(character.ground_state() == JPH::CharacterBase::EGroundState::OnGround,
          "virtual character lands on the static floor");
    check(std::abs(static_cast<float>(character.position().GetY())) < 0.02f,
          "virtual character rests at the floor surface");
}

void test_wall_collision() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0, -0.5, 0.0), JPH::Vec3(20.0f, 0.5f, 20.0f));
    world.add_box(JPH::RVec3(0.0, 2.0, 4.0), JPH::Vec3(2.0f, 2.0f, 0.1f));
    VirtualCharacter character(world);
    for (int i = 0; i < 128; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 8.0f));
    const float z = static_cast<float>(character.position().GetZ());
    check(z > 3.0f && z < 3.8f,
          "shape cast prevents the character capsule from passing through a wall");
}

void test_stair_traversal() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0, -0.5, 0.0), JPH::Vec3(20.0f, 0.5f, 20.0f));
    world.add_box(JPH::RVec3(0.0, 0.075, 1.5), JPH::Vec3(2.0f, 0.075f, 0.5f));
    world.add_box(JPH::RVec3(0.0, 0.15, 2.5), JPH::Vec3(2.0f, 0.15f, 0.5f));
    world.add_box(JPH::RVec3(0.0, 0.225, 3.5), JPH::Vec3(2.0f, 0.225f, 0.5f));
    VirtualCharacter character(world);
    for (int i = 0; i < 384; ++i) character.step(JPH::Vec3(0.0f, 0.0f, 3.0f));
    check(character.position().GetZ() > 4.5,
          "virtual character traverses a three-step staircase");
    check(character.position().GetY() > 0.35,
          "virtual character reaches the raised stair surface");
}

void test_crouch_shape_clearance() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0, -0.5, 0.0), JPH::Vec3(5.0f, 0.5f, 5.0f));
    world.add_box(JPH::RVec3(0.0, 1.45, 0.0), JPH::Vec3(2.0f, 0.05f, 2.0f));
    VirtualCharacter character(world, JPH::RVec3::sZero(), true);
    check(character.shape() == character.crouched_shape(),
          "character begins with the crouched collision shape");
    check(!character.set_crouched(false),
          "standing shape change is rejected when ceiling clearance is insufficient");
    check(character.shape() == character.crouched_shape(),
          "failed stance transition preserves the valid crouched shape");
}

void test_character_state_replay() {
    JoltWorld world;
    world.add_box(JPH::RVec3(0.0, -0.5, 0.0), JPH::Vec3(20.0f, 0.5f, 20.0f));
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

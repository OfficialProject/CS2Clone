#pragma once

#include "map_collision.hpp"
#include "game/simulation/movement.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace feel_lab {

class ImportedMap {
    std::vector<Triangle> visual;
    std::vector<Triangle> ladders;
    std::vector<Mesh> meshes;
    Material material{};
    bool uploaded = false;
public:
    MapCollision collision;
    Vector3 spawn{};
    float spawn_yaw = 0;
    float lowest = 0;
    bool original = false;

    void load_original(Vector3 start, std::vector<Triangle> geometry) {
        original=true;
        spawn=start;
        lowest=start.y;
        for (const auto& triangle : geometry)
            lowest=std::min({lowest,triangle.a.y,triangle.b.y,triangle.c.y});
        collision.build(std::move(geometry));
    }

    bool load_embedded(const std::filesystem::path& executable) {
        std::ifstream file(executable, std::ios::binary | std::ios::ate);
        const auto end = file.tellg();
        constexpr std::streamoff footer_size = 16;
        if (end < footer_size) return false;
        file.seekg(end - footer_size);
        std::uint64_t size = 0;
        std::array<char, 8> tag{};
        file.read(reinterpret_cast<char*>(&size), static_cast<std::streamsize>(sizeof(size)));
        file.read(tag.data(), static_cast<std::streamsize>(tag.size()));
        if (!file || std::memcmp(tag.data(), "CS2PACK1", tag.size()) != 0) return false;
        const auto payload_end = static_cast<std::streamoff>(end) - footer_size;
        if (size > static_cast<std::uint64_t>(payload_end))
            throw std::runtime_error("Invalid embedded Mirage map");
        file.seekg(payload_end - static_cast<std::streamoff>(size));
        load_geometry(file, size);
        return true;
    }

    void load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Could not open playtest map");
        load_geometry(file, std::filesystem::file_size(path));
    }

private:
    void load_geometry(std::istream& file, std::uint64_t size) {
        std::array<char,8> magic{};
        std::uint32_t render_count=0, collision_count=0, ladder_count=0;
        file.read(magic.data(), 8);
        file.read(reinterpret_cast<char*>(&render_count), 4);
        file.read(reinterpret_cast<char*>(&collision_count), 4);
        const bool has_ladders = std::memcmp(magic.data(),"CS2MAP02",8) == 0;
        if (has_ladders) file.read(reinterpret_cast<char*>(&ladder_count), 4);
        file.read(reinterpret_cast<char*>(&spawn), 12);
        file.read(reinterpret_cast<char*>(&spawn_yaw), 4);
        if (!file || (!has_ladders && std::memcmp(magic.data(),"CS2MAP01",8) != 0) ||
            render_count == 0 || render_count > 4000000 || collision_count == 0 || collision_count > 2000000 ||
            ladder_count > 10000 ||
            size != (has_ladders ? 36ull : 32ull) + (render_count+collision_count+ladder_count)*36ull)
            throw std::runtime_error("Invalid playtest map header");
        if (!std::isfinite(spawn.x) || !std::isfinite(spawn.y) || !std::isfinite(spawn.z) ||
            !std::isfinite(spawn_yaw)) throw std::runtime_error("Invalid map spawn");
        static_assert(sizeof(Triangle)==36 && sizeof(Vector3)==12);
        visual.resize(render_count);
        std::vector<Triangle> solid(collision_count);
        file.read(reinterpret_cast<char*>(visual.data()), static_cast<std::streamsize>(render_count)*36);
        file.read(reinterpret_cast<char*>(solid.data()), static_cast<std::streamsize>(collision_count)*36);
        ladders.resize(ladder_count);
        file.read(reinterpret_cast<char*>(ladders.data()), static_cast<std::streamsize>(ladder_count)*36);
        if (!file) throw std::runtime_error("Truncated map geometry");
        const auto valid = [](const Triangle& t) {
            for (const auto p : {t.a,t.b,t.c}) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    std::max({std::fabs(p.x),std::fabs(p.y),std::fabs(p.z)}) > 10000) return false;
            }
            return true;
        };
        if (!std::all_of(visual.begin(),visual.end(),valid) || !std::all_of(solid.begin(),solid.end(),valid) ||
            !std::all_of(ladders.begin(),ladders.end(),valid))
            throw std::runtime_error("Invalid map vertices");
        lowest = spawn.y;
        for (const auto& t : solid) lowest=std::min({lowest,t.a.y,t.b.y,t.c.y});
        collision.build(std::move(solid));
    }

public:
    bool loaded() const { return original || uploaded || !visual.empty(); }

    game::simulation::MovementState spawn_player() const { return spawn_player(spawn); }

    game::simulation::MovementState spawn_player(Vector3 start) const {
        using namespace game::simulation;
        MovementState player;
        player.position={start.x,start.z};
        player.vertical_position=start.y;
        player.mode=MovementMode::Airborne;
        const auto floor=collision.sweep(start,{0,-2,0},hull_height(0));
        if (floor.fraction<1 && floor.normal.y>=0.7f) {
            player.vertical_position=start.y-floor.fraction*2+0.001f;
            player.mode=MovementMode::Grounded;
        }
        return player;
    }

private:
    struct LadderContact {
        Vector3 normal{};
        float top = 0;
        float distance = 0;
    };

    LadderContact ladder_at(Vector3 feet, float height) const {
        LadderContact nearest;
        nearest.distance = std::numeric_limits<float>::infinity();
        for (const auto& face : ladders) {
            auto normal = Vector3CrossProduct(Vector3Subtract(face.b,face.a),
                                              Vector3Subtract(face.c,face.a));
            normal.y = 0;
            if (Vector3LengthSqr(normal) < 1e-8f) continue;
            normal = Vector3Normalize(normal);
            const Vector3 tangent{-normal.z,0,normal.x};
            const float a = Vector3DotProduct(face.a,tangent);
            const float b = Vector3DotProduct(face.b,tangent);
            const float c = Vector3DotProduct(face.c,tangent);
            const float across = Vector3DotProduct(feet,tangent);
            const float top = std::max({face.a.y,face.b.y,face.c.y});
            const float bottom = std::min({face.a.y,face.b.y,face.c.y});
            if (across < std::min({a,b,c})-MapCollision::radius ||
                across > std::max({a,b,c})+MapCollision::radius ||
                feet.y+height*0.65f < bottom || feet.y > top+0.18f) continue;
            // Tilted ladders shift horizontally as the player climbs.
            const Vector3 plane = Vector3CrossProduct(Vector3Subtract(face.b,face.a),
                                                       Vector3Subtract(face.c,face.a));
            const Vector3 at_height{feet.x,std::clamp(feet.y+height*0.45f,bottom,top),feet.z};
            const float distance = Vector3DotProduct(Vector3Subtract(at_height,face.a),plane) /
                                   std::hypot(plane.x,plane.z);
            const float reach = MapCollision::radius*(std::abs(normal.x)+std::abs(normal.z))+0.18f;
            if (std::abs(distance) <= reach && std::abs(distance) < nearest.distance)
                nearest = {Vector3Scale(normal,distance < 0 ? -1.0f : 1.0f),top,std::abs(distance)};
        }
        return nearest;
    }

    static void apply_motion(game::simulation::MovementState& state, const Motion& motion) {
        using namespace game::simulation;
        state.position = {motion.position.x,motion.position.z};
        state.vertical_position = motion.position.y;
        state.velocity = {motion.velocity.x,motion.velocity.z};
        state.vertical_velocity = motion.grounded ? 0 : motion.velocity.y;
        state.mode = motion.grounded ? MovementMode::Grounded : MovementMode::Airborne;
    }

public:
    void advance(const game::simulation::MovementSimulator& simulation,
                 game::simulation::MovementState& state,
                 game::simulation::MovementInput input) const {
        using namespace game::simulation;
        const auto old=state;
        const Vector3 feet{static_cast<float>(old.position.x),
                           static_cast<float>(old.vertical_position),
                           static_cast<float>(old.position.y)};
        const float height = hull_height(old.crouch_fraction);
        auto ladder = ladder_at(feet,height);
        if (old.on_ladder)
            ladder.normal = {static_cast<float>(old.ladder_normal.x),0,static_cast<float>(old.ladder_normal.y)};
        const Vector3 wish{static_cast<float>(input.right),0,static_cast<float>(input.forward)};
        const float into = -Vector3DotProduct(wish,ladder.normal);
        const Vector3 old_velocity{static_cast<float>(old.velocity.x),0,static_cast<float>(old.velocity.y)};
        state.on_ladder = std::isfinite(ladder.distance) &&
            (old.on_ladder || (into > 0.1f && Vector3DotProduct(old_velocity,ladder.normal) < 0.25f &&
                (old.mode != MovementMode::Grounded || feet.y < ladder.top-0.025f)));
        if (state.on_ladder) {
            state.ladder_normal = {ladder.normal.x,ladder.normal.z};
            state.mode = MovementMode::Airborne;
            if (input.jump_pressed) {
                state.on_ladder = false;
                state.velocity = {ladder.normal.x*3.0,ladder.normal.z*3.0};
                state.vertical_velocity = 2.0;
                input.jump_pressed = false;
            } else {
                constexpr float full_climb_speed = 200.0f * static_cast<float>(MovementConfig::source_unit);
                const float climb_speed = (input.crouch_held || input.walk_held) ?
                    full_climb_speed * 0.34f : full_climb_speed;
                const auto across = Vector3Scale(Vector3Add(wish,Vector3Scale(ladder.normal,into)),
                                                 input.crouch_held ? 1.0f : 0.5f);
                Vector3 direction{across.x,into,across.z};
                const bool dismounting = into > 0 && feet.y >= ladder.top+0.015f;
                if (dismounting) {
                    direction = wish;
                }
                const auto velocity = Vector3Scale(direction,climb_speed/std::max(1.0f,Vector3Length(direction)));
                const auto motion = collision.move(feet,velocity,
                    Vector3Scale(velocity,static_cast<float>(simulation.config().fixed_dt)),height,false);
                apply_motion(state,motion);
                state.on_ladder = (!motion.grounded || (into >= 0 && !dismounting)) &&
                                  std::isfinite(ladder_at(motion.position,height).distance);
                if (state.on_ladder) state.mode = MovementMode::Airborne;
                ++state.tick;
                return;
            }
        }
        const bool center_stance = old.mode==MovementMode::Airborne && !old.on_ladder;
        if (!input.crouch_held && old.crouch_fraction>0) {
            const auto& cfg=simulation.config();
            const double next=std::max(0.0,old.crouch_fraction-cfg.fixed_dt/cfg.crouch_transition_seconds);
            const float growth=hull_height(next)-hull_height(old.crouch_fraction);
            const float upward_growth=center_stance ? growth*0.5f : growth;
            const bool head_blocked=collision.sweep(feet,{0,upward_growth+0.002f,0},height).fraction<1;
            const bool feet_blocked=center_stance && collision.sweep(feet,{0,-growth*0.5f,0},height).fraction<1;
            if (head_blocked || feet_blocked)
                input.crouch_held=true;
        }
        (void)simulation.step(state,input);
        const Vector3 velocity{static_cast<float>(state.velocity.x),
                               static_cast<float>(state.vertical_velocity),
                               static_cast<float>(state.velocity.y)};
        const Vector3 delta{static_cast<float>(state.position.x-old.position.x),
                            static_cast<float>(state.vertical_position-old.vertical_position),
                            static_cast<float>(state.position.y-old.position.y)};
        const float next_height=hull_height(state.crouch_fraction);
        auto stance_origin=feet;
        if (center_stance) {
            const float offset=(height-next_height)*0.5f;
            const float clearance=offset>0 ? collision.sweep(feet,{0,offset,0},next_height).fraction : 1.0f;
            stance_origin.y+=offset*clearance;
        }
        const auto motion=collision.move(stance_origin,velocity,delta,next_height,
                                         old.mode==MovementMode::Grounded && !input.jump_pressed);
        apply_motion(state,motion);
    }

    static float hull_height(double crouch_fraction) {
        return (72.0f-18.0f*static_cast<float>(crouch_fraction)) *
               static_cast<float>(game::simulation::MovementConfig::source_unit);
    }

    static float eye_height(double crouch_fraction) {
        return (64.0f-18.0f*static_cast<float>(crouch_fraction)) *
               static_cast<float>(game::simulation::MovementConfig::source_unit);
    }

    void upload() {
        material = LoadMaterialDefault();
        const Vector3 sun = Vector3Normalize({-0.5f,0.8f,0.3f});
        constexpr std::size_t chunk = 8192;
        for (std::size_t start=0; start<visual.size(); start+=chunk) {
            const auto count=std::min(chunk,visual.size()-start);
            Mesh mesh{};
            mesh.triangleCount=static_cast<int>(count);
            mesh.vertexCount=mesh.triangleCount*3;
            mesh.vertices=static_cast<float*>(MemAlloc(static_cast<unsigned>(count*9*sizeof(float))));
            mesh.colors=static_cast<unsigned char*>(MemAlloc(static_cast<unsigned>(count*12)));
            for (std::size_t index=0; index<count; ++index) {
                const auto& t=visual[start+index];
                const auto normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(t.b,t.a),Vector3Subtract(t.c,t.a)));
                const float brightness=0.40f+0.53f*std::max(0.0f,Vector3DotProduct(normal,sun));
                const auto shade=static_cast<unsigned char>(brightness*255);
                const std::array<Vector3,3> points{t.a,t.b,t.c};
                for (std::size_t vertex=0; vertex<3; ++vertex) {
                    const auto v=index*3+vertex;
                    std::memcpy(mesh.vertices+v*3,&points[vertex],12);
                    mesh.colors[v*4]=shade;
                    mesh.colors[v*4+1]=shade;
                    mesh.colors[v*4+2]=shade;
                    mesh.colors[v*4+3]=255;
                }
            }
            UploadMesh(&mesh,false);
            meshes.push_back(mesh);
        }
        uploaded=true;
        std::vector<Triangle>().swap(visual);
    }

    void draw() const {
        for (const auto& mesh : meshes) DrawMesh(mesh,material,MatrixIdentity());
    }

    void unload() {
        if (!uploaded) return;
        for (auto& mesh : meshes) UnloadMesh(mesh);
        meshes.clear();
        UnloadMaterial(material);
        uploaded=false;
    }
};

} // namespace feel_lab

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
    std::vector<Mesh> meshes;
    Material material{};
    bool uploaded = false;
public:
    MapCollision collision;
    Vector3 spawn{};
    float spawn_yaw = 0;
    float lowest = 0;

    void load(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::array<char,8> magic{};
        std::uint32_t render_count=0, collision_count=0;
        file.read(magic.data(), 8);
        file.read(reinterpret_cast<char*>(&render_count), 4);
        file.read(reinterpret_cast<char*>(&collision_count), 4);
        file.read(reinterpret_cast<char*>(&spawn), 12);
        file.read(reinterpret_cast<char*>(&spawn_yaw), 4);
        if (!file || std::memcmp(magic.data(),"CS2MAP01",8) != 0 ||
            render_count == 0 || render_count > 4000000 || collision_count == 0 || collision_count > 2000000 ||
            std::filesystem::file_size(path) != 32ull + (render_count+collision_count)*36ull)
            throw std::runtime_error("Invalid playtest map header");
        if (!std::isfinite(spawn.x) || !std::isfinite(spawn.y) || !std::isfinite(spawn.z) ||
            !std::isfinite(spawn_yaw)) throw std::runtime_error("Invalid map spawn");
        static_assert(sizeof(Triangle)==36 && sizeof(Vector3)==12);
        visual.resize(render_count);
        std::vector<Triangle> solid(collision_count);
        file.read(reinterpret_cast<char*>(visual.data()), static_cast<std::streamsize>(render_count)*36);
        file.read(reinterpret_cast<char*>(solid.data()), static_cast<std::streamsize>(collision_count)*36);
        if (!file) throw std::runtime_error("Truncated map geometry");
        const auto valid = [](const Triangle& t) {
            for (const auto p : {t.a,t.b,t.c}) {
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                    std::max({std::fabs(p.x),std::fabs(p.y),std::fabs(p.z)}) > 10000) return false;
            }
            return true;
        };
        if (!std::all_of(visual.begin(),visual.end(),valid) || !std::all_of(solid.begin(),solid.end(),valid))
            throw std::runtime_error("Invalid map vertices");
        lowest = spawn.y;
        for (const auto& t : solid) lowest=std::min({lowest,t.a.y,t.b.y,t.c.y});
        collision.build(std::move(solid));
    }

    bool loaded() const { return uploaded || !visual.empty(); }

    game::simulation::MovementState spawn_player() const {
        using namespace game::simulation;
        MovementState player;
        player.position={spawn.x,spawn.z};
        player.vertical_position=spawn.y;
        player.mode=MovementMode::Airborne;
        const auto floor=collision.sweep(spawn,{0,-2,0},hull_height(0));
        if (floor.fraction<1 && floor.normal.y>=0.7f) {
            player.vertical_position=spawn.y-floor.fraction*2+0.001f;
            player.mode=MovementMode::Grounded;
        }
        return player;
    }

    void advance(const game::simulation::MovementSimulator& simulation,
                 game::simulation::MovementState& state,
                 game::simulation::MovementInput input) const {
        using namespace game::simulation;
        const auto old=state;
        const Vector3 feet{static_cast<float>(old.position.x),
                           static_cast<float>(old.vertical_position),
                           static_cast<float>(old.position.y)};
        if (!input.crouch_held && old.crouch_fraction>0) {
            const auto& cfg=simulation.config();
            const double next=std::max(0.0,old.crouch_fraction-cfg.fixed_dt/cfg.crouch_transition_seconds);
            const float growth=hull_height(next)-hull_height(old.crouch_fraction);
            if (collision.sweep(feet,{0,growth+0.002f,0},hull_height(old.crouch_fraction)).fraction<1)
                input.crouch_held=true;
        }
        (void)simulation.step(state,input);
        const Vector3 velocity{static_cast<float>(state.velocity.x),
                               static_cast<float>(state.vertical_velocity),
                               static_cast<float>(state.velocity.y)};
        const Vector3 delta{static_cast<float>(state.position.x-old.position.x),
                            static_cast<float>(state.vertical_position-old.vertical_position),
                            static_cast<float>(state.position.y-old.position.y)};
        const auto motion=collision.move(feet,velocity,delta,hull_height(state.crouch_fraction),
                                         old.mode==MovementMode::Grounded && !input.jump_pressed);
        state.position={motion.position.x,motion.position.z};
        state.vertical_position=motion.position.y;
        state.velocity={motion.velocity.x,motion.velocity.z};
        state.vertical_velocity=motion.grounded?0:motion.velocity.y;
        state.mode=motion.grounded?MovementMode::Grounded:MovementMode::Airborne;
    }

    static float hull_height(double crouch_fraction) {
        return (72.0f-18.0f*static_cast<float>(crouch_fraction)) *
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

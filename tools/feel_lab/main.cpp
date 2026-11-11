#include "drills.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "frontend.hpp"
#include "match_hud.hpp"
#include "imported_map.hpp"
#include "sound_cues.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace game::simulation;

namespace {
struct Wall { Vector3 center; Vector3 size; };
constexpr Wall walls[] = {
    {{0, 1.5f, -15}, {30, 3, 1}}, {{0, 1.5f, 15}, {30, 3, 1}},
    {{-15, 1.5f, 0}, {1, 3, 30}}, {{15, 1.5f, 0}, {1, 3, 30}},
    {{-4, 1.5f, -3}, {1, 3, 9}}, {{5, 1.5f, 2}, {1, 3, 9}},
    {{0, 1.5f, 7}, {8, 3, 1}}, {{-9, 1.5f, 5}, {5, 3, 1}}
};
constexpr std::array<Vector3,5> range_targets{{{0,1.5f,-10},{-10,1.5f,-8},{9,1.5f,-8},{9,1.5f,10},{-10,1.5f,10}}};
struct Bombsite {
    Vector3 min;
    Vector3 max;

    bool contains(Vector3 feet) const {
        return feet.x >= min.x && feet.x <= max.x && feet.z >= min.z && feet.z <= max.z &&
               feet.y >= min.y-0.1f && feet.y <= max.y+0.4f;
    }

    Vector3 center() const {
        return {(min.x+max.x)/2,min.y+0.04f,(min.z+max.z)/2};
    }
};

// Bounds of the two func_bomb_target brushes in the imported Mirage package.
constexpr Bombsite mirage_a{{-54.01f,-4.176f,-14.30f},{-45.75f,-3.619f,-6.124f}};
constexpr Bombsite mirage_b{{1.67f,-3.689f,-51.42f},{10.21f,-3.317f,-43.61f}};
constexpr Vector3 mirage_ct{-45.8432f,-5.548f,-41.2032f};

constexpr int magazine_size = 30;
constexpr double shot_interval = 0.1;
constexpr double reload_seconds = 2.5;

std::array<Vector3,5> mirage_targets(const feel_lab::ImportedMap& map) {
    constexpr float scale = static_cast<float>(MovementConfig::source_unit);
    // Source entity coordinates for A ramp, A site, top mid, catwalk and B site.
    constexpr std::array<Vector3,5> places{{
        {512.5f,-1625,-206.719f}, {-662.5f,-1612.5f,-118.719f},
        {475,-900,-107.024f}, {-125,-425,-121.294f}, {-2550,162.5f,-110.861f}}};
    std::array<Vector3,5> targets{};
    for (std::size_t i=0; i<places.size(); ++i) {
        const auto p=places[i];
        auto start=Vector3{p.y*scale,p.z*scale+1.0f,p.x*scale};
        const auto floor=map.collision.sweep(start,{0,-4,0},0.01f);
        if (floor.fraction<1 && floor.normal.y>=0.7f) start.y-=4*floor.fraction;
        else start.y-=1.0f;
        start.y+=1.25f;
        targets[i]=start;
    }
    return targets;
}
constexpr float player_radius = 0.28f;
constexpr float standing_eye = 1.65f;
constexpr float crouched_eye = 1.05f;
bool intersects(const float x, const float z, const float height) {
    for (const Wall& w : walls) {
        const float dx = std::max(std::fabs(x - w.center.x) - w.size.x * 0.5f, 0.0f);
        const float dz = std::max(std::fabs(z - w.center.z) - w.size.z * 0.5f, 0.0f);
        if (dx*dx + dz*dz < player_radius*player_radius && height < 3.0f) return true;
    }
    return false;
}
void draw_weapon(feel_lab::Weapon weapon, float kick, float reload_pose, float sway) {
    // Separate 60-degree horizontal viewmodel projection, on a 4:3 basis.
    static const float vertical_fov=2*std::atan(0.75f*std::tan(30*DEG2RAD))*RAD2DEG;
    const Camera3D camera{{0,0,0},{0,0,-1},{0,1,0},vertical_fov,CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    rlDisableDepthTest();
    const float z=-0.60f+kick*0.06f;
    const float y=-0.20f+kick*0.035f-reload_pose*0.19f+sway;
    const Color metal{64,71,75,255};
    DrawCube({0.23f,y-0.07f,z+0.08f},0.12f,0.14f,0.24f,{106,87,66,255});
    if (weapon==feel_lab::Weapon::Knife) {
        DrawCube({0.22f,y,z-0.09f},0.045f,0.035f,0.24f,{173,182,188,255});
        DrawCube({0.22f,y,z+0.05f},0.07f,0.06f,0.10f,metal);
    } else {
        const bool rifle=weapon==feel_lab::Weapon::Rifle;
        DrawCube({0.21f,y,z},0.105f,0.08f,rifle?0.37f:0.23f,metal);
        DrawCube({0.21f,y-0.07f-reload_pose*0.075f,z+0.04f},0.085f,0.12f,0.075f,{43,47,51,255});
        DrawCube({0.21f,y+0.01f,z-(rifle?0.23f:0.08f)},0.038f,0.036f,rifle?0.25f:0.05f,{35,41,44,255});
        DrawCube({0.21f,y+0.052f,z-0.10f},0.037f,0.026f,0.025f,{35,41,44,255});
        if (rifle) {
            DrawCube({0.21f,y-0.012f,z+0.24f},0.086f,0.062f,0.19f,{46,54,54,255});
            DrawCube({0.21f,y-0.025f,z-0.19f},0.089f,0.070f,0.18f,{103,85,60,255});
            DrawCube({0.21f,y+0.048f,z-0.32f},0.023f,0.060f,0.025f,metal);
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthTest();
    EndMode3D();
}

bool ray_hits_wall(const Ray ray, float& nearest) {
    bool hit = false;
    for (const Wall& w : walls) {
        const BoundingBox box = {
            {w.center.x-w.size.x/2, w.center.y-w.size.y/2, w.center.z-w.size.z/2},
            {w.center.x+w.size.x/2, w.center.y+w.size.y/2, w.center.z+w.size.z/2}
        };
        const RayCollision c = GetRayCollisionBox(ray, box);
        if (c.hit && c.distance < nearest) {nearest=c.distance; hit=true;}
    }
    return hit;
}
}

bool export_default_drills(const MovementSimulator& simulation) {
    std::error_code error;
    const auto temp = std::filesystem::temp_directory_path(error);
    if (error) {
        std::fprintf(stderr, "Could not locate the temporary folder: %s\n", error.message().c_str());
        return false;
    }
    const auto directory = temp / "CS2Clone";
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::fprintf(stderr, "Could not create the CSV folder: %s\n", error.message().c_str());
        return false;
    }
    const auto path = directory / "movement-drills.csv";
    const bool saved = feel_lab::record_drills(path, simulation);
    const auto display = path.u8string();
    std::fprintf(saved ? stdout : stderr, "%s %s\n",
                 saved ? "Saved" : "Could not write", reinterpret_cast<const char*>(display.c_str()));
    return saved;
}

int run_lab(int argc, char* argv[]) {
    MovementConfig cfg;
    if (argc == 2 && std::string_view(argv[1]) == "--record-drills") {
        return export_default_drills(MovementSimulator(cfg)) ? 0 : 1;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--record-drills") {
        if (feel_lab::record_drills(argv[2], MovementSimulator(cfg))) return 0;
        std::fprintf(stderr, "Could not write drill trace: %s\n", argv[2]);
        return 1;
    }
    std::filesystem::path map_path;
    std::filesystem::path capture_path;
    bool validate = false, overview = false, range = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if ((option == "--map" || option == "--capture") && index+1 < argc) {
            if (option == "--map") map_path=argv[++index];
            else capture_path=argv[++index];
        } else if (option == "--validate-map" && index+1 < argc) {
            map_path=argv[++index];
            validate=true;
        } else if (option == "--overview") overview=true;
        else if (option == "--range") range=true;
        else if (option == "--validate-yard") validate=true;
        else throw std::runtime_error("Usage: feel_lab [--map FILE | --range] [--capture PNG] [--overview] | --validate-map FILE | --validate-yard | --record-drills [CSV]");
    }
    feel_lab::ImportedMap map;
    if (map_path.empty() && !range && !validate && !map.load_embedded(argv[0])) {
        const auto automatic=std::filesystem::path(argv[0]).parent_path() / "mirage.map";
        if (std::filesystem::is_regular_file(automatic)) map_path=automatic;
    }
    if (!map_path.empty()) map.load(map_path);
    else if (!range && !map.loaded()) feel_lab::freight_yard::load(map);
    const char* map_name=map.original ? "Freight Yard" : map.loaded() ? "Mirage" : "Target Range";
    const Bombsite site_a=map.original ? Bombsite{{-19,0,-10},{-13,0,-4}} : mirage_a;
    const Bombsite site_b=map.original ? Bombsite{{13,0,-10},{19,0,-4}} : mirage_b;
    const Vector3 ct_spawn=map.original ? feel_lab::freight_yard::ct_spawn : mirage_ct;
    const float ct_yaw=map.original ? PI : -2.635447f;
    cfg.flat_ground=!map.loaded();
    MovementSimulator simulation(cfg);
    if (validate) {
        auto player=map.spawn_player();
        if (player.mode != MovementMode::Grounded) throw std::runtime_error("Spawn has no walkable floor");
        const auto spawn=player;
        bool jumped=false, landed=false;
        for (int tick=0; tick<256; ++tick) {
            map.advance(simulation,player,{.jump_pressed=tick==0});
            if (player.mode==MovementMode::Airborne) jumped=true;
            else if (jumped) landed=true;
        }
        if (!landed || std::abs(player.vertical_position-spawn.vertical_position)>0.01)
            throw std::runtime_error("Spawn jump did not return to floor");
        // Walk in the map's spawn-facing direction, then check that collision
        // kept the player on the map and that the spawn has room to move.
        for (int tick=0; tick<384; ++tick) {
            map.advance(simulation,player,{.forward=-std::cos(map.spawn_yaw),
                                          .right=std::sin(map.spawn_yaw)});
        }
        const double traveled=std::hypot(player.position.x-spawn.position.x,
                                         player.position.y-spawn.position.y);
        if (traveled<0.4 || player.vertical_position<map.lowest-1)
            throw std::runtime_error("Spawn movement failed");
        std::printf("Map validated. Jump landed; walked %.2f units from T spawn.\n",traveled);
        return 0;
    }
    feel_lab::Settings settings;
    settings.load();
    const auto& options=settings.preferences;
    const auto window_size=feel_lab::Preferences::sizes[static_cast<std::size_t>(options.window_size)];
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (options.msaa ? FLAG_MSAA_4X_HINT : 0u) |
                   (options.vsync ? FLAG_VSYNC_HINT : 0u) |
                   (capture_path.empty() ? 0u : FLAG_WINDOW_HIDDEN));
    InitWindow(window_size[0],window_size[1],"CS2Clone | Movement Feel Lab");
    SetExitKey(KEY_NULL);
    SetWindowMinSize(520,580);
    if (options.fullscreen && capture_path.empty()) options.apply_window();
    feel_lab::ui::load();
    using Audio = feel_lab::SoundCues;
    Audio sounds;
    if (capture_path.empty()) sounds.load();
    const auto play=[&](Audio::Cue cue,float level=1.0f,float pan=0.5f) {
        sounds.play(cue,level*static_cast<float>(options.effects_volume)/100.0f,pan);
    };
    int frame_cap=options.menu_fps;
    SetTargetFPS(frame_cap);
    EnableCursor();
    if (map.loaded() && !map.original) map.upload();
    MovementState state;
    state.position = {0, 0};
    const auto reset_position = [&map](MovementState& player) {
        player=map.loaded() ? map.spawn_player() : MovementState{};
    };
    reset_position(state);
    float yaw = map.loaded() ? map.spawn_yaw : 0.0f, pitch = 0.0f;
    int capture_frames = 0;
    RenderTexture2D render_target{};
    // Source uses a horizontal FOV at 4:3. Raylib takes a vertical FOV.
    const float camera_fov = 2.0f*std::atan(0.75f*std::tan(
        static_cast<float>(feel_lab::Preferences::base_camera_fov)*DEG2RAD*0.5f))*RAD2DEG;
    const auto target_positions = map.original ? feel_lab::freight_yard::targets :
        map.loaded() ? mirage_targets(map) : range_targets;
    std::array<bool,5> targets{};
    int ammunition = magazine_size;
    double next_shot = 0, reload_finish = 0;
    feel_lab::Gunplay practice_gunplay;
    int hits = 0, shots = 0;
    double total_time = 0.0, accumulator = 0.0;
    using feel_lab::Frontend;
    using feel_lab::GameMode;
    using feel_lab::BombRound;
    Frontend frontend;
    if (map.loaded()) frontend.selected=GameMode::OfflineMatch;
    BombRound round;
    feel_lab::OfflineMatch match;
    std::string score_text;
    Vector3 bomb = site_a.center();
    std::string result_text;
    bool round_won = false;
    bool pending_jump = false;
    std::array<bool, feel_lab::Settings::Count> wheel_pulses{};
    float last_hit_flash = 0.0f;
    double footstep_distance = 0;
    const char* drill_status = "Record nine movement drills";
    const auto start_round = [&] {
        const bool offline=frontend.selected==GameMode::OfflineMatch;
        if (offline) {
            const bool new_match=frontend.screen==Frontend::Screen::Lobby || match.finished();
            match.start(map,new_match,frontend.match_length);
        }
        round.start(offline ? GameMode::Practice : frontend.selected);
        frontend.screen = Frontend::Screen::Match;
        reset_position(state);
        yaw=map.loaded()?map.spawn_yaw:0;
        if (round.mode == GameMode::Defuse || (offline && !match.attacking())) {
            state=map.spawn_player(ct_spawn);
            yaw=ct_yaw;
        }
        bomb=site_a.center();
        pitch=0;
        hits=shots=0;
        total_time=accumulator=0;
        pending_jump=false;
        wheel_pulses.fill(false);
        targets.fill(false);
        ammunition=magazine_size;
        next_shot=reload_finish=0;
        practice_gunplay={};
        last_hit_flash=0;
        footstep_distance=0;
        if (offline) EnableCursor();
        else DisableCursor();
    };
    while (!WindowShouldClose()) {
        const float frame_dt = std::min(GetFrameTime(), 0.1f);
        const bool was_settings = settings.visible;
        const bool was_rebinding = settings.rebinding >= 0;
        const bool cursor_was_free = was_settings || frontend.menu() || (match.buying && frontend.selected==GameMode::OfflineMatch);
        if (IsKeyPressed(KEY_TAB)) {
            settings.visible = !settings.visible;
            settings.rebinding = -1;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            if (settings.rebinding >= 0) settings.rebinding = -1;
            else if (settings.visible) settings.visible = false;
            else if (frontend.screen == Frontend::Screen::Match) frontend.screen=Frontend::Screen::Pause;
            else if (frontend.screen == Frontend::Screen::Pause) frontend.screen=Frontend::Screen::Match;
            else if (frontend.screen == Frontend::Screen::Lobby) frontend.screen=Frontend::Screen::Home;
        }
        const bool cursor_is_free = settings.visible || frontend.menu() || (match.buying && frontend.selected==GameMode::OfflineMatch);
        if (cursor_is_free != cursor_was_free) {
            if (cursor_is_free) EnableCursor();
            else DisableCursor();
        }
        if (settings.visible && was_settings && !IsKeyPressed(KEY_ESCAPE)) settings.update();
        const bool accepting_input = !settings.visible && !was_settings && !frontend.menu();
        if (accepting_input && settings.pressed(feel_lab::Settings::Reset)) start_round();
        const bool offline=frontend.selected==GameMode::OfflineMatch;
        if (accepting_input && offline) {
            if (IsKeyPressed(KEY_B) && match.phase==feel_lab::OfflineMatch::Phase::Buy) {
                match.buying=!match.buying;
                if (match.buying) EnableCursor();
                else DisableCursor();
            }
            if (IsKeyPressed(KEY_ONE)) match.select(feel_lab::Weapon::Rifle);
            if (IsKeyPressed(KEY_TWO)) match.select(feel_lab::Weapon::Pistol);
            if (IsKeyPressed(KEY_THREE)) match.select(feel_lab::Weapon::Knife);
        }
        if (accepting_input && settings.pressed(feel_lab::Settings::Drills)) {
            drill_status = export_default_drills(simulation)
                ? "Saved to temp folder / CS2Clone / movement-drills.csv"
                : "CSV export failed. See the console for details.";
        }
        if (!accepting_input) {
            pending_jump = false;
            wheel_pulses.fill(false);
        }
        if (accepting_input) {
            const Vector2 mouse = offline && match.buying ? Vector2{} : GetMouseDelta();
            yaw += mouse.x * 0.0022f * settings.sensitivity;
            pitch = std::clamp(pitch - mouse.y * 0.0022f * settings.sensitivity * (options.invert_mouse ? -1.0f : 1.0f), -1.45f, 1.45f);
            const float sn = std::sin(yaw), cs = std::cos(yaw);
            const Vector3 look{sn*std::cos(pitch),std::sin(pitch),-cs*std::cos(pitch)};
            using Controls = feel_lab::Settings;
            pending_jump = pending_jump || settings.pressed(Controls::Jump);
            for (const auto action : {Controls::Forward, Controls::Back, Controls::Left,
                                      Controls::Right, Controls::Crouch, Controls::Walk}) {
                if (Controls::is_wheel(settings.keys[action]) && settings.pressed(action))
                    wheel_pulses[action] = true;
            }
            const auto held = [&](Controls::Action action) {
                return settings.down(action) || wheel_pulses[action];
            };
            accumulator = std::min(accumulator + frame_dt, 0.15);
            while (accumulator >= cfg.fixed_dt) {
                const float f = static_cast<float>(held(Controls::Forward)) -
                                static_cast<float>(held(Controls::Back));
                const float r = static_cast<float>(held(Controls::Right)) -
                                static_cast<float>(held(Controls::Left));
                const bool crouch = held(Controls::Crouch) ||
                    (settings.keys[Controls::Crouch] == KEY_LEFT_CONTROL &&
                     std::find(settings.keys.begin(), settings.keys.end(), KEY_C) == settings.keys.end() &&
                     IsKeyDown(KEY_C));
                MovementInput input;
                input.forward = static_cast<double>(-f * cs + r * sn);
                input.right = static_cast<double>(f * sn + r * cs);
                input.jump_pressed = pending_jump;
                input.crouch_held = crouch;
                input.walk_held = held(Controls::Walk);
                const Vector3 feet{static_cast<float>(state.position.x),
                                   static_cast<float>(state.vertical_position),
                                   static_cast<float>(state.position.y)};
                const bool grounded = state.mode == MovementMode::Grounded;
                const bool plant = round.mode == GameMode::Plant && round.phase == BombRound::Phase::Live &&
                                   (site_a.contains(feet) || site_b.contains(feet));
                const bool defuse = round.mode == GameMode::Defuse &&
                                    Vector3Distance(feet,bomb) <= 1.65f;
                const bool interacting = grounded && settings.down(Controls::Use) && (plant || defuse);
                const auto previous_phase = round.phase;
                if (offline) {
                    const auto previous=match.phase;
                    const auto eye=Vector3Add(feet,{0,feel_lab::ImportedMap::eye_height(state.crouch_fraction),0});
                    match.step(cfg.fixed_dt,map,feet,eye,
                               site_a.contains(feet)||site_b.contains(feet),grounded,settings.down(Controls::Use),look);
                    if (previous==feel_lab::OfflineMatch::Phase::Live && match.phase==feel_lab::OfflineMatch::Phase::Planted)
                        play(Audio::Click,0.8f);
                    if (previous==feel_lab::OfflineMatch::Phase::Planted && match.phase==feel_lab::OfflineMatch::Phase::Result) {
                        const bool detonated=match.time<=0 && std::string_view(match.result)=="Bomb detonated";
                        play(detonated?Audio::Detonation:Audio::Click,0.9f);
                    }
                    if (previous==feel_lab::OfflineMatch::Phase::Live || previous==feel_lab::OfflineMatch::Phase::Planted) {
                        for (const auto& bot : match.bots) {
                            if (!bot.fired) continue;
                            const auto offset=Vector3Subtract(bot.head(),eye);
                            const float distance=Vector3Length(offset);
                            const float side=Vector3DotProduct(Vector3Normalize(offset),{cs,0,sn});
                            play(Audio::Rifle,0.55f/(1.0f+0.09f*distance),0.5f-0.42f*side);
                        }
                    }
                    if (previous==feel_lab::OfflineMatch::Phase::Buy) {
                        input={};
                        if (!match.buying) DisableCursor();
                    }
                    if (match.interaction>0 && settings.down(Controls::Use)) input={};
                } else round.step(cfg.fixed_dt,interacting);
                if (plant && previous_phase != round.phase && round.phase == BombRound::Phase::Planted)
                    bomb=feet;
                if (interacting) {
                    input.forward=input.right=0;
                    input.jump_pressed=false;
                }
                const MovementState old = state;
                if (map.loaded()) {
                    if (!offline || match.alive()) map.advance(simulation,state,input);
                } else {
                    (void)simulation.step(state,input);
                    const float height=static_cast<float>(state.vertical_position);
                    if (intersects(static_cast<float>(state.position.x),static_cast<float>(old.position.y),height)) {
                        state.position.x=old.position.x; state.velocity.x=0;
                    }
                    if (intersects(static_cast<float>(state.position.x),static_cast<float>(state.position.y),height)) {
                        state.position.y=old.position.y; state.velocity.y=0;
                    }
                }
                if (map.loaded() && state.vertical_position < map.lowest-10) reset_position(state);
                const bool landed = !grounded && !old.on_ladder && state.mode==MovementMode::Grounded;
                if (landed) play(Audio::Step,0.7f);
                if (state.mode==MovementMode::Grounded && !input.walk_held && !input.crouch_held) {
                    footstep_distance+=std::hypot(state.position.x-old.position.x,state.position.y-old.position.y);
                    if (footstep_distance>=1.7) {
                        play(Audio::Step,0.48f);
                        footstep_distance=0;
                    }
                } else footstep_distance=0;
                if (offline) match.advance_gunplay(cfg.fixed_dt,landed);
                else practice_gunplay.advance(feel_lab::rifle_accuracy,shot_interval,cfg.fixed_dt,landed);
                accumulator -= cfg.fixed_dt;
                if (hits < static_cast<int>(targets.size())) total_time += cfg.fixed_dt;
                pending_jump = false;
                wheel_pulses.fill(false);
                if (offline && match.phase==feel_lab::OfflineMatch::Phase::Result) {
                    round_won=match.finished()?match.won_rounds>match.lost_rounds:match.won_round;
                    result_text=match.finished() ?
                        (match.won_rounds==match.lost_rounds?"Match drawn":round_won?"Match won":"Match lost") :
                        match.result;
                    score_text=TextFormat("YOU %d - %d BOTS   |   K %d   D %d",match.won_rounds,
                                          match.lost_rounds,match.kills,match.deaths);
                    frontend.screen=Frontend::Screen::Result;
                    EnableCursor();
                    break;
                }
                if (round.finished()) {
                    round_won=round.won();
                    result_text=round.phase==BombRound::Phase::Defused?"Bomb defused":
                                round.phase==BombRound::Phase::Expired?"Time expired before the plant":
                                "Bomb detonated";
                    frontend.screen=Frontend::Screen::Result;
                    EnableCursor();
                    break;
                }
            }
            const float eye = map.loaded() ? feel_lab::ImportedMap::eye_height(state.crouch_fraction) :
                standing_eye + (crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
            const Vector3 pos = {static_cast<float>(state.position.x), eye+static_cast<float>(state.vertical_position), static_cast<float>(state.position.y)};
            const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            if (offline) {
                if (!frontend.menu() && match.phase!=feel_lab::OfflineMatch::Phase::Buy) {
                    if (match.interaction==0) {
                        if (IsKeyPressed(KEY_FOUR)) match.throw_grenade(0,{pos,dir});
                        if (IsKeyPressed(KEY_FIVE)) match.throw_grenade(1,{pos,dir});
                    }
                    if (settings.pressed(Controls::Reload)) match.start_reload();
                    if ((settings.pressed(Controls::Fire) || settings.down(Controls::Fire)) &&
                        !(settings.down(Controls::Use) && match.interaction>0)) {
                        const bool ready=match.cooldown==0;
                        if (match.fire(map,{pos,dir},state)) last_hit_flash=0.18f;
                        if (ready && match.cooldown>0)
                            play(match.equipped==feel_lab::Weapon::Rifle?Audio::Rifle:
                                 match.equipped==feel_lab::Weapon::Pistol?Audio::Pistol:Audio::Click,0.78f);
                    }
                }
            } else if (reload_finish > 0 && total_time >= reload_finish) {
                ammunition = magazine_size;
                reload_finish = 0;
            }
            if (!offline && settings.pressed(Controls::Reload) && ammunition < magazine_size && reload_finish == 0)
                reload_finish = total_time + reload_seconds;
            if (!offline && (settings.pressed(Controls::Fire) || settings.down(Controls::Fire)) &&
                ammunition > 0 && reload_finish == 0 && total_time >= next_shot &&
                !frontend.menu() && round.progress == 0 &&
                (round.mode != GameMode::Practice || hits < static_cast<int>(targets.size()))) {
                next_shot = total_time + shot_interval;
                play(Audio::Rifle,0.78f);
                --ammunition;
                ++shots;
                const Ray ray=practice_gunplay.shoot({pos,dir},feel_lab::rifle_accuracy,state);
                float nearest=1000;
                if (map.loaded()) nearest = map.collision.shot_distance(ray,nearest);
                else ray_hits_wall(ray,nearest);
                std::size_t hit=targets.size();
                for (std::size_t i=0;i<targets.size();++i) {
                    if (offline || round.mode != GameMode::Practice || targets[i]) continue;
                    const RayCollision c=GetRayCollisionSphere(ray,target_positions[i],0.45f);
                    if(c.hit && c.distance < nearest) {nearest=c.distance;hit=i;}
                }
                if (hit<targets.size()) {
                    targets[hit]=true;
                    ++hits;
                    last_hit_flash=0.18f;
                    if (hits==static_cast<int>(targets.size())) {
                        round_won=true;
                        result_text=TextFormat("Five targets cleared in %.1f seconds",total_time);
                        frontend.screen=Frontend::Screen::Result;
                        EnableCursor();
                    }
                }
            }
        }
        last_hit_flash=std::max(0.0f,last_hit_flash-frame_dt);
        const float eye = map.loaded() ? feel_lab::ImportedMap::eye_height(state.crouch_fraction) :
            standing_eye + (crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
        const Vector3 pos = {static_cast<float>(state.position.x),eye+static_cast<float>(state.vertical_position),static_cast<float>(state.position.y)};
        const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
        Camera3D camera={pos,Vector3Add(pos,dir),{0,1,0},camera_fov,CAMERA_PERSPECTIVE};
        const bool front_page=frontend.screen==Frontend::Screen::Home || frontend.screen==Frontend::Screen::Lobby;
        if (map.original && front_page) {
            const float drift=std::sin(static_cast<float>(GetTime())*0.18f)*2.5f;
            camera={{12+drift,12,25},{-5,0,-8},{0,1,0},51,CAMERA_PERSPECTIVE};
        }
        if (overview && map.loaded()) camera={{0,map.original?55.0f:105.0f,0},{0,0,0},{0,0,-1},60,CAMERA_PERSPECTIVE};
        const auto resolution=options.render_size();
        const int screen_width=GetScreenWidth();
        const int screen_height=GetScreenHeight();
        const bool scale_scene=resolution.width!=screen_width || resolution.height!=screen_height;
        if (scale_scene && (render_target.texture.width!=resolution.width ||
                            render_target.texture.height!=resolution.height)) {
            if (render_target.id!=0) UnloadRenderTexture(render_target);
            render_target=LoadRenderTexture(resolution.width,resolution.height);
            if (!IsRenderTextureValid(render_target))
                throw std::runtime_error("Could not allocate the selected render resolution");
            SetTextureFilter(render_target.texture,TEXTURE_FILTER_BILINEAR);
        }
        if (scale_scene) BeginTextureMode(render_target);
        else BeginDrawing();
        ClearBackground({155,185,204,255});
        BeginMode3D(camera);
        if (map.original) feel_lab::freight_yard::draw();
        else if (map.loaded()) map.draw();
        else {
            DrawPlane({0,-0.01f,0},{30,30},{155,157,151,255});
            DrawGrid(30,1);
            for(const Wall& w:walls) {DrawCubeV(w.center,w.size,{93,104,117,255});DrawCubeWiresV(w.center,w.size,{50,55,63,255});}
        }
        for (std::size_t i=0;i<targets.size();++i) {
            if (front_page || offline || round.mode != GameMode::Practice || targets[i]) continue;
            DrawSphere(target_positions[i],0.45f,{230,85,55,255});
            DrawSphereWires(target_positions[i],0.45f,8,12,{80,35,20,255});
        }
        if (offline && frontend.screen!=Frontend::Screen::Home && frontend.screen!=Frontend::Screen::Lobby) {
            match.draw_bots();
            match.utility.draw();
            if (match.phase==feel_lab::OfflineMatch::Phase::Planted)
                DrawCube(Vector3Add(match.bomb,{0,0.09f,0}),0.35f,0.18f,0.24f,ORANGE);
        }
        if (round.mode != GameMode::Practice) {
            if (round.phase == BombRound::Phase::Live) {
                for (const auto site : {site_a,site_b}) {
                    const auto marker=site.center();
                    DrawCylinder(marker,0.7f,0.7f,0.04f,24,ORANGE);
                }
            } else {
                DrawCube(Vector3Add(bomb,{0,0.09f,0}),0.35f,0.18f,0.24f,ORANGE);
            }
        }
        EndMode3D();
        if (offline && accepting_input && match.alive() && !match.buying) {
            const float speed=static_cast<float>(std::hypot(state.velocity.x,state.velocity.y));
            const float sway=state.mode==MovementMode::Grounded ?
                std::sin(static_cast<float>(total_time)*12)*std::min(0.015f,speed*0.003f) : 0.0f;
            const float reload_pose=match.reload>0 ?
                std::sin(PI*static_cast<float>(match.reload/match.weapon().reload)) : 0.0f;
            draw_weapon(match.equipped,match.shot_kick,reload_pose,sway);
        }
        if (scale_scene) {
            EndTextureMode();
            BeginDrawing();
            ClearBackground(BLACK);
            const float scale=std::min(static_cast<float>(screen_width)/static_cast<float>(resolution.width),
                                       static_cast<float>(screen_height)/static_cast<float>(resolution.height));
            const float width=static_cast<float>(resolution.width)*scale;
            const float height=static_cast<float>(resolution.height)*scale;
            DrawTexturePro(render_target.texture,
                           {0,0,static_cast<float>(resolution.width),-static_cast<float>(resolution.height)},
                           {(static_cast<float>(screen_width)-width)/2,(static_cast<float>(screen_height)-height)/2,width,height},
                           {0,0},0,WHITE);
        }
        if (options.brightness!=100) {
            const auto alpha=static_cast<unsigned char>(std::abs(options.brightness-100)*2);
            const auto tone=static_cast<unsigned char>(options.brightness>100?255:0);
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{tone,tone,tone,alpha});
        }
        const int cx=GetScreenWidth()/2,cy=GetScreenHeight()/2;
        if (!frontend.menu() && !settings.visible) {
            if (offline && match.utility.smoke_seconds>0 &&
                Vector3Distance(pos,match.utility.smoke)<feel_lab::Utility::smoke_radius)
                DrawRectangle(0,0,screen_width,screen_height,{132,141,137,245});
            if (offline && match.utility.player_flash>0) {
                const float opacity=std::clamp(static_cast<float>(match.utility.player_flash/0.55),0.0f,1.0f);
                DrawRectangle(0,0,screen_width,screen_height,Fade(RAYWHITE,opacity));
            }
            if (!offline || match.alive()) options.draw_crosshair(cx,cy,last_hit_flash>0);
            const int margin=options.hud_margin;
            if (offline) {
                feel_lab::draw_match_hud(match,margin,screen_width,screen_height,map.original,pos,dir);
                if (options.movement_stats)
                    feel_lab::ui::label(TextFormat("%.0f u/s  |  %.1f mrad",
                        std::hypot(state.velocity.x,state.velocity.y)/MovementConfig::source_unit,
                        match.spread(state)*1000.0f),margin,margin+(map.original?132:0),15);
            } else {
                const int left=margin+12;
                feel_lab::ui::surface(margin,margin,400,145,{19,26,28,235});
                feel_lab::ui::label(map_name,left,margin+8,20);
                if (options.movement_stats)
                    feel_lab::ui::label(TextFormat("%.0f u/s  |  %.1f mrad  |  Crouch %.0f%%",
                        std::hypot(state.velocity.x,state.velocity.y)/MovementConfig::source_unit,
                        practice_gunplay.spread(feel_lab::rifle_accuracy,state)*1000.0f,state.crouch_fraction*100),
                        left,margin+36,16,feel_lab::ui::muted);
                const char* progress=round.mode==GameMode::Practice ?
                    TextFormat("Targets %d/5  |  Shots %d  |  %.1fs",hits,shots,total_time) :
                    TextFormat("%s  |  %.1f s",round.mode==GameMode::Defuse?"Defuse at A":
                        round.phase==BombRound::Phase::Live?"Plant at A or B":"Bomb armed",round.remaining);
                feel_lab::ui::label(progress,left,margin+61,18);
                feel_lab::ui::label(reload_finish>0?"Reloading":TextFormat("Ammo %d / %d",ammunition,magazine_size),
                                    left,margin+87,17);
                feel_lab::ui::label(TextFormat("%s: %s",feel_lab::Settings::key_label(feel_lab::Settings::Drills),
                                               drill_status),left,margin+111,15,feel_lab::ui::muted);
            }
            if (offline && !match.buying) {
                using Phase=feel_lab::OfflineMatch::Phase;
                if (match.alive()) {
                    const auto* key=feel_lab::Settings::key_label(settings.keys[feel_lab::Settings::Use]);
                    const bool planting=match.attacking() && match.phase==Phase::Live;
                    const bool defusing=!match.attacking() && match.phase==Phase::Planted;
                    const char* objective=match.phase==Phase::Buy?"B opens buy menu":
                        planting?TextFormat("Hold %s at A or B to plant",key):
                        defusing?TextFormat("Hold %s near the bomb to defuse",key):
                        match.phase==Phase::Planted?"Defend the bomb":"Defend A site";
                    const int label_y=screen_height-margin-93;
                    const int label_width=feel_lab::ui::label_width(objective,17);
                    feel_lab::ui::surface(cx-label_width/2-10,label_y-5,label_width+20,30,{19,26,28,225});
                    feel_lab::ui::label(objective,cx-label_width/2,label_y,17);
                    if (match.interaction>0 && (planting || defusing)) {
                        const double duration=planting?BombRound::plant_seconds:match.defuse_duration();
                        DrawRectangle(cx-80,label_y+28,160,4,feel_lab::ui::line);
                        DrawRectangle(cx-80,label_y+28,static_cast<int>(160*match.interaction/duration),4,feel_lab::ui::accent);
                    }
                }
            }
            if (round.mode != GameMode::Practice && !frontend.menu()) {
                const auto* action = feel_lab::Settings::key_label(settings.keys[feel_lab::Settings::Use]);
                const bool defusing=round.mode==GameMode::Defuse;
                if (round.progress > 0) {
                    const double duration=defusing?BombRound::defuse_seconds:BombRound::plant_seconds;
                    DrawRectangle(cx-130,cy+67,260,7,DARKGRAY);
                    DrawRectangle(cx-130,cy+67,static_cast<int>(260*round.progress/duration),7,ORANGE);
                }
                DrawText(TextFormat("Hold %s to %s",action,defusing?"defuse":"plant at a bombsite"),cx-130,cy+83,18,RAYWHITE);
            }
            if (options.show_fps) DrawFPS(GetScreenWidth()-88-margin,margin);
        }
        if (offline && accepting_input && match.buying) {
            const int left=cx-198, top=cy-166;
            feel_lab::ui::surface(cx-220,cy-188,440,376);
            feel_lab::ui::label(TextFormat("BUY   $%d   |   %.1f s",match.cash,match.time),left,top,24);
            if (feel_lab::ui::button(left,top+43,396,36,match.rifle?"Rifle  /  Owned":"Rifle  /  $2700"))
                match.purchase(0);
            if (feel_lab::ui::button(left,top+85,396,36,match.armor==100?"Armor  /  Full":"Armor  /  $650"))
                match.purchase(1);
            if (feel_lab::ui::button(left,top+127,194,36,match.utility.owned[0]?"Smoke  /  Owned":"Smoke  /  $300"))
                match.purchase_grenade(0);
            if (feel_lab::ui::button(left+202,top+127,194,36,match.utility.owned[1]?"Flash  /  Owned":"Flash  /  $200"))
                match.purchase_grenade(1);
            if (!match.attacking() && feel_lab::ui::button(left,top+169,396,36,match.kit?"Kit  /  Owned":"Defuse kit  /  $400"))
                match.purchase(2);
            feel_lab::ui::label(match.purchase_message,left,top+225,17,ORANGE);
            if (feel_lab::ui::button(left,top+274,396,42,"Close buy menu",false,true)) {
                match.buying=false;
                DisableCursor();
            }
        }
        const auto action=frontend.draw(settings,map.loaded(),round_won,result_text.c_str(),
            offline && !match.finished() ? "Next round" : offline ? "New match" : "Play again",
            offline ? score_text.c_str() : "",map_name,offline && match.finished());
        if (settings.visible) settings.draw(was_settings && !was_rebinding);
        EndDrawing();
        if (action==Frontend::Action::Exit) break;
        if (action==Frontend::Action::Start) start_round();
        if (action==Frontend::Action::Resume) {
            frontend.screen=Frontend::Screen::Match;
            if (!(offline && match.buying)) DisableCursor();
        }
        const int desired_cap=frontend.menu() || settings.visible ? options.menu_fps : options.match_fps;
        if (desired_cap!=frame_cap) {
            frame_cap=desired_cap;
            SetTargetFPS(frame_cap);
        }
        if (!capture_path.empty() && ++capture_frames == 3) {
            Image frame=LoadImageFromScreen();
            const bool saved=ExportImage(frame,capture_path.string().c_str());
            UnloadImage(frame);
            if (!saved) {
                if (render_target.id!=0) UnloadRenderTexture(render_target);
                map.unload();
                feel_lab::ui::unload();
                CloseWindow();
                return 1;
            }
            break;
        }
    }
    if (render_target.id!=0) UnloadRenderTexture(render_target);
    map.unload();
    feel_lab::ui::unload();
    CloseWindow();
    return 0;
}


int main(int argc, char* argv[]) {
    try {
        return run_lab(argc,argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr,"Movement lab: %s\n",error.what());
        return 1;
    }
}

#include "drills.hpp"
#include "raylib.h"
#include "raymath.h"
#include "frontend.hpp"
#include "imported_map.hpp"
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
constexpr Bombsite site_a{{-54.01f,-4.176f,-14.30f},{-45.75f,-3.619f,-6.124f}};
constexpr Bombsite site_b{{1.67f,-3.689f,-51.42f},{10.21f,-3.317f,-43.61f}};
constexpr Vector3 ct_spawn{-45.8432f,-5.548f,-41.2032f};
constexpr float ct_yaw = -2.635447f;

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
    bool validate = false, overview = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if ((option == "--map" || option == "--capture") && index+1 < argc) {
            if (option == "--map") map_path=argv[++index];
            else capture_path=argv[++index];
        } else if (option == "--validate-map" && index+1 < argc) {
            map_path=argv[++index];
            validate=true;
        } else if (option == "--overview") overview=true;
        else throw std::runtime_error("Usage: feel_lab [--map FILE] [--capture PNG] [--overview] | --validate-map FILE | --record-drills [CSV]");
    }
    feel_lab::ImportedMap map;
    if (map_path.empty() && !map.load_embedded(argv[0])) {
        const auto automatic=std::filesystem::path(argv[0]).parent_path() / "mirage.map";
        if (std::filesystem::is_regular_file(automatic)) map_path=automatic;
    }
    if (!map_path.empty()) map.load(map_path);
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
    int frame_cap=options.menu_fps;
    SetTargetFPS(frame_cap);
    EnableCursor();
    if (map.loaded()) map.upload();
    MovementState state;
    state.position = {0, 0};
    const auto reset_position = [&map](MovementState& player) {
        player=map.loaded() ? map.spawn_player() : MovementState{};
    };
    reset_position(state);
    float yaw = map.loaded() ? map.spawn_yaw : 0.0f, pitch = 0.0f;
    int capture_frames = 0;
    const auto target_positions = map.loaded() ? mirage_targets(map) : range_targets;
    std::array<bool,5> targets{};
    int ammunition = magazine_size;
    double next_shot = 0, reload_finish = 0;
    int hits = 0, shots = 0;
    double total_time = 0.0, accumulator = 0.0;
    using feel_lab::Frontend;
    using feel_lab::GameMode;
    using feel_lab::BombRound;
    Frontend frontend;
    BombRound round;
    Vector3 bomb = site_a.center();
    std::string result_text;
    bool round_won = false;
    bool pending_jump = false;
    std::array<bool, feel_lab::Settings::Count> wheel_pulses{};
    float last_hit_flash = 0.0f;
    const char* drill_status = "Record nine movement drills";
    const auto start_round = [&] {
        round.start(frontend.selected);
        frontend.screen = Frontend::Screen::Match;
        reset_position(state);
        yaw=map.loaded()?map.spawn_yaw:0;
        if (round.mode == GameMode::Defuse) {
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
        last_hit_flash=0;
        DisableCursor();
    };
    while (!WindowShouldClose()) {
        const float frame_dt = std::min(GetFrameTime(), 0.1f);
        const bool was_settings = settings.visible;
        const bool was_rebinding = settings.rebinding >= 0;
        const bool cursor_was_free = was_settings || frontend.menu();
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
        const bool cursor_is_free = settings.visible || frontend.menu();
        if (cursor_is_free != cursor_was_free) {
            if (cursor_is_free) EnableCursor();
            else DisableCursor();
        }
        if (settings.visible && was_settings && !IsKeyPressed(KEY_ESCAPE)) settings.update();
        const bool accepting_input = !settings.visible && !was_settings && !frontend.menu();
        if (accepting_input && settings.pressed(feel_lab::Settings::Reset)) start_round();
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
            const Vector2 mouse = GetMouseDelta();
            yaw += mouse.x * 0.0022f * settings.sensitivity;
            pitch = std::clamp(pitch - mouse.y * 0.0022f * settings.sensitivity * (options.invert_mouse ? -1.0f : 1.0f), -1.45f, 1.45f);
            const float sn = std::sin(yaw), cs = std::cos(yaw);
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
                round.step(cfg.fixed_dt,interacting);
                if (plant && previous_phase != round.phase && round.phase == BombRound::Phase::Planted)
                    bomb=feet;
                if (interacting) {
                    input.forward=input.right=0;
                    input.jump_pressed=false;
                }
                const MovementState old = state;
                if (map.loaded()) {
                    map.advance(simulation,state,input);
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
                accumulator -= cfg.fixed_dt;
                if (hits < static_cast<int>(targets.size())) total_time += cfg.fixed_dt;
                pending_jump = false;
                wheel_pulses.fill(false);
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
            const float eye = map.loaded() ? (64.0f-18.0f*static_cast<float>(state.crouch_fraction))*static_cast<float>(MovementConfig::source_unit) : standing_eye + (crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
            const Vector3 pos = {static_cast<float>(state.position.x), eye+static_cast<float>(state.vertical_position), static_cast<float>(state.position.y)};
            const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            if (reload_finish > 0 && total_time >= reload_finish) {
                ammunition = magazine_size;
                reload_finish = 0;
            }
            if (settings.pressed(Controls::Reload) && ammunition < magazine_size && reload_finish == 0)
                reload_finish = total_time + reload_seconds;
            if ((settings.pressed(Controls::Fire) || settings.down(Controls::Fire)) &&
                ammunition > 0 && reload_finish == 0 && total_time >= next_shot &&
                !frontend.menu() && round.progress == 0 &&
                (round.mode != GameMode::Practice || hits < static_cast<int>(targets.size()))) {
                next_shot = total_time + shot_interval;
                --ammunition;
                ++shots;
                const Ray ray={pos,dir};
                float nearest=1000;
                if (map.loaded()) nearest = map.collision.shot_distance(ray,nearest);
                else ray_hits_wall(ray,nearest);
                std::size_t hit=targets.size();
                for (std::size_t i=0;i<targets.size();++i) {
                    if (round.mode != GameMode::Practice || targets[i]) continue;
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
        const float eye = map.loaded() ? (64.0f-18.0f*static_cast<float>(state.crouch_fraction))*static_cast<float>(MovementConfig::source_unit) : standing_eye+(crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
        const Vector3 pos = {static_cast<float>(state.position.x),eye+static_cast<float>(state.vertical_position),static_cast<float>(state.position.y)};
        const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
        Camera3D camera={pos,Vector3Add(pos,dir),{0,1,0},static_cast<float>(options.fov),CAMERA_PERSPECTIVE};
        if (overview && map.loaded()) camera={{0,105,0},{0,0,0},{0,0,-1},65,CAMERA_PERSPECTIVE};
        BeginDrawing();
        ClearBackground({155,185,204,255});
        BeginMode3D(camera);
        if (map.loaded()) map.draw();
        else {
            DrawPlane({0,-0.01f,0},{30,30},{155,157,151,255});
            DrawGrid(30,1);
            for(const Wall& w:walls) {DrawCubeV(w.center,w.size,{93,104,117,255});DrawCubeWiresV(w.center,w.size,{50,55,63,255});}
        }
        for (std::size_t i=0;i<targets.size();++i) {
            if (round.mode != GameMode::Practice || targets[i]) continue;
            DrawSphere(target_positions[i],0.45f,{230,85,55,255});
            DrawSphereWires(target_positions[i],0.45f,8,12,{80,35,20,255});
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
        if (options.brightness!=100) {
            const auto alpha=static_cast<unsigned char>(std::abs(options.brightness-100)*2);
            const auto tone=static_cast<unsigned char>(options.brightness>100?255:0);
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{tone,tone,tone,alpha});
        }
        const int cx=GetScreenWidth()/2,cy=GetScreenHeight()/2;
        if (!frontend.menu() && !settings.visible) {
            options.draw_crosshair(cx,cy,last_hit_flash>0);
            const int margin=options.hud_margin;
            const int offset=margin-12;
            DrawRectangle(margin,margin,480,169,{12,18,26,190});
            DrawText(map.loaded()?"MIRAGE  |  128 Hz movement lab":"MOVEMENT FEEL LAB  |  128 Hz simulation",
                     24+offset,20+offset,19,RAYWHITE);
            if (options.movement_stats) DrawText(TextFormat("Speed: %.2f  |  Crouch: %.0f%%  |  %s",
                std::hypot(state.velocity.x,state.velocity.y),state.crouch_fraction*100,
                state.on_ladder?"LADDER":state.mode==MovementMode::Grounded?"GROUND":"AIR"),24+offset,51+offset,18,RAYWHITE);
            if (round.mode == GameMode::Practice) {
                DrawText(TextFormat("Targets: %d/5  |  Shots: %d  |  Time: %.1fs",hits,shots,total_time),24+offset,78+offset,18,RAYWHITE);
            } else {
                const char* objective = round.mode==GameMode::Defuse ? "Defuse at A" :
                    round.phase==BombRound::Phase::Live ? "Plant at A or B" : "Bomb armed";
                DrawText(TextFormat("%s  |  %.1f s",objective,round.remaining),24+offset,78+offset,18,RAYWHITE);
            }
            DrawText(reload_finish > 0 ? "Reloading..." : TextFormat("Ammo: %d/%d  |  Tab settings",ammunition,magazine_size),
                     24+offset,106+offset,16,RAYWHITE);
            DrawText(TextFormat("%s reload  |  %s reset",feel_lab::Settings::key_label(settings.keys[feel_lab::Settings::Reload]),
                                feel_lab::Settings::key_label(settings.keys[feel_lab::Settings::Reset])),24+offset,127+offset,16,RAYWHITE);
            DrawText(TextFormat("%s: %s", feel_lab::Settings::key_label(settings.keys[feel_lab::Settings::Drills]), drill_status),24+offset,151+offset,16,RAYWHITE);
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
        const auto action=frontend.draw(settings,map.loaded(),round_won,result_text.c_str());
        if (settings.visible) settings.draw(was_settings && !was_rebinding);
        EndDrawing();
        if (action==Frontend::Action::Exit) break;
        if (action==Frontend::Action::Start) start_round();
        if (action==Frontend::Action::Resume) {
            frontend.screen=Frontend::Screen::Match;
            DisableCursor();
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
                map.unload();
                feel_lab::ui::unload();
                CloseWindow();
                return 1;
            }
            break;
        }
    }
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

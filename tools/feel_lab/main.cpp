#include "game/simulation/movement.hpp"
#include "raylib.h"
#include "raymath.h"
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
constexpr Vector3 target_positions[] = {{0,1.5f,-10},{-10,1.5f,-8},{9,1.5f,-8},{9,1.5f,10},{-10,1.5f,10}};
constexpr float player_radius = 0.28f;
constexpr float standing_eye = 1.65f;
constexpr float crouched_eye = 1.05f;
constexpr float pi = 3.14159265358979323846f;
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

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "CS2Clone | Movement Feel Lab");
    SetTargetFPS(240);
    DisableCursor();
    MovementConfig cfg;
    MovementSimulator simulation(cfg);
    MovementState state;
    state.position = {0, 0};
    float yaw = 0.0f, pitch = 0.0f;
    bool targets[5] = {};
    int hits = 0, shots = 0;
    double total_time = 0.0, accumulator = 0.0;
    bool paused = false, prev_jump = false;
    float last_hit_flash = 0.0f;
    while (!WindowShouldClose()) {
        const float frame_dt = std::min(GetFrameTime(), 0.1f);
        if (IsKeyPressed(KEY_ESCAPE)) {paused = !paused; if(paused) EnableCursor(); else DisableCursor();}
        if (IsKeyPressed(KEY_R)) {state = {}; yaw=0; pitch=0; hits=0; shots=0; total_time=0; for(bool& t:targets)t=false;}
        if (!paused) {
            const Vector2 mouse = GetMouseDelta();
            yaw += mouse.x * 0.0022f;
            pitch = std::clamp(pitch - mouse.y * 0.0022f, -1.45f, 1.45f);
            const float sn = std::sin(yaw), cs = std::cos(yaw);
            const float f = static_cast<float>(IsKeyDown(KEY_W)) - static_cast<float>(IsKeyDown(KEY_S));
            const float r = static_cast<float>(IsKeyDown(KEY_D)) - static_cast<float>(IsKeyDown(KEY_A));
            const bool jump = IsKeyDown(KEY_SPACE);
            const bool crouch = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C);
            accumulator = std::min(accumulator + frame_dt, 0.15);
            while (accumulator >= cfg.fixed_dt) {
                MovementInput input;
                input.forward = static_cast<double>(f * cs + r * sn);
                input.right = static_cast<double>(f * sn - r * cs);
                input.jump_pressed = jump && !prev_jump;
                input.crouch_held = crouch;
                const MovementState old = state;
                (void)simulation.step(state, input);
                const float height = static_cast<float>(state.vertical_position);
                if (intersects(static_cast<float>(state.position.x), static_cast<float>(old.position.y), height)) {
                    state.position.x=old.position.x; state.velocity.x=0;
                }
                if (intersects(static_cast<float>(state.position.x), static_cast<float>(state.position.y), height)) {
                    state.position.y=old.position.y; state.velocity.y=0;
                }
                accumulator -= cfg.fixed_dt;
                total_time += cfg.fixed_dt;
                prev_jump = jump;
            }
            const float eye = standing_eye + (crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
            const Vector3 pos = {static_cast<float>(state.position.x), eye+static_cast<float>(state.vertical_position), static_cast<float>(state.position.y)};
            const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ++shots;
                const Ray ray={pos,dir};
                float nearest=1000;
                ray_hits_wall(ray,nearest);
                int hit=-1;
                for (int i=0;i<5;i++) {
                    if (targets[i]) continue;
                    const RayCollision c=GetRayCollisionSphere(ray,target_positions[i],0.45f);
                    if(c.hit && c.distance < nearest) {nearest=c.distance;hit=i;}
                }
                if(hit>=0) {targets[hit]=true;++hits;last_hit_flash=0.18f;}
            }
        }
        last_hit_flash=std::max(0.0f,last_hit_flash-frame_dt);
        const float eye = standing_eye+(crouched_eye-standing_eye)*static_cast<float>(state.crouch_fraction);
        const Vector3 pos = {static_cast<float>(state.position.x),eye+static_cast<float>(state.vertical_position),static_cast<float>(state.position.y)};
        const Vector3 dir = {std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
        const Camera3D camera={pos,Vector3Add(pos,dir),{0,1,0},80,CAMERA_PERSPECTIVE};
        BeginDrawing();
        ClearBackground({155,185,204,255});
        BeginMode3D(camera);
        DrawPlane({0,-0.01f,0},{30,30},{155,157,151,255});
        DrawGrid(30,1);
        for(const Wall& w:walls) {DrawCubeV(w.center,w.size,{93,104,117,255});DrawCubeWiresV(w.center,w.size,{50,55,63,255});}
        for(int i=0;i<5;i++) if(!targets[i]) {DrawSphere(target_positions[i],0.45f,{230,85,55,255});DrawSphereWires(target_positions[i],0.45f,8,12,{80,35,20,255});}
        EndMode3D();
        const int cx=GetScreenWidth()/2,cy=GetScreenHeight()/2;
        const Color cross=last_hit_flash>0?GREEN:WHITE;
        DrawLine(cx-11,cy,cx-4,cy,cross);DrawLine(cx+4,cy,cx+11,cy,cross);
        DrawLine(cx,cy-11,cx,cy-4,cross);DrawLine(cx,cy+4,cx,cy+11,cross);
        DrawRectangle(12,12,480,142,{12,18,26,190});
        DrawText("MOVEMENT FEEL LAB  |  128 Hz simulation",24,20,19,RAYWHITE);
        DrawText(TextFormat("Speed: %.2f  |  Crouch: %.0f%%  |  %s",
            std::hypot(state.velocity.x,state.velocity.y),state.crouch_fraction*100,
            state.mode==MovementMode::Grounded?"GROUND":"AIR"),24,51,18,RAYWHITE);
        DrawText(TextFormat("Targets: %d/5  |  Shots: %d  |  Time: %.1fs",hits,shots,total_time),24,78,18,RAYWHITE);
        DrawText("WASD move  |  Mouse aim  |  LMB fire  |  Space jump",24,106,16,RAYWHITE);
        DrawText("Ctrl/C crouch  |  R reset  |  Esc release mouse",24,127,16,RAYWHITE);
        if(hits==5) {DrawRectangle(cx-225,cy-100,450,50,{18,30,23,220});DrawText("ALL TARGETS HIT - R TO RESET",cx-194,cy-86,23,GREEN);}
        if(paused) DrawText("PAUSED - ESC TO RESUME",cx-160,cy+75,23,YELLOW);
        DrawFPS(GetScreenWidth()-100,20);
        EndDrawing();
    }
    CloseWindow();
}

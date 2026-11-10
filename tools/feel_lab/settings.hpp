#pragma once

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>

namespace feel_lab {

struct Settings {
    enum Action { Forward, Back, Left, Right, Jump, Crouch, Reset, Drills, Walk, Fire, Reload, Use, Count };
    static constexpr std::array<const char*, Count> names{
        "Forward", "Back", "Left", "Right", "Jump", "Crouch", "Reset", "Record drills", "Walk", "Fire", "Reload", "Use / C4"};
    static constexpr int mouse_base = -100;
    static constexpr int wheel_up = -201;
    static constexpr int wheel_down = -202;
    static constexpr std::array<int, Count> defaults{
        KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE, KEY_LEFT_CONTROL, KEY_R, KEY_F5, KEY_LEFT_SHIFT, mouse_base - MOUSE_BUTTON_LEFT, KEY_E, KEY_F};

    std::array<int, Count> keys = defaults;
    float sensitivity = 1.0f;
    int rebinding = -1;
    bool visible = false;

    void load() {
        std::ifstream file("feel-lab-settings.txt");
        float value = 0;
        auto saved = defaults;
        if (!(file >> value) || !std::isfinite(value) || value < 0.1f || value > 5.0f) return;
        for (std::size_t index = 0; index < saved.size(); ++index) {
            int key = 0;
            if (!(file >> key)) {
                if ((index == Walk || index == Fire || index == Reload || index == Use) && file.eof()) {
                    for (std::size_t added = index; added < saved.size(); ++added) {
                        const auto assigned_end = saved.begin()+static_cast<std::ptrdiff_t>(added);
                        if (std::find(saved.begin(),assigned_end,saved[added]) == assigned_end) continue;
                        for (int candidate = KEY_E; candidate <= KEY_Z; ++candidate) {
                            if (std::find(saved.begin(),assigned_end,candidate) == assigned_end) {
                                saved[added]=candidate;
                                break;
                            }
                        }
                    }
                    break;
                }
                return;
            }
            if (!valid_binding(key)) return;
            saved[index] = key;
        }
        for (auto it = saved.begin(); it != saved.end(); ++it) {
            if (std::find(saved.begin(), it, *it) != it) return;
        }
        sensitivity = value;
        keys = saved;
    }

    void save() const {
        std::ofstream file("feel-lab-settings.txt");
        file << sensitivity;
        for (const auto key : keys) file << ' ' << key;
        file << '\n';
    }

    static Rectangle control(float x, float y, float width) {
        return {x, y, width, 28};
    }

    static void draw_button(Rectangle bounds, const char* label) {
        DrawRectangleRec(bounds, CheckCollisionPointRec(GetMousePosition(), bounds)
            ? Color{64, 91, 123, 255} : Color{42, 59, 78, 255});
        DrawText(label, static_cast<int>(bounds.x) + 8, static_cast<int>(bounds.y) + 5, 17, RAYWHITE);
    }

    static bool is_mouse_button(int binding) {
        return binding <= mouse_base - MOUSE_BUTTON_LEFT &&
               binding >= mouse_base - MOUSE_BUTTON_BACK;
    }

    static bool is_wheel(int binding) {
        return binding == wheel_up || binding == wheel_down;
    }

    static bool valid_binding(int binding) {
        return is_mouse_button(binding) || is_wheel(binding) ||
               (binding >= 32 && binding <= KEY_KB_MENU &&
                binding != KEY_ESCAPE && binding != KEY_TAB);
    }

    bool down(Action action) const {
        const int binding = keys[action];
        if (is_wheel(binding)) return false;
        return is_mouse_button(binding) ? IsMouseButtonDown(mouse_base - binding) : IsKeyDown(binding);
    }

    bool pressed(Action action) const {
        const int binding = keys[action];
        if (binding == wheel_up) return GetMouseWheelMove() > 0;
        if (binding == wheel_down) return GetMouseWheelMove() < 0;
        return is_mouse_button(binding) ? IsMouseButtonPressed(mouse_base - binding) : IsKeyPressed(binding);
    }

    void update() {
        if (rebinding >= 0) {
            int key = GetKeyPressed();
            const float wheel = GetMouseWheelMove();
            if (wheel != 0) key = wheel > 0 ? wheel_up : wheel_down;
            for (int button = MOUSE_BUTTON_LEFT; key == 0 && button <= MOUSE_BUTTON_BACK; ++button) {
                if (IsMouseButtonPressed(button)) key = mouse_base - button;
            }
            if (valid_binding(key)) {
                auto& current = keys[static_cast<std::size_t>(rebinding)];
                const auto existing = std::find(keys.begin(), keys.end(), key);
                if (existing != keys.end()) std::swap(*existing, current);
                else current = key;
                rebinding = -1;
                save();
            }
            return;
        }

        if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
        const auto clicked = [](Rectangle bounds) {
            return CheckCollisionPointRec(GetMousePosition(), bounds);
        };
        if (clicked(control(230, 69, 42))) {
            sensitivity = std::max(0.1f, sensitivity - 0.1f);
            save();
        } else if (clicked(control(279, 69, 42))) {
            sensitivity = std::min(5.0f, sensitivity + 0.1f);
            save();
        } else if (clicked(control(340, 69, 148))) {
            ToggleFullscreen();
        } else {
            for (int index = 0; index < Count; ++index) {
                if (clicked(control(230, 112.0f + 34.0f * static_cast<float>(index), 258))) {
                    while (GetKeyPressed() != 0) {}
                    rebinding = index;
                    break;
                }
            }
        }
    }

    static const char* key_label(int key) {
        if ((key >= KEY_A && key <= KEY_Z) || (key >= KEY_ZERO && key <= KEY_NINE))
            return TextFormat("%c", key);
        if (key >= KEY_F1 && key <= KEY_F12) return TextFormat("F%d", key - KEY_F1 + 1);
        switch (key) {
        case wheel_up: return "Wheel Up";
        case wheel_down: return "Wheel Down";
        case mouse_base - MOUSE_BUTTON_LEFT: return "Mouse Left";
        case mouse_base - MOUSE_BUTTON_RIGHT: return "Mouse Right";
        case mouse_base - MOUSE_BUTTON_MIDDLE: return "Mouse Middle";
        case mouse_base - MOUSE_BUTTON_SIDE: return "Mouse Side";
        case mouse_base - MOUSE_BUTTON_EXTRA: return "Mouse Extra";
        case mouse_base - MOUSE_BUTTON_FORWARD: return "Mouse Forward";
        case mouse_base - MOUSE_BUTTON_BACK: return "Mouse Back";
        case KEY_SPACE: return "Space";
        case KEY_LEFT_CONTROL: return "Left Ctrl";
        case KEY_RIGHT_CONTROL: return "Right Ctrl";
        case KEY_LEFT_SHIFT: return "Left Shift";
        case KEY_RIGHT_SHIFT: return "Right Shift";
        case KEY_LEFT: return "Left arrow";
        case KEY_RIGHT: return "Right arrow";
        case KEY_UP: return "Up arrow";
        case KEY_DOWN: return "Down arrow";
        default: return TextFormat("Key %d", key);
        }
    }

    void draw() const {
        DrawRectangle(16, 16, 492, 548, {12, 18, 26, 248});
        DrawText("SETTINGS", 32, 29, 22, RAYWHITE);
        DrawText("Tab / Esc to close", 335, 32, 16, LIGHTGRAY);
        DrawText(TextFormat("Sensitivity: %.1fx", static_cast<double>(sensitivity)), 32, 75, 17, RAYWHITE);
        draw_button(control(230, 69, 42), "-");
        draw_button(control(279, 69, 42), "+");
        draw_button(control(340, 69, 148), IsWindowFullscreen() ? "Windowed" : "Fullscreen");
        for (int index = 0; index < Count; ++index) {
            const float y = 112.0f + 34.0f * static_cast<float>(index);
            const auto offset = static_cast<std::size_t>(index);
            DrawText(names[offset], 32, static_cast<int>(y) + 5, 17, RAYWHITE);
            draw_button(control(230, y, 258), index == rebinding ? "Press key, button, or scroll" : key_label(keys[offset]));
        }
        DrawText("Click a binding to change it. Existing bindings swap.", 32, 531, 15, LIGHTGRAY);
    }
};

} // namespace feel_lab

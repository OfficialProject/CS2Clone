#pragma once

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>

namespace feel_lab {

struct Settings {
    enum Action { Forward, Back, Left, Right, Jump, Crouch, Reset, Drills, Walk, Count };
    static constexpr std::array<const char*, Count> names{
        "Forward", "Back", "Left", "Right", "Jump", "Crouch", "Reset", "Record drills", "Walk"};
    static constexpr std::array<int, Count> defaults{
        KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE, KEY_LEFT_CONTROL, KEY_R, KEY_F5, KEY_LEFT_SHIFT};

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
                if (index == Walk && file.eof()) break;
                return;
            }
            if (key < 32 || key > KEY_KB_MENU || key == KEY_ESCAPE || key == KEY_TAB) return;
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

    void update() {
        if (rebinding >= 0) {
            const int key = GetKeyPressed();
            if (key != 0 && key != KEY_ESCAPE && key != KEY_TAB) {
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
        DrawRectangle(16, 16, 492, 446, {12, 18, 26, 248});
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
            draw_button(control(230, y, 258), index == rebinding ? "Press a key (Esc cancels)" : key_label(keys[offset]));
        }
        DrawText("Click a binding to change it. Existing bindings swap.", 32, 429, 15, LIGHTGRAY);
    }
};

} // namespace feel_lab

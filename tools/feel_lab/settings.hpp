#pragma once

#include "ui.hpp"

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
    enum class Category { Display, Mouse, Bindings };
    Category category = Category::Display;

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

    void draw(bool interactive) {
        const int width=GetScreenWidth();
        const int height=GetScreenHeight();
        DrawRectangle(0,0,width,height,{9,15,21,242});
        ui::bar();
        ui::label("SETTINGS",169,21,21,ui::accent);
        if (ui::button(width-105,13,89,38,"Close") && interactive) {
            visible=false;
            rebinding=-1;
        }

        const int panel_width=std::min(width-36,930);
        const int left=(width-panel_width)/2;
        const int tab_width=(panel_width-16)/3;
        const auto tab = [&](int column, Category item, const char* label) {
            if (ui::button(left+column*(tab_width+8),88,tab_width,40,label,category==item) && interactive) {
                category=item;
                rebinding=-1;
            }
        };
        tab(0,Category::Display,"Display");
        tab(1,Category::Mouse,"Mouse");
        tab(2,Category::Bindings,"Bindings");
        DrawRectangle(left,146,panel_width,height-164,ui::panel);

        if (category==Category::Display) {
            ui::label("Screen mode",left+20,174,20);
            if (ui::button(left+panel_width-176,161,156,39,
                           IsWindowFullscreen()?"Fullscreen":"Windowed") && interactive) ToggleFullscreen();
        } else if (category==Category::Mouse) {
            ui::label("Mouse sensitivity",left+20,174,20);
            const int slider_x=left+panel_width/2;
            const int slider_width=panel_width/2-88;
            const Rectangle slider{static_cast<float>(slider_x),165,static_cast<float>(slider_width),39};
            if (interactive && ui::hovered(slider) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const float ratio=std::clamp((GetMousePosition().x-slider.x)/slider.width,0.0f,1.0f);
                const float value=std::round((0.1f+4.9f*ratio)*10)/10;
                if (value!=sensitivity) {
                    sensitivity=value;
                    save();
                }
            }
            DrawRectangle(slider_x,183,slider_width,4,ui::line);
            const int knob=slider_x+static_cast<int>((sensitivity-0.1f)/4.9f*static_cast<float>(slider_width));
            DrawCircle(knob,185,7,ui::accent);
            ui::label(TextFormat("%.1fx",static_cast<double>(sensitivity)),left+panel_width-64,173,18);
        } else {
            const bool two_columns=panel_width>=740;
            const int rows=two_columns?(Count+1)/2:Count;
            const int column_width=two_columns?(panel_width-24)/2:panel_width-24;
            const int row_height=std::min(43,(height-192)/rows);
            for (int index=0; index<Count; ++index) {
                const int x=left+12+column_width*(index/rows);
                const int y=158+(index%rows)*row_height;
                const auto offset=static_cast<std::size_t>(index);
                ui::label(names[offset],x+8,y+5,17);
                const int binding_width=std::min(225,column_width/2);
                const auto* label=index==rebinding?"Press input...":key_label(keys[offset]);
                if (ui::button(x+column_width-binding_width-8,y,binding_width,row_height-3,label,
                               index==rebinding) && interactive && rebinding<0) {
                    while (GetKeyPressed()!=0) {}
                    rebinding=index;
                }
                DrawRectangle(x+8,y+row_height-1,column_width-16,1,ui::line);
            }
            ui::label(rebinding>=0?"Press a key, mouse button or scroll. Esc cancels.":"Click a binding to change it.",
                      left+20,height-46,15,ui::muted);
        }
    }
};

} // namespace feel_lab

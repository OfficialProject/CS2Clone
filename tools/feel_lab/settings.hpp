#pragma once

#include "preferences.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>

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
    Preferences preferences;
    int rebinding = -1;
    bool visible = false;
    enum class Category { Display, Advanced, Audio, Hud, Crosshair, Mouse, Bindings };
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
        std::string version;
        if (file >> version && (version=="preferences-v1" || version=="preferences-v2" || version=="preferences-v3"))
            preferences.read(file,version!="preferences-v1",version=="preferences-v3");
    }

    void save() const {
        std::ofstream file("feel-lab-settings.txt");
        file << sensitivity;
        for (const auto key : keys) file << ' ' << key;
        file << '\n' << "preferences-v3 ";
        preferences.write(file);
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
        DrawRectangle(0,0,width,height,{12,20,23,222});
        ui::bar();
        ui::label("SETTINGS",169,21,21,ui::accent);
        if (ui::button(width-105,13,89,38,"Close") && interactive) {
            visible=false;
            rebinding=-1;
        }

        if (ui::button(width-255,13,138,38,"Reset defaults") && interactive) {
            keys=defaults;
            sensitivity=1;
            preferences=Preferences{};
            preferences.apply_window();
            rebinding=-1;
            save();
        }

        const int panel_width=std::min(width-36,930);
        const int left=(width-panel_width)/2;
        constexpr int tab_count=7;
        const int tab_width=(panel_width-5*5)/tab_count;
        const auto tab = [&](int column, Category item, const char* label) {
            if (ui::button(left+column*(tab_width+5),88,tab_width,40,label,category==item) && interactive) {
                category=item;
                rebinding=-1;
            }
        };
        tab(0,Category::Display,"Video");
        tab(1,Category::Advanced,"Quality");
        tab(2,Category::Audio,"Audio");
        tab(3,Category::Hud,"HUD");
        tab(4,Category::Crosshair,"Reticle");
        tab(5,Category::Mouse,"Mouse");
        tab(6,Category::Bindings,"Keys");
        ui::surface(left,146,panel_width,height-164);

        const int right=left+panel_width;
        const auto button_row=[&](int y,const char* title,const char* value) {
            ui::label(title,left+20,y+4,17);
            DrawRectangle(left+20,y+29,panel_width-40,1,ui::line);
            return ui::button(right-154,y-2,134,27,value) && interactive;
        };
        const auto toggle=[&](int y,const char* title,bool& value) {
            if (!button_row(y,title,value?"On":"Off")) return false;
            value=!value;
            save();
            return true;
        };
        const auto slider=[&](int y,const char* title,int value,int min,int max,const char* suffix) {
            const int track_x=left+panel_width/2;
            const int track_width=panel_width/2-85;
            const Rectangle bounds{static_cast<float>(track_x),static_cast<float>(y),static_cast<float>(track_width),27};
            ui::label(title,left+20,y+4,17);
            if (interactive && ui::hovered(bounds) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const float ratio=std::clamp((GetMousePosition().x-bounds.x)/bounds.width,0.0f,1.0f);
                value=min+static_cast<int>(std::round(ratio*static_cast<float>(max-min)));
            }
            DrawRectangle(track_x,y+12,track_width,4,ui::line);
            DrawCircle(track_x+(value-min)*track_width/(max-min),y+14,6,ui::accent);
            ui::label(TextFormat("%d%s",value,suffix),right-65,y+3,17);
            DrawRectangle(left+20,y+29,panel_width-40,1,ui::line);
            return value;
        };
        const auto adjust=[&](int y,const char* title,int& value,int min,int max,const char* suffix="") {
            const int next=slider(y,title,value,min,max,suffix);
            if (next!=value) {
                value=next;
                save();
            }
        };

        if (category==Category::Display) {
            if (button_row(160,"Display mode",preferences.fullscreen?"Fullscreen":"Windowed")) {
                preferences.fullscreen=!preferences.fullscreen;
                preferences.apply_window();
                save();
            }
            const auto size=Preferences::sizes[static_cast<std::size_t>(preferences.window_size)];
            if (button_row(199,"Window resolution",TextFormat("%d x %d",size[0],size[1]))) {
                preferences.window_size=(preferences.window_size+1)%static_cast<int>(Preferences::sizes.size());
                if (!preferences.fullscreen) preferences.apply_window();
                save();
            }
            if (button_row(238,"Aspect ratio",Preferences::aspect_names[static_cast<std::size_t>(preferences.aspect)])) {
                preferences.aspect=(preferences.aspect+1)%static_cast<int>(Preferences::aspect_names.size());
                save();
            }
            const auto render_size=preferences.render_size();
            if (button_row(277,"Render resolution",TextFormat("%d x %d",render_size.width,render_size.height))) {
                preferences.resolution=(preferences.resolution+1)%
                    static_cast<int>(Preferences::render_modes.front().size());
                save();
            }
            adjust(316,"Brightness",preferences.brightness,60,140,"%");
            ui::label(TextFormat("Monitor refresh rate: %d Hz",GetMonitorRefreshRate(GetCurrentMonitor())),
                      left+20,360,17,ui::muted);
        } else if (category==Category::Advanced) {
            if (toggle(160,"Vertical sync",preferences.vsync)) preferences.apply_window();
            toggle(199,"4x MSAA at native size (restart)",preferences.msaa);
            adjust(238,"Maximum FPS in game",preferences.match_fps,30,500);
            adjust(277,"Maximum FPS in menus",preferences.menu_fps,30,500);
        } else if (category==Category::Audio) {
            adjust(160,"Effects volume",preferences.effects_volume,0,100,"%");
        } else if (category==Category::Hud) {
            toggle(160,"Show FPS",preferences.show_fps);
            toggle(199,"Show movement stats",preferences.movement_stats);
            toggle(238,"Crosshair hit feedback",preferences.hit_feedback);
            adjust(277,"HUD edge margin",preferences.hud_margin,0,40," px");
        } else if (category==Category::Crosshair) {
            DrawRectangle(left+20,156,panel_width-40,50,{50,62,68,255});
            preferences.draw_crosshair(left+panel_width/2,181);
            adjust(211,"Length",preferences.cross_length,1,20);
            adjust(244,"Gap",preferences.cross_gap,0,16);
            adjust(277,"Thickness",preferences.cross_thickness,1,6);
            adjust(310,"Red",preferences.red,0,255);
            adjust(343,"Green",preferences.green,0,255);
            adjust(376,"Blue",preferences.blue,0,255);
            adjust(409,"Opacity",preferences.cross_alpha,0,255);
            toggle(442,"Center dot",preferences.cross_dot);
            toggle(475,"Outline",preferences.cross_outline);
        } else if (category==Category::Mouse) {
            const float next=static_cast<float>(slider(160,"Sensitivity",static_cast<int>(std::round(sensitivity*10)),1,50,""))/10;
            ui::label(TextFormat("Sensitivity multiplier: %.1fx",static_cast<double>(next)),left+20,245,17,ui::muted);
            if (next!=sensitivity) {
                sensitivity=next;
                save();
            }
            toggle(199,"Invert vertical look",preferences.invert_mouse);
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

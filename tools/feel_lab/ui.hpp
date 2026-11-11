#pragma once

#include "raylib.h"

#include <array>
#include <optional>

namespace feel_lab::ui {

inline constexpr Color panel{19,25,32,246};
inline constexpr Color text{236,240,243,255};
inline constexpr Color muted{163,178,187,255};
inline constexpr Color accent{99,192,214,255};
inline constexpr Color line{64,82,92,255};

inline Font font{};
inline void load() {
#ifdef _WIN32
    constexpr const char* path = "C:/Windows/Fonts/segoeui.ttf";
    if (FileExists(path)) {
        font=LoadFontEx(path,48,nullptr,0);
        if (font.texture.id != GetFontDefault().texture.id) SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    }
#endif
}

inline void unload() {
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
}

inline void label(const char* value, int x, int y, int size, Color color=text) {
    if (font.texture.id == 0) {
        DrawText(value,x,y,size,color);
        return;
    }
    DrawTextEx(font,value,{static_cast<float>(x),static_cast<float>(y)},static_cast<float>(size),1,color);
}

inline bool hovered(Rectangle bounds) {
    return CheckCollisionPointRec(GetMousePosition(),bounds);
}

inline bool button(int x, int y, int width, int height, const char* value, bool selected=false, bool primary=false) {
    const Rectangle bounds{static_cast<float>(x),static_cast<float>(y),
                           static_cast<float>(width),static_cast<float>(height)};
    const bool hover=hovered(bounds);
    const Color fill=primary ? (hover?Color{104,166,79,255}:Color{68,123,59,255}) :
        selected ? Color{34,72,87,255} : hover ? Color{48,65,76,255} : Color{31,42,52,255};
    DrawRectangleRec(bounds,fill);
    DrawRectangleLinesEx(bounds,1,selected?accent:line);
    label(value,x+13,y+(height-18)/2-1,18);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

template<class Command>
struct MenuButton {
    const char* label;
    Command command;
    bool selected = false;
    bool primary = false;
    bool visible = true;
};

struct ButtonLayout {
    int x, y, width, height;
    int step_x, step_y;
};

template<class Command, std::size_t Count>
std::optional<Command> menu_buttons(ButtonLayout layout, const std::array<MenuButton<Command>, Count>& items) {
    std::optional<Command> activated;
    for (const auto& item : items) {
        if (item.visible && button(layout.x, layout.y, layout.width, layout.height,
                                   item.label, item.selected, item.primary)) {
            activated = item.command;
        }
        layout.x += layout.step_x;
        layout.y += layout.step_y;
    }
    return activated;
}

inline void bar() {
    DrawRectangle(0,0,GetScreenWidth(),64,{17,23,29,255});
    DrawRectangle(0,63,GetScreenWidth(),1,line);
    label("CS2CLONE",24,18,24);
}

} // namespace feel_lab::ui

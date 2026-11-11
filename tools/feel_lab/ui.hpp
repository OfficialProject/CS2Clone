#pragma once

#include "raylib.h"

#include <array>
#include <algorithm>
#include <optional>

namespace feel_lab::ui {

inline constexpr Color panel{22,27,31,235};
inline constexpr Color text{241,243,240,255};
inline constexpr Color muted{178,190,188,255};
inline constexpr Color accent{232,188,105,255};
inline constexpr Color line{82,96,96,160};

inline Font font{};
inline Font display_font{};
inline void load() {
#ifdef _WIN32
    constexpr const char* path = "C:/Windows/Fonts/segoeui.ttf";
    if (FileExists(path)) {
        font=LoadFontEx(path,48,nullptr,0);
        if (font.texture.id != GetFontDefault().texture.id) SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
    }
    constexpr const char* display_path = "C:/Windows/Fonts/segoeuib.ttf";
    if (FileExists(display_path)) {
        display_font=LoadFontEx(display_path,84,nullptr,0);
        if (display_font.texture.id != GetFontDefault().texture.id)
            SetTextureFilter(display_font.texture,TEXTURE_FILTER_BILINEAR);
    }
#endif
}

inline void unload() {
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
    if (display_font.texture.id != 0 && display_font.texture.id != GetFontDefault().texture.id)
        UnloadFont(display_font);
}

inline void label(const char* value, int x, int y, int size, Color color=text) {
    if (font.texture.id == 0) {
        DrawText(value,x,y,size,color);
        return;
    }
    DrawTextEx(font,value,{static_cast<float>(x),static_cast<float>(y)},static_cast<float>(size),1,color);
}

inline void heading(const char* value, int x, int y, int size) {
    if (display_font.texture.id==0) {
        label(value,x,y,size);
        return;
    }
    DrawTextEx(display_font,value,{static_cast<float>(x),static_cast<float>(y)},static_cast<float>(size),1,text);
}

inline bool hovered(Rectangle bounds) {
    return CheckCollisionPointRec(GetMousePosition(),bounds);
}

inline int label_width(const char* value, int size) {
    if (font.texture.id==0) return MeasureText(value,size);
    return static_cast<int>(MeasureTextEx(font,value,static_cast<float>(size),1).x);
}

inline void surface(int x, int y, int width, int height, Color fill=panel) {
    DrawRectangle(x,y,width,height,fill);
    DrawRectangleLinesEx({static_cast<float>(x),static_cast<float>(y),
                          static_cast<float>(width),static_cast<float>(height)},1,line);
}

inline bool button(int x, int y, int width, int height, const char* value, bool selected=false, bool primary=false) {
    const Rectangle bounds{static_cast<float>(x),static_cast<float>(y),
                           static_cast<float>(width),static_cast<float>(height)};
    const bool hover=hovered(bounds);
    const Color fill=primary ? (hover?Color{126,177,105,255}:Color{103,153,87,255}) :
        selected ? Color{76,63,42,235} : hover ? Color{65,77,78,244} : Color{41,50,53,222};
    surface(x,y,width,height,fill);
    if (selected) DrawRectangle(x,y,3,height,accent);
    int size=height>=36?18:16;
    while (size>12 && label_width(value,size)>width-16) --size;
    label(value,x+8,y+(height-size)/2-1,size,primary?Color{12,25,14,255}:text);
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
    DrawRectangle(0,0,GetScreenWidth(),64,{19,25,28,248});
    DrawRectangle(0,63,GetScreenWidth(),1,line);
    DrawRectangle(0,0,GetScreenWidth(),2,accent);
    label("CS2CLONE",24,19,23);
}

} // namespace feel_lab::ui

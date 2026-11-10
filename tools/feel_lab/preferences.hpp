#pragma once

#include "ui.hpp"
#include <array>
#include <cmath>
#include <istream>
#include <ostream>

namespace feel_lab {

struct Preferences {
    inline static constexpr std::array<std::array<int,2>,5> sizes{{
        {1024,768},{1280,720},{1280,960},{1600,900},{1920,1080}}};
    struct Resolution { int width; int height; };
    inline static constexpr std::array<const char*,3> aspect_names{"16:9","16:10","4:3"};
    inline static constexpr std::array<std::array<Resolution,8>,3> render_modes{{
        {{{960,540},{1152,648},{1280,720},{1600,900},{1920,1080},{2560,1440},{3200,1800},{3840,2160}}},
        {{{768,480},{960,600},{1280,800},{1440,900},{1680,1050},{1920,1200},{2560,1600},{3840,2400}}},
        {{{640,480},{800,600},{1024,768},{1280,960},{1440,1080},{1600,1200},{2048,1536},{2880,2160}}}
    }};
    int aspect=0, resolution=2;
    static constexpr float vertical_fov=60.0f;

    Resolution render_size() const {
        return render_modes[static_cast<std::size_t>(aspect)][static_cast<std::size_t>(resolution)];
    }

    int window_size=1;
    bool fullscreen=false, vsync=false, msaa=true;
    int brightness=100, match_fps=240, menu_fps=120;
    bool show_fps=true, movement_stats=true, hit_feedback=true, invert_mouse=false;
    int hud_margin=12;
    int cross_length=7, cross_gap=4, cross_thickness=1, cross_alpha=255;
    int red=255, green=255, blue=255;
    bool cross_dot=false, cross_outline=false;

    void read(std::istream& file, bool render_preferences) {
        int legacy_fov=0;
        Preferences value;
        if (!(file >> value.window_size >> value.fullscreen >> value.vsync >> value.msaa
                   >> value.brightness >> value.match_fps >> value.menu_fps >> legacy_fov
                   >> value.show_fps >> value.movement_stats >> value.hit_feedback >> value.invert_mouse
                   >> value.hud_margin >> value.cross_length >> value.cross_gap >> value.cross_thickness
                   >> value.cross_alpha >> value.red >> value.green >> value.blue
                   >> value.cross_dot >> value.cross_outline)) return;
        if (render_preferences && !(file >> value.aspect >> value.resolution)) return;
        const auto valid=[](int n,int min,int max){return n>=min && n<=max;};
        if (!valid(value.window_size,0,static_cast<int>(sizes.size())-1) ||
            !valid(value.brightness,60,140) || !valid(value.match_fps,30,500) ||
            !valid(value.menu_fps,30,500) || !valid(value.aspect,0,2) || !valid(value.resolution,0,7) ||
            !valid(value.hud_margin,0,40) || !valid(value.cross_length,1,20) ||
            !valid(value.cross_gap,0,16) || !valid(value.cross_thickness,1,6) ||
            !valid(value.cross_alpha,0,255) || !valid(value.red,0,255) ||
            !valid(value.green,0,255) || !valid(value.blue,0,255)) return;
        *this=value;
    }

    void write(std::ostream& file) const {
        file << window_size << ' ' << fullscreen << ' ' << vsync << ' ' << msaa << ' '
             << brightness << ' ' << match_fps << ' ' << menu_fps << ' ' << vertical_fov << ' '
             << show_fps << ' ' << movement_stats << ' ' << hit_feedback << ' ' << invert_mouse << ' '
             << hud_margin << ' ' << cross_length << ' ' << cross_gap << ' ' << cross_thickness << ' '
             << cross_alpha << ' ' << red << ' ' << green << ' ' << blue << ' '
             << cross_dot << ' ' << cross_outline << ' ' << aspect << ' ' << resolution << '\n';
    }

    void apply_window() const {
        if (IsWindowFullscreen()!=fullscreen) ToggleFullscreen();
        if (!fullscreen) {
            const auto& size=sizes[static_cast<std::size_t>(window_size)];
            SetWindowSize(size[0],size[1]);
        }
        if (vsync) SetWindowState(FLAG_VSYNC_HINT);
        else ClearWindowState(FLAG_VSYNC_HINT);
    }

    void draw_crosshair(int x,int y,bool hit=false) const {
        const auto alpha=static_cast<unsigned char>(cross_alpha);
        const auto color=hit && hit_feedback ? Color{67,238,117,alpha} :
            Color{static_cast<unsigned char>(red),static_cast<unsigned char>(green),
                  static_cast<unsigned char>(blue),alpha};
        const auto bar=[&](int bx,int by,int width,int height) {
            if (cross_outline) DrawRectangle(bx-1,by-1,width+2,height+2,{0,0,0,alpha});
            DrawRectangle(bx,by,width,height,color);
        };
        const int half=cross_thickness/2;
        bar(x-cross_gap-cross_length,y-half,cross_length,cross_thickness);
        bar(x+cross_gap+1,y-half,cross_length,cross_thickness);
        bar(x-half,y-cross_gap-cross_length,cross_thickness,cross_length);
        bar(x-half,y+cross_gap+1,cross_thickness,cross_length);
        if (cross_dot) bar(x-half,y-half,cross_thickness,cross_thickness);
    }
};

} // namespace feel_lab

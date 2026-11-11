#pragma once

#include "offline_match.hpp"
#include "ui.hpp"

namespace feel_lab {

inline void draw_yard_radar(int x, int y, Vector3 player, Vector3 look, bool attacking) {
    constexpr float scale=2.25f;
    constexpr int size=126;
    ui::surface(x,y,size,size,{19,26,28,230});
    const auto screen=[&](Vector3 point) {
        return Vector2{static_cast<float>(x+size/2)+point.x*scale,
                       static_cast<float>(y+size/2)+point.z*scale};
    };
    for (const auto& box : freight_yard::solids) {
        if (box.center.y<0) continue;
        const auto center=screen(box.center);
        DrawRectangleV({center.x-box.size.x*scale/2,center.y-box.size.z*scale/2},
                       {box.size.x*scale,box.size.z*scale},{88,102,102,255});
    }
    for (const auto site : {freight_yard::site_a,freight_yard::site_b}) {
        const auto center=screen(site);
        DrawCircleV(center,8,{103,79,47,255});
        ui::label(site.x<0?"A":"B",static_cast<int>(center.x)-4,static_cast<int>(center.y)-8,13,ui::accent);
    }
    const auto marker=screen(player);
    const Color team=attacking?ui::accent:Color{141,201,234,255};
    DrawCircleV(marker,3,team);
    DrawLineEx(marker,{marker.x+look.x*9,marker.y+look.z*9},2,team);
}

inline void draw_match_hud(OfflineMatch& match, int margin, int width, int height, bool yard,
                           Vector3 eye, Vector3 look) {
    using Phase=OfflineMatch::Phase;
    const int center=width/2;
    const auto centered=[&](const char* value,int y,int size,Color color=ui::text) {
        ui::label(value,center-ui::label_width(value,size)/2,y,size,color);
    };
    if (yard) draw_yard_radar(margin,margin,eye,look,match.attacking());
    ui::surface(center-91,margin,182,62,{19,26,28,235});
    const char* clock=match.phase==Phase::Buy?"BUY":match.phase==Phase::Planted?"BOMB":"ROUND";
    const int seconds=static_cast<int>(std::ceil(std::max(0.0,match.time)));
    centered(TextFormat("%s %d:%02d",clock,seconds/60,seconds%60),margin+6,24,
             match.phase==Phase::Planted?ui::accent:ui::text);
    centered(TextFormat("%s  %d  -  %d  %s",match.attacking()?"T":"CT",match.won_rounds,
                        match.lost_rounds,match.attacking()?"CT":"T"),margin+36,17,ui::muted);

    const int bottom=height-margin-54;
    ui::surface(margin,bottom,165,54,{19,26,28,235});
    ui::label(TextFormat("HP %d    Armor %d",match.health,match.armor),margin+9,bottom+5,18);
    ui::label(TextFormat("$%d",match.cash),margin+9,bottom+29,16,ui::accent);
    const int right=width-margin-183;
    ui::surface(right,bottom,183,54,{19,26,28,235});
    const char* equipped=match.weapon().name;
    if (match.reload>0) equipped="Reloading";
    else if (match.equipped!=Weapon::Knife)
        equipped=TextFormat("%s  %d / %d",equipped,match.magazine(),match.spare_ammo());
    ui::label(equipped,right+9,bottom+5,18);
    if (match.utility.owned[0] || match.utility.owned[1])
        ui::label(TextFormat("%s%s",match.utility.owned[0]?"4 Smoke   ":"",
                             match.utility.owned[1]?"5 Flash":""),right+9,bottom+29,16,ui::muted);
    if (!match.alive()) centered("Eliminated. Waiting for the bomb outcome.",height/2+60,17,ui::accent);
}

} // namespace feel_lab

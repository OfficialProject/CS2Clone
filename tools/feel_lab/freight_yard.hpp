#pragma once

#include "imported_map.hpp"

namespace feel_lab::freight_yard {

inline constexpr Vector3 t_spawn{0,0.3f,16};
inline constexpr Vector3 ct_spawn{0,0.3f,-16};
inline constexpr Vector3 site_a{-16,0.03f,-7};
inline constexpr Vector3 site_b{16,0.03f,-7};
inline constexpr std::array<Vector3,5> targets{{
    {0,1.25f,3},{-16,1.25f,7},{16,1.25f,7},{-18,1.25f,-11},{18,1.25f,-11}
}};

struct Box { Vector3 center, size; Color color; };
inline constexpr Color concrete{132,143,139,255};
inline constexpr Color steel{56,95,103,255};
inline constexpr Color rust{171,100,62,255};
inline constexpr Color freight{174,149,96,255};
inline constexpr Box solids[]{
    {{0,-0.3f,0},{50,0.6f,42},concrete},
    {{0,2.5f,-21},{50,5,1},steel}, {{0,2.5f,21},{50,5,1},steel},
    {{-25,2.5f,0},{1,5,42},concrete}, {{25,2.5f,0},{1,5,42},concrete},
    {{-8,1.6f,6},{2.6f,3.2f,12},steel}, {{8,1.6f,6},{2.6f,3.2f,12},rust},
    {{-8,1.6f,-10},{2.6f,3.2f,9},rust}, {{8,1.6f,-10},{2.6f,3.2f,9},steel},
    {{0,1.1f,-4},{4,2.2f,3},freight}, {{3,0.55f,3},{2.4f,1.1f,2.4f},freight},
    {{-20,1.45f,1},{4,2.9f,2.6f},steel}, {{20,1.45f,1},{4,2.9f,2.6f},rust},
    {{-20,1.45f,-8},{2.6f,2.9f,5},rust}, {{20,1.45f,-8},{2.6f,2.9f,5},steel},
    {{-13,0.65f,-10},{2.2f,1.3f,2},freight}, {{13,0.65f,-10},{2.2f,1.3f,2},freight},
    {{-16,0.20f,-13},{3,0.4f,2.4f},steel}, {{16,0.20f,-13},{3,0.4f,2.4f},rust},
    {{-5,1.2f,16},{3,2.4f,3},freight}, {{5,1.2f,-16},{3,2.4f,3},freight}
};

inline void load(ImportedMap& map) {
    std::vector<Triangle> geometry;
    geometry.reserve(std::size(solids)*12);
    for (const auto& box : solids) {
        const auto half=Vector3Scale(box.size,0.5f);
        const auto lo=Vector3Subtract(box.center,half);
        const auto hi=Vector3Add(box.center,half);
        const Vector3 a{lo.x,lo.y,lo.z}, b{hi.x,lo.y,lo.z}, c{hi.x,lo.y,hi.z}, d{lo.x,lo.y,hi.z};
        const Vector3 e{lo.x,hi.y,lo.z}, f{hi.x,hi.y,lo.z}, g{hi.x,hi.y,hi.z}, h{lo.x,hi.y,hi.z};
        const auto face=[&](Vector3 p,Vector3 q,Vector3 r,Vector3 s) {
            geometry.push_back({p,q,r});
            geometry.push_back({p,r,s});
        };
        face(e,h,g,f);
        face(a,b,c,d);
        face(a,e,f,b);
        face(d,c,g,h);
        face(a,d,h,e);
        face(b,f,g,c);
    }
    map.load_original(t_spawn,std::move(geometry));
}

inline void draw() {
    DrawCube({-20,5,-34},20,12,12,{86,109,114,255});
    DrawCube({0,2,-33},15,7,8,{105,125,125,255});
    DrawCube({23,5,-36},19,13,14,{94,114,120,255});
    DrawCube({-35,4,-20},12,10,25,{107,130,132,255});
    DrawCube({36,4,-13},13,10,30,{106,125,126,255});
    for (const auto& box : solids) {
        DrawCubeV(box.center,box.size,box.color);
        if (box.center.y>0) DrawCubeWiresV(box.center,box.size,ColorBrightness(box.color,-0.30f));
        if (box.size.z>=5 && box.size.x<5 && box.size.y>2) {
            const Color ribs=ColorBrightness(box.color,-0.16f);
            const float half=box.size.x*0.5f;
            for (float z=-box.size.z*0.5f+0.36f; z<box.size.z*0.5f; z+=0.45f)
                for (float side : {-1.0f,1.0f})
                    DrawCube(Vector3Add(box.center,{side*half,0,z}),0.035f,box.size.y-0.24f,0.07f,ribs);
        }
    }
    for (const auto site : {site_a,site_b}) {
        DrawPlane(site,{6,6},{186,142,72,255});
        DrawPlane(Vector3Add(site,{0,0.008f,0}),{5.6f,5.6f},{104,110,102,255});
    }
    for (float x : {-5.0f,5.0f}) {
        for (int z=-19;z<20;z+=3)
            DrawPlane({x,0.015f,static_cast<float>(z)},{0.10f,1.4f},{210,195,151,255});
    }
}

} // namespace feel_lab::freight_yard

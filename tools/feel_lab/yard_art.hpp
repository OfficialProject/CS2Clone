#pragma once

#include "freight_yard.hpp"
#include "rlgl.h"

#include <cstdint>

namespace feel_lab::freight_yard {

class Art {
    enum Material : std::size_t { Concrete, PaintedSteel, Plywood, MaterialCount };
    std::array<Texture2D,MaterialCount> textures{};
    Texture2D site_stencil{};
    Texture2D gate_sign{};

    static unsigned noise(unsigned x, unsigned y) {
        std::uint32_t value=(x+37u)*374761393u+(y+61u)*668265263u;
        value=(value^(value>>13u))*1274126177u;
        return (value^(value>>16u))&255u;
    }

    static Texture2D material_texture(Material material) {
        constexpr int size=256;
        auto image=GenImageColor(size,size,WHITE);
        if (!image.data) return {};
        auto* pixels=static_cast<Color*>(image.data);
        for (unsigned y=0;y<size;++y) {
            for (unsigned x=0;x<size;++x) {
                const int grain=static_cast<int>(noise(x,y))-128;
                const int aggregate=static_cast<int>(noise(x/3,y/3))-128;
                int tone=226+grain/14+aggregate/13;
                if (material==Concrete && (x==0 || y==0)) tone-=34;
                if (material==Concrete && grain>120) tone-=19;
                if (material==PaintedSteel) {
                    tone=234+grain/22;
                    const int chip=static_cast<int>(noise(x/5,y/2));
                    if (chip<10 && grain<30) tone-=38;
                }
                if (material==Plywood) {
                    const float bands=std::sin(static_cast<float>(y)*0.39f+
                                               std::sin(static_cast<float>(x)*0.029f)*0.6f);
                    tone=220+static_cast<int>(12*bands)+grain/18;
                    if (y%64<2) tone-=35;
                }
                const auto shade=static_cast<unsigned char>(std::clamp(tone,0,255));
                pixels[y*size+x]={shade,shade,shade,255};
            }
        }
        auto texture=LoadTextureFromImage(image);
        UnloadImage(image);
        if (texture.id!=0) {
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture,TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(texture,TEXTURE_WRAP_REPEAT);
        }
        return texture;
    }

    static Texture2D lettering(int width,int height,const char* title,int size,Color background,Color ink) {
        auto image=GenImageColor(width,height,background);
        if (!image.data) return {};
        ImageDrawText(&image,title,(width-MeasureText(title,size))/2,(height-size)/2,size,ink);
        auto texture=LoadTextureFromImage(image);
        UnloadImage(image);
        if (texture.id!=0) {
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture,TEXTURE_FILTER_TRILINEAR);
        }
        return texture;
    }

    static void quad(Texture2D texture,Vector3 a,Vector3 b,Vector3 c,Vector3 d,
                     float width,float height,Color tint) {
        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(tint.r,tint.g,tint.b,tint.a);
        rlTexCoord2f(0,0); rlVertex3f(a.x,a.y,a.z);
        rlTexCoord2f(0,height); rlVertex3f(b.x,b.y,b.z);
        rlTexCoord2f(width,height); rlVertex3f(c.x,c.y,c.z);
        rlTexCoord2f(width,0); rlVertex3f(d.x,d.y,d.z);
        rlEnd();
        rlSetTexture(0);
    }

    void box(const Box& solid,Material material,Color tint) const {
        const auto texture=textures[material];
        if (texture.id==0) {
            DrawCubeV(solid.center,solid.size,tint);
            return;
        }
        const auto half=Vector3Scale(solid.size,0.5f);
        const auto lo=Vector3Subtract(solid.center,half);
        const auto hi=Vector3Add(solid.center,half);
        const Vector3 a{lo.x,lo.y,lo.z}, b{hi.x,lo.y,lo.z}, c{hi.x,lo.y,hi.z}, d{lo.x,lo.y,hi.z};
        const Vector3 e{lo.x,hi.y,lo.z}, f{hi.x,hi.y,lo.z}, g{hi.x,hi.y,hi.z}, h{lo.x,hi.y,hi.z};
        const float repeats=material==Concrete?0.55f:0.70f;
        const auto shade=[&](float light) { return ColorBrightness(tint,light); };
        quad(texture,e,h,g,f,solid.size.x*repeats,solid.size.z*repeats,shade(0.16f));
        quad(texture,a,b,c,d,solid.size.z*repeats,solid.size.x*repeats,shade(-0.30f));
        quad(texture,f,b,a,e,solid.size.x*repeats,solid.size.y*repeats,shade(-0.20f));
        quad(texture,h,d,c,g,solid.size.x*repeats,solid.size.y*repeats,shade(0.04f));
        quad(texture,e,a,d,h,solid.size.z*repeats,solid.size.y*repeats,shade(-0.08f));
        quad(texture,g,c,b,f,solid.size.z*repeats,solid.size.y*repeats,shade(0.07f));
    }

    static void container_trim(const Box& solid,Color tint) {
        const auto half=Vector3Scale(solid.size,0.5f);
        const auto frame=ColorBrightness(tint,-0.29f);
        for (float side : {-1.0f,1.0f}) {
            for (float y : {-half.y+0.10f,half.y-0.10f})
                DrawCube(Vector3Add(solid.center,{side*half.x,y,0}),0.035f,0.15f,solid.size.z,frame);
            for (float z=-half.z+0.34f;z<half.z-0.23f;z+=0.33f)
                DrawCube(Vector3Add(solid.center,{side*half.x,0,z}),0.042f,solid.size.y-0.30f,0.075f,
                         ColorBrightness(tint,-0.19f));
        }
        for (float side : {-1.0f,1.0f}) {
            const float z=side*(half.z+0.015f);
            DrawCube(Vector3Add(solid.center,{0,0,z}),0.045f,solid.size.y-0.22f,0.045f,frame);
            for (float x : {-half.x*0.63f,-half.x*0.25f,half.x*0.25f,half.x*0.63f})
                DrawCube(Vector3Add(solid.center,{x,0,z}),0.052f,solid.size.y-0.41f,0.045f,{80,89,89,255});
        }
    }

    void backdrop() const {
        constexpr Color warehouse{109,123,124,255};
        box({{-31,3,-13},{9,8,25},warehouse},PaintedSteel,warehouse);
        box({{-14,3,-34},{29,8,12},warehouse},PaintedSteel,{132,138,135,255});
        box({{21,4,-34},{24,10,13},warehouse},PaintedSteel,{109,129,132,255});
        constexpr Color gantry{188,141,69,255};
        for (float z : {-20.0f,-4.0f})
            DrawCube({-28,5.5f,z},0.66f,11.0f,0.66f,gantry);
        DrawCube({-28,10.8f,-12},0.87f,0.75f,17.0f,gantry);
        DrawCube({-27,10.7f,-12},4.8f,0.55f,0.57f,gantry);
        DrawCube({-25.6f,10.15f,-12},0.75f,0.60f,0.72f,{74,78,76,255});
        DrawCube({-25.6f,8.5f,-12},0.025f,2.8f,0.025f,{55,60,61,255});
        DrawCube({-25.6f,7.1f,-12},0.23f,0.18f,0.23f,gantry);
    }

    void loading_bay() const {
        constexpr float wall=-20.48f;
        constexpr Color frame{67,79,78,255};
        constexpr Color panel{127,140,135,255};
        DrawCube({-15.7f,0.30f,wall},17.2f,0.58f,0.035f,frame);
        for (float x : {-21.2f,-12.7f}) {
            DrawCube({x,1.69f,wall+0.03f},4.72f,3.39f,0.048f,frame);
            DrawCube({x,1.53f,wall+0.06f},4.36f,3.04f,0.030f,panel);
            for (float y=0.24f;y<3.04f;y+=0.21f)
                DrawCube({x,y,wall+0.083f},4.36f,0.025f,0.014f,{104,116,111,255});
            DrawCube({x,3.08f,wall+0.09f},4.48f,0.10f,0.045f,{172,167,142,255});
        }
        for (float x : {-23.8f,-18.5f,-15.5f,-9.8f})
            DrawCube({x,2.49f,wall},0.17f,4.97f,0.07f,{101,112,109,255});
        for (float x : {-22.5f,-18.0f,-13.5f,-9.0f}) {
            DrawCube({x,4.25f,wall},2.75f,0.64f,0.045f,frame);
            for (float dx : {-0.86f,0.0f,0.86f})
                DrawCube({x+dx,4.25f,wall+0.03f},0.76f,0.44f,0.026f,{113,141,148,255});
        }
        if (gate_sign.id!=0)
            quad(gate_sign,{-19.6f,3.99f,wall+0.10f},{-19.6f,3.43f,wall+0.10f},
                           {-13.6f,3.43f,wall+0.10f},{-13.6f,3.99f,wall+0.10f},1,1,WHITE);
    }

    void markings() const {
        constexpr Color paint{216,178,96,255};
        constexpr float y=0.027f;
        for (const float x : {-18.9f,-13.1f}) DrawPlane({x,y,-7},{0.10f,5.8f},paint);
        for (const float z : {-9.9f,-4.1f}) DrawPlane({-16,y,z},{5.8f,0.10f},paint);
        for (float z=12.0f;z>-4.0f;z-=2.8f)
            DrawPlane({-15.9f,y,z},{0.12f,1.2f},paint);
        for (float x=-18.7f;x<-13.3f;x+=0.42f)
            DrawPlane({x,y,-4.39f},{0.20f,0.32f},paint);
        if (site_stencil.id!=0)
            quad(site_stencil,{-16.95f,y,-7.95f},{-16.95f,y,-6.05f},
                              {-15.05f,y,-6.05f},{-15.05f,y,-7.95f},1,1,WHITE);
    }

public:
    Art()=default;
    Art(const Art&)=delete;
    Art& operator=(const Art&)=delete;

    void load() {
        for (std::size_t index=0;index<textures.size();++index)
            textures[index]=material_texture(static_cast<Material>(index));
        site_stencil=lettering(128,128,"A",96,BLANK,{222,185,105,255});
        gate_sign=lettering(256,64,"LOADING BAY A",29,{47,65,66,255},{231,230,204,255});
    }

    void unload() {
        for (auto& texture : textures) {
            if (texture.id!=0) UnloadTexture(texture);
            texture={};
        }
        if (site_stencil.id!=0) UnloadTexture(site_stencil);
        if (gate_sign.id!=0) UnloadTexture(gate_sign);
        site_stencil=gate_sign={};
    }

    void draw() const {
        backdrop();
        for (const auto& solid : solids) {
            const bool boundary=solid.size.x>20 || solid.size.z>20;
            const bool wood=solid.color.r==freight.r;
            const Material material=boundary?Concrete:wood?Plywood:PaintedSteel;
            Color tint=solid.color;
            if (boundary) tint=solid.center.y<0?Color{140,143,134,255}:Color{118,128,123,255};
            else if (wood) tint={168,144,105,255};
            else if (solid.color.r==steel.r) tint={71,114,117,255};
            else tint={154,93,67,255};
            box(solid,material,tint);
            if (!boundary && !wood && solid.size.y>2) container_trim(solid,tint);
            if (!boundary && solid.center.y>0)
                DrawPlane({solid.center.x+0.25f,0.013f,solid.center.z+0.19f},
                          {solid.size.x+0.51f,solid.size.z+0.39f},{43,50,46,96});
        }
        DrawPlane(site_b,{6,6},{186,142,72,255});
        DrawPlane(Vector3Add(site_b,{0,0.008f,0}),{5.6f,5.6f},{104,110,102,255});
        markings();
        loading_bay();
    }
};

} // namespace feel_lab::freight_yard

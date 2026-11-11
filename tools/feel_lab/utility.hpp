#pragma once

#include "map_collision.hpp"

#include <array>

namespace feel_lab {

struct Grenade {
    Vector3 position{};
    Vector3 velocity{};
    double fuse=0;

    void launch(Ray aim) {
        position=aim.position;
        velocity=Vector3Add(Vector3Scale(aim.direction,12.0f),{0,2.6f,0});
        fuse=1.5;
    }

    // True on the tick the fuse expires.
    bool step(const MapCollision& collision, double seconds) {
        if (fuse<=0) return false;
        const float dt=static_cast<float>(seconds);
        velocity.y-=9.8f*dt;
        constexpr float radius=0.07f;
        const auto delta=Vector3Scale(velocity,dt);
        const auto contact=collision.sweep(Vector3Subtract(position,{0,radius,0}),delta,2*radius,radius);
        position=Vector3Add(position,Vector3Scale(delta,contact.fraction));
        if (contact.fraction<1) {
            position=Vector3Add(position,Vector3Scale(contact.normal,0.003f));
            const float into=Vector3DotProduct(velocity,contact.normal);
            if (into<0) velocity=Vector3Scale(Vector3Subtract(velocity,Vector3Scale(contact.normal,1.5f*into)),0.72f);
        }
        fuse=std::max(0.0,fuse-seconds);
        return fuse==0;
    }
};

struct Utility {
    std::array<bool,2> owned{};
    std::array<Grenade,2> grenades{};
    Vector3 smoke{};
    double smoke_seconds=0;
    double player_flash=0;
    static constexpr float smoke_radius=2.4f;

    bool obscures(Ray ray,float length) const {
        if (smoke_seconds<=0) return false;
        const auto from_eye=Vector3Subtract(smoke,ray.position);
        const float distance=std::clamp(Vector3DotProduct(from_eye,ray.direction),0.0f,length);
        return Vector3Distance(Vector3Add(ray.position,Vector3Scale(ray.direction,distance)),smoke)<smoke_radius;
    }

    static bool flash_visible(const MapCollision& collision,Vector3 flash,Vector3 eye) {
        const auto offset=Vector3Subtract(flash,eye);
        const float distance=Vector3Length(offset);
        return distance<14 && (distance<0.001f ||
            collision.shot_distance({eye,Vector3Scale(offset,1/distance)},distance)>=distance-0.025f);
    }

    void draw() const {
        for (std::size_t index=0;index<grenades.size();++index)
            if (grenades[index].fuse>0)
                DrawSphere(grenades[index].position,0.08f,index==0?DARKGREEN:LIGHTGRAY);
        if (smoke_seconds>0) {
            for (const float x : {-0.85f,0.85f})
                for (const float z : {-0.7f,0.7f})
                    DrawSphere(Vector3Add(smoke,{x,0,z}),1.5f,{131,140,136,255});
            DrawSphere(Vector3Add(smoke,{0,0.5f,0}),1.72f,{145,151,147,255});
        }
    }
};

} // namespace feel_lab

#include "map_collision.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    using namespace feel_lab;
    MapCollision collision;
    const Triangle floor_a{{-5,0,-5},{5,0,5},{5,0,-5}};
    const Triangle floor_b{{-5,0,-5},{-5,0,5},{5,0,5}};
    collision.build({floor_a,floor_b,
        {{2,0,-5},{2,3,-5},{2,3,5}}, {{2,0,-5},{2,3,5},{2,0,5}}});
    const auto landing=collision.move({0,2,0},{0,-4,0},{0,-4,0},1.67f,false);
    if (!landing.grounded || std::abs(landing.position.y)>0.01f) {
        std::cerr<<"Fall should stop on the floor\n";
        return EXIT_FAILURE;
    }
    const auto slide=collision.move({0,0.001f,0},{4,0,1},{4,0,1},1.67f,true);
    if (!slide.grounded || slide.position.x>2-MapCollision::radius ||
        std::abs(slide.position.z-1)>0.01f || std::abs(slide.velocity.x)>0.001f) {
        std::cerr<<"Wall should stop the hull and preserve motion along the wall\n";
        return EXIT_FAILURE;
    }

    collision.build({floor_a,floor_b,
        {{1,0,-2},{1,0.2f,-2},{1,0.2f,2}}, {{1,0,-2},{1,0.2f,2},{1,0,2}},
        {{1,0.2f,-2},{3,0.2f,2},{3,0.2f,-2}}, {{1,0.2f,-2},{1,0.2f,2},{3,0.2f,2}}});
    const auto step=collision.move({0.5f,0.001f,0},{1,0,0},{0.4f,0,0},1.67f,true);
    if (!step.grounded || std::abs(step.position.y-0.2f)>0.01f ||
        std::abs(step.position.x-0.9f)>0.01f) {
        std::cerr<<"Grounded movement should climb a walkable step\n";
        return EXIT_FAILURE;
    }
    const auto air=collision.move({0.5f,0.001f,0},{1,0,0},{0.4f,0,0},1.67f,false);
    if (air.position.x>1-MapCollision::radius || air.position.y>0.01f) {
        std::cerr<<"Airborne movement should not receive a step boost\n";
        return EXIT_FAILURE;
    }
    std::cout<<"Map collision tests passed\n";
}

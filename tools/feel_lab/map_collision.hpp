#pragma once

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace feel_lab {

struct Triangle { Vector3 a, b, c; };
struct Sweep { float fraction = 1; Vector3 normal{}; };
struct Motion { Vector3 position; Vector3 velocity; bool grounded = false; };

class MapCollision {
    static constexpr float cell_size = 2.0f;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells;
    std::vector<Triangle> triangles;

    static std::uint64_t key(int x, int z) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
               static_cast<std::uint32_t>(z);
    }
    static int cell(float coordinate) { return static_cast<int>(std::floor(coordinate / cell_size)); }

    static bool triangle_sweep(Vector3 center, Vector3 half, Vector3 delta,
                               const Triangle& triangle, Sweep& result) {
        const std::array<Vector3, 3> edges{
            Vector3Subtract(triangle.b, triangle.a),
            Vector3Subtract(triangle.c, triangle.b),
            Vector3Subtract(triangle.a, triangle.c)};
        const std::array<Vector3, 3> basis{Vector3{1,0,0}, Vector3{0,1,0}, Vector3{0,0,1}};
        std::array<Vector3, 13> axes{};
        std::copy(basis.begin(), basis.end(), axes.begin());
        axes[3] = Vector3CrossProduct(edges[0], edges[1]);
        std::size_t offset = 4;
        for (Vector3 edge : edges) {
            for (Vector3 axis : basis) axes[offset++] = Vector3CrossProduct(edge, axis);
        }
        float enter = -std::numeric_limits<float>::infinity();
        float leave = std::numeric_limits<float>::infinity();
        Vector3 normal{};
        for (Vector3 axis : axes) {
            const float magnitude = Vector3Length(axis);
            if (magnitude < 1e-7f) continue;
            axis = Vector3Scale(axis, 1 / magnitude);
            const float a = Vector3DotProduct(triangle.a, axis);
            const float b = Vector3DotProduct(triangle.b, axis);
            const float c = Vector3DotProduct(triangle.c, axis);
            const float projected_half_extent = std::fabs(axis.x)*half.x + std::fabs(axis.y)*half.y +
                                 std::fabs(axis.z)*half.z;
            const float low = std::min({a,b,c}) - projected_half_extent;
            const float high = std::max({a,b,c}) + projected_half_extent;
            const float origin = Vector3DotProduct(center, axis);
            const float speed = Vector3DotProduct(delta, axis);
            if (std::fabs(speed) < 1e-8f) {
                if (origin < low - 1e-6f || origin > high + 1e-6f) return false;
                continue;
            }
            float first = (low - origin) / speed;
            float last = (high - origin) / speed;
            if (first > last) std::swap(first, last);
            if (first > enter) {
                enter = first;
                normal = Vector3Scale(axis, speed > 0 ? -1.0f : 1.0f);
            }
            leave = std::min(leave, last);
            if (enter > leave) return false;
        }
        // An already intersecting face cannot determine an entering plane.
        if (enter < -1e-5f || enter > result.fraction || leave < 0) return false;
        result = {std::max(0.0f, enter), normal};
        return true;
    }

    Motion slide(Vector3 feet, Vector3 velocity, Vector3 delta, float height) const {
        Motion motion{feet, velocity};
        for (int iteration = 0; iteration < 5 && Vector3LengthSqr(delta) > 1e-12f; ++iteration) {
            const auto hit = sweep(motion.position, delta, height);
            motion.position = Vector3Add(motion.position, Vector3Scale(delta, hit.fraction));
            if (hit.fraction == 1) break;
            motion.position = Vector3Add(motion.position, Vector3Scale(hit.normal, 0.001f));
            delta = Vector3Scale(delta, 1 - hit.fraction);
            const float into = Vector3DotProduct(delta, hit.normal);
            if (into < 0) delta = Vector3Subtract(delta, Vector3Scale(hit.normal, into));
            const float velocity_into = Vector3DotProduct(motion.velocity, hit.normal);
            if (velocity_into < 0) {
                motion.velocity = Vector3Subtract(motion.velocity, Vector3Scale(hit.normal, velocity_into));
            }
            motion.grounded = motion.grounded || hit.normal.y >= 0.7f;
        }
        return motion;
    }

public:
    static constexpr float radius = 16.0f * (5.8f / 250);
    static constexpr float step_height = 18.0f * (5.8f / 250);

    void build(std::vector<Triangle> input) {
        cells.clear();
        triangles = std::move(input);
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            const auto& t = triangles[index];
            const int x0 = cell(std::min({t.a.x,t.b.x,t.c.x}));
            const int x1 = cell(std::max({t.a.x,t.b.x,t.c.x}));
            const int z0 = cell(std::min({t.a.z,t.b.z,t.c.z}));
            const int z1 = cell(std::max({t.a.z,t.b.z,t.c.z}));
            if (x1-x0 > 2048 || z1-z0 > 2048) throw std::runtime_error("Collision triangle is too large");
            for (int x = x0; x <= x1; ++x) {
                for (int z = z0; z <= z1; ++z)
                    cells[key(x,z)].push_back(static_cast<std::uint32_t>(index));
            }
        }
    }

    Sweep sweep(Vector3 feet, Vector3 delta, float height, float half_width=radius) const {
        const auto end = Vector3Add(feet, delta);
        std::vector<std::uint32_t> candidates;
        for (int x=cell(std::min(feet.x,end.x)-half_width); x<=cell(std::max(feet.x,end.x)+half_width); ++x) {
            for (int z=cell(std::min(feet.z,end.z)-half_width); z<=cell(std::max(feet.z,end.z)+half_width); ++z) {
                const auto found = cells.find(key(x,z));
                if (found != cells.end()) candidates.insert(candidates.end(), found->second.begin(), found->second.end());
            }
        }
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        const Vector3 half{half_width, height/2, half_width};
        const auto center = Vector3Add(feet, {0,height/2,0});
        Sweep hit;
        for (const auto index : candidates) triangle_sweep(center, half, delta, triangles[index], hit);
        return hit;
    }

    float shot_distance(Ray ray, float limit) const {
        for (const auto& triangle : triangles) {
            const auto hit = GetRayCollisionTriangle(ray,triangle.a,triangle.b,triangle.c);
            if (hit.hit && hit.distance < limit) limit = hit.distance;
        }
        return limit;
    }

    Motion move(Vector3 feet, Vector3 velocity, Vector3 delta, float height, bool was_grounded) const {
        auto result = slide(feet, velocity, delta, height);
        const auto distance = [feet](Vector3 point) {
            return (point.x-feet.x)*(point.x-feet.x) + (point.z-feet.z)*(point.z-feet.z);
        };
        const float desired = delta.x*delta.x + delta.z*delta.z;
        if (was_grounded && velocity.y <= 0 && distance(result.position) + 1e-7f < desired) {
            const Vector3 up{0, step_height + 0.003f, 0};
            if (sweep(feet, up, height).fraction == 1) {
                auto stepped = slide(Vector3Add(feet,up), velocity, {delta.x,0,delta.z}, height);
                const Vector3 down{0,-step_height-0.06f,0};
                const auto floor = sweep(stepped.position, down, height);
                if (floor.fraction < 1 && floor.normal.y >= 0.7f &&
                    distance(stepped.position) > distance(result.position) + 1e-7f) {
                    stepped.position = Vector3Add(stepped.position, Vector3Scale(down, floor.fraction));
                    stepped.position.y += 0.001f;
                    stepped.velocity.y = 0;
                    stepped.grounded = true;
                    result = stepped;
                }
            }
        }
        if (velocity.y <= 0) {
            const Vector3 down{0,-0.06f,0};
            const auto floor = sweep(result.position, down, height);
            if (floor.fraction < 1 && floor.normal.y >= 0.7f) {
                result.position = Vector3Add(result.position, Vector3Scale(down, floor.fraction));
                result.position.y += 0.001f;
                result.velocity.y = 0;
                result.grounded = true;
            }
        }
        return result;
    }
};

} // namespace feel_lab

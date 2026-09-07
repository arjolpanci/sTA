#pragma once
#include <array>
#include <cmath>
#include <glm/glm.hpp>

// Upright box: yaw rotates the footprint, never the vertical axis.
// Separating-axis tests keep vehicle corners accurate at every heading.
struct CollisionBox
{
    glm::vec3 center{0.0f};
    glm::vec3 half{0.0f};
    float yaw = 0.0f;

    static CollisionBox fromCenterHalf(const glm::vec3& center, const glm::vec3& half, float yaw = 0.0f)
    { return {center, half, yaw}; }

    std::array<glm::vec2, 2> axes() const
    {
        float r = glm::radians(yaw), c = std::cos(r), s = std::sin(r);
        return {glm::vec2(c, -s), glm::vec2(s, c)};
    }

    bool intersects(const CollisionBox& other) const
    {
        constexpr float epsilon = 0.0001f; // touching faces are not penetration
        if (std::abs(center.y - other.center.y) >= half.y + other.half.y - epsilon)
            return false;
        auto a = axes(), b = other.axes();
        glm::vec2 delta(other.center.x - center.x, other.center.z - center.z);
        for (const auto& axis : {a[0], a[1], b[0], b[1]})
        {
            float ra = half.x * std::abs(glm::dot(a[0], axis)) + half.z * std::abs(glm::dot(a[1], axis));
            float rb = other.half.x * std::abs(glm::dot(b[0], axis)) + other.half.z * std::abs(glm::dot(b[1], axis));
            if (std::abs(glm::dot(delta, axis)) >= ra + rb - epsilon)
                return false;
        }
        return true;
    }
};

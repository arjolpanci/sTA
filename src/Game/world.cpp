#include "world.hpp"

#include <algorithm>
#include <cmath>

std::optional<float> Ramp::heightAt(float x, float z) const
{
    float dx = x - footprintCenter.x;
    float dz = z - footprintCenter.z;
    float along = alongX ? dx : dz;    // position along the slope direction
    float across = alongX ? dz : dx;   // position across the slope (width)

    float halfWidth = footprintSize.x * 0.5f;
    float halfLength = footprintSize.y * 0.5f;
    if (std::abs(across) > halfWidth || std::abs(along) > halfLength)
        return std::nullopt;

    float t = (along + halfLength) / (2.0f * halfLength); // 0 at low end, 1 at high end
    return lowHeight + t * (highHeight - lowHeight);
}

World::World()
{
    const float halfX = m_groundSize.x * 0.5f; // 80
    const float halfZ = m_groundSize.y * 0.5f; // 50
    const glm::vec3 wallColor(0.45f, 0.45f, 0.48f);
    const float wallHeight = 4.0f;

    // border walls
    addBox({ 0.0f, 0.0f, -halfZ }, { m_groundSize.x, wallHeight, 1.0f }, wallColor);
    addBox({ 0.0f, 0.0f,  halfZ }, { m_groundSize.x, wallHeight, 1.0f }, wallColor);
    addBox({ -halfX, 0.0f, 0.0f }, { 1.0f, wallHeight, m_groundSize.y }, wallColor);
    addBox({  halfX, 0.0f, 0.0f }, { 1.0f, wallHeight, m_groundSize.y }, wallColor);

    // buildings
    addBox({ -45.0f, 0.0f, -25.0f }, { 22.0f, 14.0f, 16.0f }, { 0.55f, 0.50f, 0.48f });
    addBox({  35.0f, 0.0f, -30.0f }, { 18.0f, 24.0f, 18.0f }, { 0.40f, 0.42f, 0.50f });
    addBox({  45.0f, 0.0f,  25.0f }, { 14.0f, 10.0f, 20.0f }, { 0.60f, 0.45f, 0.40f });
    addBox({ -35.0f, 0.0f,  30.0f }, { 26.0f,  8.0f, 12.0f }, { 0.50f, 0.55f, 0.60f });
    addBox({   0.0f, 0.0f, -38.0f }, { 12.0f, 18.0f, 10.0f }, { 0.45f, 0.45f, 0.45f });
    addBox({ -10.0f, 0.0f,  18.0f }, { 10.0f,  6.0f, 10.0f }, { 0.58f, 0.52f, 0.45f });

    // placeholder ramps, to test walking/driving up a slope - both clear of
    // every building above, verified by hand against their footprints
    addRamp({ 55.0f, 0.0f, -5.0f }, { 6.0f, 10.0f }, false, 0.0f, 3.5f, { 0.55f, 0.52f, 0.50f });  // rises toward +Z
    addRamp({ -65.0f, 0.0f, -5.0f }, { 6.0f, 10.0f }, true, 0.0f, 3.5f, { 0.50f, 0.55f, 0.52f });  // rises toward +X
}

void World::addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color)
{
    glm::vec3 center = centerOnGround + glm::vec3(0.0f, size.y * 0.5f, 0.0f);
    m_boxes.push_back({ center, size, color });
    m_colliders.push_back(AABB::fromCenterHalf(center, size * 0.5f));
}

void World::addRamp(const glm::vec3& footprintCenter, const glm::vec2& footprintSize, bool alongX,
                     float lowHeight, float highHeight, const glm::vec3& color)
{
    m_ramps.push_back({ footprintCenter, footprintSize, alongX, lowHeight, highHeight, color });
}

bool World::collides(const AABB& box) const
{
    for (const AABB& collider : m_colliders)
        if (box.intersects(collider))
            return true;
    return false;
}

float World::groundHeightAt(float x, float z) const
{
    float height = 0.0f; // flat asphalt floor

    for (const StaticBox& box : m_boxes)
    {
        float halfX = box.size.x * 0.5f;
        float halfZ = box.size.z * 0.5f;
        bool withinFootprint = x >= box.center.x - halfX && x <= box.center.x + halfX &&
                               z >= box.center.z - halfZ && z <= box.center.z + halfZ;
        if (withinFootprint)
            height = std::max(height, box.center.y + box.size.y * 0.5f);
    }

    for (const Ramp& ramp : m_ramps)
    {
        if (std::optional<float> h = ramp.heightAt(x, z))
            height = std::max(height, *h);
    }

    return height;
}

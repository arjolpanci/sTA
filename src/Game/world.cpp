#include "world.hpp"

#include <algorithm>
#include <cmath>
#include "vertical_motion.hpp"

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

    // Roads lie at multiples of 60; each block has a six-metre sidewalk
    // surrounding its lots. The central southeast block is a public park.
    auto detail = [this](glm::vec3 center, glm::vec3 size, glm::vec3 color) {
        m_decorations.push_back({center, size, color});
    };
    for (int ix = -3; ix < 3; ++ix)
    for (int iz = -3; iz < 3; ++iz)
    {
        float x = ix * 60.0f + 30.0f, z = iz * 60.0f + 30.0f;
        addBox({x, 0, z}, {44, 0.16f, 44}, {0.56f, 0.57f, 0.55f});
        bool park = (ix == 0 && iz == 0);
        bool yard = (ix == -2 && iz == 0);
        if (park || yard)
        {
            detail({x, 0.18f, z}, {34, 0.03f, 34}, park ? glm::vec3(0.23f, 0.39f, 0.23f) : glm::vec3(0.39f));
            if (yard)
            {
                addRamp({x - 8, 0, z}, {6, 14}, false, 0.16f, 3.66f, {0.65f, 0.58f, 0.38f});
                addRamp({x + 8, 0, z}, {6, 14}, true, 0.16f, 3.66f, {0.65f, 0.58f, 0.38f});
            }
        }
        else
        {
            int seed = (ix + 3) * 7 + iz + 3;
            for (int lot = 0; lot < 2; ++lot)
            {
                float bx = x + (lot == 0 ? -9.0f : 9.0f);
                float h = 7.0f + static_cast<float>((seed * 13 + lot * 7) % 25);
                glm::vec3 color = glm::vec3(0.48f + (seed % 3) * 0.08f, 0.46f + (seed % 4) * 0.05f, 0.43f + (seed % 5) * 0.04f);
                addBox({bx, 0.16f, z}, {16, h, 30}, color);
                m_boxes.back().facade = true;
                detail({bx, h + 0.35f, z}, {16.5f, 0.4f, 30.5f}, {0.25f, 0.28f, 0.30f});
                detail({bx, h + 0.8f, z + 3}, {4, 1, 5}, {0.42f, 0.45f, 0.47f});
                detail({bx, 2.8f, z - 15.4f}, {12, 0.3f, 1.1f}, lot ? glm::vec3(0.72f, 0.31f, 0.18f) : glm::vec3(0.18f, 0.42f, 0.45f));
                detail({bx, 1.25f, z - 15.02f}, {1.6f, 2.2f, 0.04f}, {0.12f, 0.19f, 0.22f});
            }
        }
        // Street trees and lamps are spaced away from sidewalk walking routes.
        for (int side : {-1, 1})
        {
            float tx = x + side * 16.5f, tz = z + 17.5f;
            addBox({tx, 0.16f, tz}, {0.65f, 3.1f, 0.65f}, {0.32f, 0.23f, 0.16f});
            detail({tx, 4, tz}, {3.8f, 3.6f, 3.8f}, {0.20f, 0.37f + (ix + 3) * 0.018f, 0.22f});
            detail({tx + 0.3f, 5.9f, tz}, {2.7f, 1.6f, 2.7f}, {0.27f, 0.45f, 0.25f});
            float lx = x + side * 20.5f;
            addBox({lx, 0.16f, z - 16}, {0.18f, 5.5f, 0.18f}, {0.18f, 0.22f, 0.25f});
            detail({lx, 5.65f, z - 16.6f}, {0.6f, 0.18f, 1.6f}, {0.95f, 0.88f, 0.63f});
        }
        if (park)
            for (int i = -1; i <= 1; ++i)
            {
                addBox({x + i * 9.0f, 0.16f, z}, {3, 0.6f, 1}, {0.48f, 0.29f, 0.15f});
                detail({x + i * 9.0f, 1.05f, z + 0.5f}, {3, 0.65f, 0.15f}, {0.48f, 0.29f, 0.15f});
            }
    }
    // Dashed road centres and zebra crossings make the grid readable.
    for (int road = -2; road <= 2; ++road)
    {
        float r = road * 60.0f;
        for (int d = -170; d <= 170; d += 10)
        {
            if (std::abs(d % 60) < 12) continue;
            detail({r, 0.018f, float(d)}, {0.16f, 0.025f, 4}, {0.85f, 0.73f, 0.35f});
            detail({float(d), 0.018f, r}, {4, 0.025f, 0.16f}, {0.85f, 0.73f, 0.35f});
        }
        for (int cross = -2; cross <= 2; ++cross)
            for (int stripe = -3; stripe <= 3; ++stripe)
                for (int side : {-1, 1})
                {
                    detail({r + stripe * 1.8f, 0.02f, cross * 60.0f + side * 6.5f}, {0.85f, 0.03f, 2.6f}, {0.83f, 0.84f, 0.78f});
                    detail({r + side * 6.5f, 0.02f, cross * 60.0f + stripe * 1.8f}, {2.6f, 0.03f, 0.85f}, {0.83f, 0.84f, 0.78f});
                }
    }

}

void World::addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color)
{
    glm::vec3 center = centerOnGround + glm::vec3(0.0f, size.y * 0.5f, 0.0f);
    m_boxes.push_back({ center, size, color });
    m_colliders.push_back(CollisionBox::fromCenterHalf(center, size * 0.5f));
}

void World::addRamp(const glm::vec3& footprintCenter, const glm::vec2& footprintSize, bool alongX,
                     float lowHeight, float highHeight, const glm::vec3& color)
{
    m_ramps.push_back({ footprintCenter, footprintSize, alongX, lowHeight, highHeight, color });
}

bool World::collides(const CollisionBox& box) const
{
    for (const CollisionBox& collider : m_colliders)
        if (box.intersects(collider))
            return true;
    for (const Ramp& ramp : m_ramps)
    {
        glm::vec3 size(ramp.footprintSize.x, ramp.highHeight - ramp.lowHeight, ramp.footprintSize.y);
        CollisionBox volume = CollisionBox::fromCenterHalf(
            {ramp.footprintCenter.x, (ramp.lowHeight + ramp.highHeight) * 0.5f, ramp.footprintCenter.z},
            size * 0.5f, ramp.alongX ? 90.0f : 0.0f);
        if (!box.intersects(volume)) continue;
        float halfX = ramp.alongX ? ramp.footprintSize.y * 0.5f : ramp.footprintSize.x * 0.5f;
        float halfZ = ramp.alongX ? ramp.footprintSize.x * 0.5f : ramp.footprintSize.y * 0.5f;
        float x = std::clamp(box.center.x, ramp.footprintCenter.x - halfX, ramp.footprintCenter.x + halfX);
        float z = std::clamp(box.center.z, ramp.footprintCenter.z - halfZ, ramp.footprintCenter.z + halfZ);
        auto surface = ramp.heightAt(x, z);
        if (surface && *surface > box.center.y - box.half.y + MAX_STEP_UP)
            return true; // block the tall end and sides; allow gradual uphill steps
    }
    return false;
}

float World::groundHeightAt(float x, float z, float maxHeight) const
{
    float height = 0.0f; // flat asphalt floor

    for (const StaticBox& box : m_boxes)
    {
        float halfX = box.size.x * 0.5f;
        float halfZ = box.size.z * 0.5f;
        bool withinFootprint = x >= box.center.x - halfX && x <= box.center.x + halfX &&
                               z >= box.center.z - halfZ && z <= box.center.z + halfZ;
        if (withinFootprint && box.center.y + box.size.y * 0.5f <= maxHeight)
            height = std::max(height, box.center.y + box.size.y * 0.5f);
    }

    for (const Ramp& ramp : m_ramps)
    {
        if (std::optional<float> h = ramp.heightAt(x, z))
            if (*h <= maxHeight) height = std::max(height, *h);
    }

    return height;
}

float World::supportHeight(const CollisionBox& actor, float maxHeight) const
{
    float height = groundHeightAt(actor.center.x, actor.center.z, maxHeight);
    for (const StaticBox& box : m_boxes)
    {
        float top = box.center.y + box.size.y * 0.5f;
        if (top > maxHeight || top <= height) continue;
        CollisionBox footprint = actor;
        footprint.center.y = box.center.y;
        footprint.half.y = box.size.y;
        if (footprint.intersects(CollisionBox::fromCenterHalf(box.center, box.size * 0.5f)))
            height = top;
    }
    return height;
}

glm::vec3 World::surfaceNormal(const glm::vec3& feet) const
{
    for (const Ramp& ramp : m_ramps)
        if (auto h = ramp.heightAt(feet.x, feet.z))
            if (std::abs(*h - feet.y) < 0.1f)
            {
                float slope = (ramp.highHeight - ramp.lowHeight) / ramp.footprintSize.y;
                return glm::normalize(ramp.alongX ? glm::vec3(-slope,1,0) : glm::vec3(0,1,-slope));
            }
    return {0,1,0};
}

std::vector<glm::vec3> World::trafficLoop(float x, float z)
{
    return {{x+12,0,z+3.5f}, {x+56.5f,0,z+3.5f}, {x+56.5f,0,z+56.5f},
            {x+3.5f,0,z+56.5f}, {x+3.5f,0,z+3.5f}};
}

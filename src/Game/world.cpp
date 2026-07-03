#include "world.hpp"

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
}

void World::addBox(const glm::vec3& centerOnGround, const glm::vec3& size, const glm::vec3& color)
{
    glm::vec3 center = centerOnGround + glm::vec3(0.0f, size.y * 0.5f, 0.0f);
    m_boxes.push_back({ center, size, color });
    m_colliders.push_back(AABB::fromCenterHalf(center, size * 0.5f));
}

void World::addCollider(const AABB& box)
{
    m_colliders.push_back(box);
}

bool World::collides(const AABB& box) const
{
    for (const AABB& collider : m_colliders)
        if (box.intersects(collider))
            return true;
    return false;
}

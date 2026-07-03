#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>

#include "aabb.hpp"

class Input;
class Camera;
class World;

// The playable character: a cube for now, replaced by a real model later.
class Player
{
public:
    void update(const Input& input, const Camera& camera, const World& world, float dt);

    AABB aabb() const;

    glm::vec3 position{ 0.0f };            // feet position, y = ground level
    float yaw = 0.0f;                      // facing, degrees around Y
    glm::vec3 size{ 0.6f, 1.8f, 0.6f };

private:
    float m_walkSpeed = 4.0f;
    float m_runSpeed = 9.0f;
};

#endif

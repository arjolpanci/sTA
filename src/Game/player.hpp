#ifndef PLAYER_H
#define PLAYER_H

#include <functional>
#include <glm/glm.hpp>

#include "aabb.hpp"

class Input;
class Camera;

// The playable character: a cube for now, replaced by a real model later.
class Player
{
public:
    // collides(box) reports whether box overlaps something the player should
    // stop for - Game builds it so Player never needs to know about World
    void update(const Input& input, const Camera& camera, float dt, const std::function<bool(const AABB&)>& collides);

    AABB aabb() const;

    glm::vec3 position{ 0.0f };            // feet position, y = ground level
    float yaw = 0.0f;                      // facing, degrees around Y
    glm::vec3 size{ 0.6f, 1.8f, 0.6f };

    // tunable parameters, exposed so a debug UI can adjust them live
    float walkSpeed = 4.0f;
    float runSpeed = 9.0f;
};

#endif

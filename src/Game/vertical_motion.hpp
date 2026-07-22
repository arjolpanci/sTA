#ifndef VERTICAL_MOTION_H
#define VERTICAL_MOTION_H

#include <algorithm>

// One physical constant, shared by every actor - gravity isn't something to
// tune per object.
constexpr float GRAVITY = 28.0f;
constexpr float TERMINAL_VELOCITY = -40.0f;

// Shared Y-axis physics for anything affected by gravity. This only
// integrates a velocity; each actor still owns its own position and
// collision handling - move position.y by step()'s result, test collision
// exactly like it already does for X/Z, then call land()/bonkHead() on a
// hit - so this stays a small integrator instead of a parallel collision
// system that has to know about AABBs or World.
struct VerticalMotion
{
    float velocity = 0.0f;
    bool grounded = true;

    // advances velocity under gravity and returns how far to move this frame
    float step(float dt)
    {
        velocity = std::max(velocity - GRAVITY * dt, TERMINAL_VELOCITY);
        return velocity * dt;
    }

    void jump(float jumpSpeed)
    {
        if (grounded)
        {
            velocity = jumpSpeed;
            grounded = false;
        }
    }

    void land() { velocity = 0.0f; grounded = true; }
    void bonkHead() { velocity = 0.0f; } // hit a ceiling while rising; still airborne
};

#endif

#ifndef VERTICAL_MOTION_H
#define VERTICAL_MOTION_H

#include <algorithm>
#include <functional>
#include <cmath>
#include <glm/glm.hpp>

// One physical constant, shared by every actor - gravity isn't something to
// tune per object.
constexpr float GRAVITY = 28.0f;
constexpr float TERMINAL_VELOCITY = -40.0f;

// A ground drop bigger than this in one frame means an edge was walked off
// (a rooftop, not a ramp) - real falling kicks in instead of smoothly
// sliding down to match it. Comfortably bigger than any per-frame height
// change a reasonable ramp produces, comfortably smaller than the shortest
// building.
constexpr float MAX_STEP_DOWN = 0.45f;
constexpr float MAX_STEP_UP = 0.45f;

struct VerticalMotion
{
    float velocity = 0.0f;
    bool grounded = true;

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

// Resolves one frame of Y-axis motion for an actor at (positionY), given
// groundY (World::groundHeightAt() at the actor's XZ - flat ground, a ramp,
// or a rooftop) and collidesHere() (a full CollisionBox test the caller provides,
// used to resolve contacts, since groundY alone cannot express bumping into
// a ceiling or landing on another actor).
//
// While grounded and the ground doesn't drop more than MAX_STEP_DOWN this
// frame, position snaps straight to groundY - this is what makes walking up
// *and down* a ramp look smooth instead of the actor falling frame-by-frame
// until gravity catches up with a receding slope. A bigger drop (an actual
// edge) releases it into normal gravity, exactly like walking off a ledge.
inline void resolveVerticalMotion(VerticalMotion& motion, float& positionY, float dt, float groundY,
                                   const std::function<bool()>& collidesHere)
{
    if (motion.grounded)
    {
        if (positionY - groundY <= MAX_STEP_DOWN && groundY - positionY <= MAX_STEP_UP)
        {
            positionY = groundY;
            motion.velocity = 0.0f;
            return;
        }
        motion.grounded = false; // stepped off a real edge - fall for real
    }

    float previousY = positionY;
    motion.velocity = std::max(motion.velocity - GRAVITY * dt, TERMINAL_VELOCITY);
    float deltaY = motion.velocity * dt;
    positionY += deltaY;

    if (motion.velocity <= 0.0f && previousY >= groundY - 0.001f && positionY <= groundY)
    {
        positionY = groundY;
        motion.land();
    }
    else if (collidesHere())
    {
        positionY -= deltaY;
        if (deltaY <= 0.0f)
            motion.land();
        else
            motion.bonkHead();
    }
    else
    {
        motion.grounded = false;
    }
}


// Substeps prevent tunnelling at high driving speeds. Raise grounded actors
// before the horizontal collision check so low curbs and ramp exits are usable.
template<class Bounds, class Context>
bool moveHorizontal(glm::vec3& position, const glm::vec3& delta, bool grounded,
                    Bounds bounds, const Context& ctx)
{
    int steps = std::max(1, static_cast<int>(std::ceil(glm::length(delta) / 0.18f)));
    bool hit = false;
    for (int i = 0; i < steps; ++i)
        for (int axis : {0, 2})
        {
            glm::vec3 previous = position;
            position[axis] += delta[axis] / static_cast<float>(steps);
            float floor = ctx.groundHeightAt(position.x, position.z);
            if (grounded && floor > position.y && floor - position.y <= MAX_STEP_UP)
                position.y = floor;
            if (ctx.collides(bounds())) { position = previous; hit = true; }
        }
    return hit;
}

#endif

#ifndef ACTOR_H
#define ACTOR_H

#include <functional>

#include "collision_box.hpp"

class Input;
class Camera;
class Renderer;
class Mesh;

// Everything Actor::update() needs from the outside world, built fresh by
// Game every frame for every actor. Actors never reach into Game, World, or
// each other directly - that's what keeps Player/Vehicle/(future NPCs)
// decoupled from one another.
struct ActorContext
{
    const Input& input;
    const Camera& camera;
    bool controlled;                          // true only for the one actor currently receiving player input
    std::function<bool(const CollisionBox&)> collides; // world geometry + every other actor, precomputed for this actor
    std::function<glm::vec3(const glm::vec3&)> surfaceNormal;
    std::function<float(float, float)> groundHeightAt; // ground height at (x, z): flat floor, a rooftop, or a ramp
};

// Common interface for anything that lives in the world and needs a
// per-frame update, a draw call, and a collision box: today Player and
// Vehicle, later NPCs/traffic. Game holds these in one polymorphic list so
// adding a new actor type never means touching Game's update/render loops -
// only genuinely actor-specific interactions (entering a car, per-type
// debug panels) still reach for the concrete type.
class Actor
{
public:
    virtual ~Actor() = default;

    virtual void update(const ActorContext& ctx, float dt) = 0;
    virtual void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const = 0;
    virtual void renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const = 0;
    virtual CollisionBox collisionBox() const = 0;
};

#endif

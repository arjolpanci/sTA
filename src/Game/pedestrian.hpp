#ifndef PEDESTRIAN_H
#define PEDESTRIAN_H

#include <glm/glm.hpp>

#include "actor.hpp"
#include "Rendering/model_asset.hpp"
#include "animation_state.hpp"
#include "vertical_motion.hpp"
#include "waypoint_path.hpp"

// A simple animated NPC that walks a looping/patrolling waypoint
// path, sliding along obstacles the same way Player does. Never
// player-controlled, so it ignores ActorContext::controlled entirely.
class Pedestrian : public Actor
{
public:
    Pedestrian(const glm::vec3& startPosition, WaypointPath path, const glm::vec3& color, int modelIndex = 0);

    void update(const ActorContext& ctx, float dt) override;
    void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    void renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    CollisionBox collisionBox() const override;

    float walkSpeed = 2.0f;

private:
    glm::vec3 m_position;
    float m_yaw = 0.0f;
    glm::vec3 m_size{ 0.5f, 1.7f, 0.5f };
    glm::vec3 m_color;
    WaypointPath m_path;
    AnimationState m_animation;
    std::shared_ptr<const ModelAsset> m_model;
    VerticalMotion m_vertical;
};

#endif

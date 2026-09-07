#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>

#include "actor.hpp"
#include "vertical_motion.hpp"

// The playable character, rendered as an articulated box silhouette.
class Player : public Actor
{
public:
    void update(const ActorContext& ctx, float dt) override;
    void render(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    void renderShadow(Renderer& renderer, const Mesh& cubeMesh, bool controlled) const override;
    CollisionBox collisionBox() const override;

    void resetMotion() { m_vertical = VerticalMotion{}; m_swimming = false; }

    bool isSwimming() const { return m_swimming; }
    bool isGrounded() const { return m_vertical.grounded; }

    glm::vec3 position{ 0.0f };            // feet position, y = ground level (or higher, mid-jump)
    float yaw = 0.0f;                      // facing, degrees around Y
    glm::vec3 size{ 0.6f, 1.8f, 0.6f };

    // tunable parameters, exposed so a debug UI can adjust them live
    float walkSpeed = 4.0f;
    float runSpeed = 9.0f;
    float jumpSpeed = 9.0f;

private:
    bool m_swimming = false;
    float m_gait = 0.0f;
    VerticalMotion m_vertical;
};

#endif

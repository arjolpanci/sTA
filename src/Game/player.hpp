#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>

#include "actor.hpp"
#include "Rendering/model_asset.hpp"
#include "animation_state.hpp"
#include "vertical_motion.hpp"

// Player movement and animation share the fixed simulation timestep.
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
    AnimationState m_animation;
    std::shared_ptr<const ModelAsset> m_model=ModelAsset::load("resources/models/characters/player.glb");
    VerticalMotion m_vertical;
};

#endif

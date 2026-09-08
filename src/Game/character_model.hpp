#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include "Rendering/model_asset.hpp"
#include "Rendering/renderer.hpp"

inline void drawCharacter(Renderer& renderer, const ModelAsset& asset, const void* instance,
                          glm::vec3 feet, float height, float yaw,
                          const AnimationState& animation, bool shadow)
{
    auto root=glm::translate(glm::mat4(1),feet);
    root=glm::rotate(root,glm::radians(yaw),{0,1,0});
    root=glm::scale(root,glm::vec3(height));
    renderer.drawModel(asset,instance,root,animation,shadow);
}

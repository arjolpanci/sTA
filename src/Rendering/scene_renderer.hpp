#pragma once
#include <memory>
#include <vector>
#include "material.hpp"
#include "mesh.hpp"
class World;
class Renderer;
class Texture;

// Saved scenery is merged once into spatial/material batches, not thousands
// of individual draws. Original World boxes remain the physics/debug source.
class SceneRenderer
{
public:
    explicit SceneRenderer(const World& world);
    ~SceneRenderer();
    void draw(Renderer& renderer, const glm::vec3& camera) const;
    void drawShadow(Renderer& renderer, const glm::vec3& focus) const;
private:
    // Trees are drawn nearer than the buildings behind them: they are the
    // densest geometry in the scene and the least legible at distance.
    struct Batch { glm::vec3 center; float radius, distance; Material material; std::unique_ptr<Mesh> mesh; };
    std::vector<Batch> m_batches;
    // Tree bark and leaves cannot share a batch - they need different textures
    // and only the leaves are cut out - so the merge is per material, and the
    // textures those materials point at are owned here.
    std::vector<std::unique_ptr<Texture>> m_textures;
};

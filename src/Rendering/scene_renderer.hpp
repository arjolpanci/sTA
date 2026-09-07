#pragma once
#include <memory>
#include <vector>
#include "mesh.hpp"
class World;
class Renderer;

// Saved scenery is merged once into spatial/material batches, not thousands
// of individual draws. Original World boxes remain the physics/debug source.
class SceneRenderer
{
public:
    explicit SceneRenderer(const World& world);
    void draw(Renderer& renderer, const glm::vec3& camera) const;
    void drawShadow(Renderer& renderer, const glm::vec3& focus) const;
private:
    struct Batch { glm::vec3 center; float radius; bool facade; std::unique_ptr<Mesh> mesh; };
    std::vector<Batch> m_batches;
};

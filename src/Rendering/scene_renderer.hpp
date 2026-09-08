#pragma once
#include <memory>
#include <vector>
#include "material.hpp"
#include "mesh.hpp"
class World;
class Renderer;
class Texture;
class ModelAsset;

// Saved scenery is drawn two ways, by what it is rather than by where it is.
//
// Buildings are unique boxes, so they are merged once into spatial batches -
// thousands of individual draws would cost more than the memory does.
//
// Trees and street props are a couple of dozen models repeated a few thousand
// times, so they are instanced: one copy of each model's geometry, and a
// per-frame buffer of the matrices that survived culling. Merging those the
// way buildings are merged cost 960 MB of duplicated vertices and ten seconds
// of startup, which is what this class used to do.
//
// The original World boxes remain the physics/debug source either way.
class SceneRenderer
{
public:
    explicit SceneRenderer(const World& world);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void draw(Renderer& renderer, const glm::vec3& camera) const;
    void drawShadow(Renderer& renderer, const glm::vec3& focus) const;

private:
    struct Batch { glm::vec3 center; float radius, distance; Material material; std::unique_ptr<Mesh> mesh; };

    struct Instance { glm::mat4 matrix; glm::vec3 center; float radius; };
    struct Model {
        struct Surface { std::unique_ptr<Mesh> mesh; Material material; };
        std::vector<Surface> surfaces;
        std::vector<Instance> instances;
        float distance = 500;      // beyond this the model stops being drawn
        unsigned int buffer = 0;   // instance matrices, refilled per pass
    };

    // Fills a model's GPU buffer with the instances that pass the test and
    // returns how many there are; zero means nothing to draw.
    int gather(Model& model, Renderer& renderer, const glm::vec3& eye, bool shadow, float range) const;
    std::vector<Model::Surface> loadSurfaces(const ModelAsset& asset);

    std::vector<Batch> m_batches;
    mutable std::vector<Model> m_models;
    mutable std::vector<glm::mat4> m_visible; // scratch, reused every pass
    // Textures the instanced materials point at are owned here.
    std::vector<std::unique_ptr<Texture>> m_textures;
};

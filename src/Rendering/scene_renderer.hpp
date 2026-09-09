#pragma once
#include <memory>
#include <vector>
#include <map>
#include "Game/world.hpp"
#include "material.hpp"
#include "mesh.hpp"
class World;
class Renderer;
class Texture;
class ModelAsset;

// Metadata is resident; nearby building geometry is built under a frame budget.
class SceneRenderer
{
public:
    explicit SceneRenderer(const World& world);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void updateStreaming(Renderer& renderer, const glm::vec3& camera, const glm::vec3& focus);
    size_t residentChunks() const;
    size_t residentBytes() const;
    static constexpr size_t ResidentLimit = 96;
    void draw(Renderer& renderer, const glm::vec3& camera) const;
    void drawShadow(Renderer& renderer, const glm::vec3& focus) const;

private:
    struct Batch { glm::vec3 center; float radius, distance; Material material; std::unique_ptr<Mesh> mesh;
        std::vector<const StaticBox*> boxes; std::vector<const Marking*> markings; };
    void build(Batch& batch);
    const World& m_world;

    struct Instance { glm::mat4 matrix; glm::vec3 center; float radius; };
    struct Model {
        struct Surface { std::unique_ptr<Mesh> mesh; Material material; };
        std::vector<Surface> surfaces;
        std::map<std::pair<int,int>,std::vector<Instance>> cells;
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

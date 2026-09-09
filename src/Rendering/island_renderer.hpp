#pragma once
#include <memory>
#include <vector>
#include <future>
#include <tuple>
#include "mesh.hpp"
#include "shader.hpp"
class Terrain;
class Camera;
class Renderer;
class ShadowMap;
struct Lighting;

class IslandRenderer
{
public:
    explicit IslandRenderer(const Terrain& terrain);
    ~IslandRenderer();
    void updateStreaming(Renderer& renderer, const glm::vec3& eye, const glm::vec3& focus);
    size_t residentChunks() const;
    size_t residentBytes() const;
    static constexpr size_t ResidentLimit = 96;
    void drawShadow(Renderer& renderer, const glm::vec3& focus);
    void drawTerrain(const Camera& camera, float aspect, const glm::mat4& lightSpace,
                     const Lighting& lighting, const ShadowMap& shadows, bool enabled);
    void drawWater(const Camera& camera, float aspect, const Lighting& lighting, float time, float waveStrength);
private:
    struct Chunk { glm::vec3 center; float radius; int x,z, stride=8; std::unique_ptr<Mesh> coarse, mesh; };
    const Terrain& m_terrain;
    using BuiltTile=std::tuple<size_t,int,std::vector<float>>;
    std::future<std::vector<BuiltTile>> m_pending;
    void common(Shader& shader, const Camera& camera, float aspect);
    std::vector<Chunk> m_chunks;
    std::unique_ptr<Mesh> m_water;
    Shader m_terrainShader, m_waterShader;
    unsigned int m_terrainTexture = 0, m_roadTexture = 0;
    float m_extent, m_resolution, m_seaLevel;
};

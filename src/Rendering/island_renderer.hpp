#pragma once
#include <memory>
#include <vector>
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
    void drawShadow(Renderer& renderer, const glm::vec3& focus);
    void drawTerrain(const Camera& camera, float aspect, const glm::mat4& lightSpace,
                     const Lighting& lighting, const ShadowMap& shadows, bool enabled);
    void drawWater(const Camera& camera, float aspect, const Lighting& lighting, float time, float waveStrength);
private:
    struct Chunk { glm::vec3 center; float radius; std::unique_ptr<Mesh> mesh; };
    void common(Shader& shader, const Camera& camera, float aspect);
    std::vector<Chunk> m_chunks;
    std::unique_ptr<Mesh> m_water;
    Shader m_terrainShader, m_waterShader;
    unsigned int m_terrainTexture = 0;
    float m_extent, m_resolution, m_seaLevel;
};

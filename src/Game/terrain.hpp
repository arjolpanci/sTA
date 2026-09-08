#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Read-only baked heightfield. Runtime does no noise generation or road shaping.
class Terrain
{
public:
    explicit Terrain(const std::string& path = "resources/maps/island.bin");
    int resolution() const { return m_resolution; }
    float spacing() const { return m_spacing; }
    float extent() const { return (m_resolution - 1) * m_spacing; }
    float seaLevel() const { return m_seaLevel; }
    float heightAt(float x, float z) const;
    glm::vec3 normalAt(float x, float z) const;
    const std::vector<float>& heights() const { return m_heights; }
    const std::vector<float>& roads() const { return m_roads; }
    // Road coverage sampled finely enough to draw: see the loader for why the
    // terrain grid cannot carry it.
    const std::vector<unsigned char>& roadMask() const { return m_roadMask; }
    int roadMaskResolution() const { return m_roadMaskResolution; }
    // Triangles and collision use the same diagonal and interpolation.
    std::vector<float> vertices(int x0, int z0, int cells) const;
private:
    int m_resolution = 0;
    float m_spacing = 0, m_seaLevel = 0;
    std::vector<float> m_heights, m_roads;
    std::vector<unsigned char> m_roadMask;
    int m_roadMaskResolution = 0;
};

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// CPU-only glTF asset. Geometry is centered on the footprint, feet at Y=0,
// and normalized to one unit tall. Safe to load in headless physics tests.
class ModelAsset
{
public:
    // One glTF material's worth of geometry. Models that mix bark, leaves and
    // glass cannot be drawn as a single batch: the leaves need their own
    // texture and their own cutout, so geometry is grouped by material and the
    // renderer draws one surface at a time.
    struct Surface
    {
        glm::vec3 albedo{ 1.0f };  // base color factor, already in display space
        int baseColorImage = -1;   // index into images(), -1 = untextured
        int normalImage = -1;
        float alphaCutoff = 0.0f;  // >0 for glTF MASK/BLEND materials
        bool doubleSided = false;
        // Small palette atlases (Kenney's kits) are folded into vertex colors
        // at load time instead: their UVs address single texels, which bilinear
        // filtering would bleed across at runtime. Real texture maps are not.
        bool bakedColor = true;
    };
    struct ImageData { int width = 0, height = 0, channels = 0; std::vector<unsigned char> pixels; };

    explicit ModelAsset(const std::string& path, bool centerFootprint = true);
    ~ModelAsset();
    ModelAsset(const ModelAsset&) = delete;
    ModelAsset& operator=(const ModelAsset&) = delete;
    const std::string& cacheKey() const { return m_cacheKey; }
    glm::vec3 size() const;
    bool animated() const;
    size_t surfaceCount() const;
    const Surface& surface(size_t index) const;
    const ImageData& image(size_t index) const;
    // Same layouts as vertices()/skinVertices(), restricted to one surface.
    // Joint indices stay asset-global, so one skinPalette() still drives them all.
    std::vector<float> surfaceVertices(size_t surface, const std::string& clip = "", float time = 0,
        const std::string& previous = "", float previousTime = 0, float blend = 1) const;
    std::vector<float> surfaceSkinVertices(size_t surface) const;
    // Immutable bind vertices: position/normal/UV/color/joint indices/weights (19 floats).
    std::vector<float> skinVertices() const;
    // Seven vec4 texels per bone: position matrix, then inverse-transpose normal matrix.
    std::vector<glm::vec4> skinPalette(const std::string& clip, float time,
        const std::string& previous = "", float previousTime = 0, float blend = 1) const;
    std::vector<std::string> animations() const;
    bool hasAnimation(const std::string& clip) const;
    float duration(const std::string& clip) const;
    // Interleaved position, normal, UV, color. Clips accept the suffix after |/_.
    // blend mixes local bone transforms from previous to clip, before skinning.
    std::vector<float> vertices(const std::string& clip = "", float time = 0,
        const std::string& previous = "", float previousTime = 0, float blend = 1) const;
    static std::shared_ptr<const ModelAsset> load(const std::string& path);
private:
    std::vector<float> skinBindVertices(int surfaceFilter) const;
    struct Impl;
    std::unique_ptr<Impl> m;
    std::string m_cacheKey;
};

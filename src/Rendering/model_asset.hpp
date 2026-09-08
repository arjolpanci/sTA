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
    explicit ModelAsset(const std::string& path, bool centerFootprint = true);
    ~ModelAsset();
    ModelAsset(const ModelAsset&) = delete;
    ModelAsset& operator=(const ModelAsset&) = delete;
    glm::vec3 size() const;
    bool animated() const;
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
    struct Impl;
    std::unique_ptr<Impl> m;
};

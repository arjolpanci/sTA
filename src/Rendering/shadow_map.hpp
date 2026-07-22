#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>

// Owns the depth-only render target used for shadow mapping: a framebuffer
// with just a depth texture attached, no color. One static orthographic
// frustum sized to cover the whole map - this is a small, fixed-size level,
// not an open world, so there's no need for cascades or a frustum that
// follows the camera around.
class ShadowMap
{
public:
    explicit ShadowMap(int resolution = 2048);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    // the light's combined view-projection matrix, covering sceneRadius
    // around sceneCenter as seen from lightDir
    static glm::mat4 lightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter, float sceneRadius);

    void beginCapture();                                   // bind the FBO + viewport, clear depth
    void endCapture(int restoreWidth, int restoreHeight);   // rebind the default framebuffer + viewport

    void bindForSampling(int textureUnit) const;
    unsigned int depthTexture() const { return m_depthTexture; } // for a debug preview (ImGui::Image)

private:
    unsigned int m_FBO = 0;
    unsigned int m_depthTexture = 0;
    int m_resolution;
};

#endif

#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <glm/glm.hpp>

// Owns the depth-only render target used for shadow mapping: a framebuffer
// with just a depth texture attached, no color. One orthographic frustum,
// fitted around whatever the player is controlling rather than the whole map,
// which is what keeps a single map this sharp without cascades.
//
// Sampling goes through a comparison sampler object rather than the texture's
// own parameters, so the hardware does the depth test and gives bilinear PCF
// for free - while the plain texture stays sampleable by the debug preview,
// which cannot use a comparison sampler.
class ShadowMap
{
public:
    explicit ShadowMap(int resolution = 4096);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    // The light's combined view-projection matrix, covering sceneRadius around
    // sceneCenter as seen from lightDir. The centre is snapped to whole shadow
    // texels: without that, walking forwards slides the sampling grid under
    // every static edge and the whole scene crawls.
    static glm::mat4 lightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter,
                                      float sceneRadius, int resolution = 4096);

    // World size of one shadow texel, which is the unit a normal-offset bias
    // has to be expressed in.
    static float texelWorldSize(float sceneRadius, int resolution) { return 2.0f * sceneRadius / float(resolution); }
    int resolution() const { return m_resolution; }
    // Radius the last lightSpaceMatrix() call was fitted to, so shaders can be
    // told the texel size without every caller passing it along.
    static float fittedRadius();
    float texelWorld() const { return texelWorldSize(fittedRadius(), m_resolution); }

    void beginCapture();                                   // bind the FBO + viewport, clear depth
    void endCapture(int restoreWidth, int restoreHeight);   // rebind the default framebuffer + viewport

    void bindForSampling(int textureUnit) const;
    unsigned int depthTexture() const { return m_depthTexture; } // for a debug preview (ImGui::Image)

private:
    unsigned int m_FBO = 0;
    unsigned int m_depthTexture = 0;
    unsigned int m_sampler = 0;
    int m_resolution;
};

#endif

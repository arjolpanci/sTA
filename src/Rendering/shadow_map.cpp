#include "shadow_map.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

ShadowMap::ShadowMap(int resolution) : m_resolution(resolution)
{
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // outside the shadow frustum, sample as "far" (never in shadow) rather
    // than wrapping/repeating the edge of the map
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // The comparison state lives on a sampler object, not the texture, so the
    // debug preview can still read the raw depth through the same texture.
    glGenSamplers(1, &m_sampler);
    glSamplerParameteri(m_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(m_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(m_sampler, GL_TEXTURE_BORDER_COLOR, borderColor);
    glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(m_sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glDrawBuffer(GL_NONE); // depth-only: no color attachment to write
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ShadowMap::~ShadowMap()
{
    glDeleteSamplers(1, &m_sampler);
    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_depthTexture);
}

namespace { float g_fittedRadius = 95.0f; }
float ShadowMap::fittedRadius() { return g_fittedRadius; }

glm::mat4 ShadowMap::lightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter,
                                      float sceneRadius, int resolution)
{
    g_fittedRadius = sceneRadius;
    // A light straight overhead leaves lookAt without a usable up vector.
    const glm::vec3 up = std::abs(lightDir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightView = glm::lookAt(sceneCenter + lightDir * sceneRadius * 2.0f, sceneCenter, up);

    // Snap the centre to the texel grid *of this light's own basis*, so the
    // texels a static edge lands on do not change as the camera moves.
    const float texel = texelWorldSize(sceneRadius, resolution);
    glm::vec3 viewCenter = glm::vec3(lightView * glm::vec4(sceneCenter, 1.0f));
    const glm::vec2 snapped(std::floor(viewCenter.x / texel) * texel, std::floor(viewCenter.y / texel) * texel);
    lightView = glm::translate(glm::mat4(1.0f), glm::vec3(snapped - glm::vec2(viewCenter), 0.0f)) * lightView;

    glm::mat4 lightProj = glm::ortho(-sceneRadius, sceneRadius, -sceneRadius, sceneRadius, 1.0f, sceneRadius * 4.0f);
    return lightProj * lightView;
}

void ShadowMap::beginCapture()
{
    glViewport(0, 0, m_resolution, m_resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    // rendering back faces into the depth buffer instead of front faces is a
    // cheap, standard fix for shadow acne on closed convex shapes (our boxes)
    glCullFace(GL_FRONT);
}

void ShadowMap::endCapture(int restoreWidth, int restoreHeight)
{
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, restoreWidth, restoreHeight);
}

void ShadowMap::bindForSampling(int textureUnit) const
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glBindSampler(textureUnit, m_sampler);
}

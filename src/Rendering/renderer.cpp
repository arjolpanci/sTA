#include "renderer.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "camera.hpp"
#include "mesh.hpp"
#include "shadow_map.hpp"
#include "texture.hpp"

Renderer::Renderer()
    : m_shader("resources/shaders/basic.vert", "resources/shaders/basic.frag"),
      m_shadowShader("resources/shaders/shadow.vert", "resources/shaders/shadow.frag")
{
}

void Renderer::beginShadowPass(const glm::mat4& lightSpaceMatrix)
{
    m_shadowShader.use();
    m_shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
}

void Renderer::drawShadow(const Mesh& mesh, const glm::mat4& model)
{
    m_shadowShader.setMat4("model", model);
    mesh.draw();
}

void Renderer::beginFrame(const Camera& camera, float aspect, const glm::mat4& lightSpaceMatrix,
                           const glm::vec3& lightDir, const ShadowMap& shadowMap, bool shadowsEnabled)
{
    m_shader.use();
    m_shader.setMat4("view", camera.viewMatrix());
    m_shader.setMat4("projection", glm::perspective(glm::radians(60.0f), aspect, 0.1f, 400.0f));
    m_shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    m_shader.setVec3("lightDir", lightDir);
    m_shader.setVec3("viewPos", camera.position());
    m_shader.setBool("shadowsEnabled", shadowsEnabled);
    m_shader.setInt("tex", 0);
    m_shader.setInt("shadowMap", 1);
    shadowMap.bindForSampling(1);
}

void Renderer::draw(const Mesh& mesh, const glm::mat4& model, const Material& material)
{
    m_shader.setMat4("model", model);
    m_shader.setVec3("color", material.albedo);
    m_shader.setFloat("shininess", material.shininess);
    m_shader.setBool("useTexture", material.albedoMap != nullptr);
    if (material.albedoMap)
        material.albedoMap->bind(0);
    mesh.draw();
}

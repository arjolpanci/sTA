#include "renderer.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "camera.hpp"
#include "mesh.hpp"
#include "texture.hpp"

Renderer::Renderer()
    : m_shader("resources/shaders/basic.vert", "resources/shaders/basic.frag")
{
}

void Renderer::beginFrame(const Camera& camera, float aspect)
{
    m_shader.use();
    m_shader.setMat4("view", camera.viewMatrix());
    m_shader.setMat4("projection", glm::perspective(glm::radians(60.0f), aspect, 0.1f, 400.0f));
    m_shader.setInt("tex", 0);
}

void Renderer::draw(const Mesh& mesh, const glm::mat4& model, const glm::vec3& color, const Texture* texture)
{
    m_shader.setMat4("model", model);
    m_shader.setVec3("color", color);
    m_shader.setBool("useTexture", texture != nullptr);
    if (texture)
        texture->bind(0);
    mesh.draw();
}

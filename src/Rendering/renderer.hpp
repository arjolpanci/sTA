#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include "shader.hpp"

class Mesh;
class Texture;
class Camera;

// Thin draw-call layer: one shader, view/projection set once per frame,
// then draw shared meshes with per-object model matrix + color.
class Renderer
{
public:
    Renderer();

    void beginFrame(const Camera& camera, float aspect);
    void draw(const Mesh& mesh, const glm::mat4& model, const glm::vec3& color, const Texture* texture = nullptr);

private:
    Shader m_shader;
};

#endif

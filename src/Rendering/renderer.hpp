#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include "shader.hpp"
#include "material.hpp"

class Mesh;
class Camera;
class ShadowMap;

// Thin draw-call layer: two shaders (a depth-only one for the shadow pass,
// a lit one for the main pass), view/projection/light set once per frame,
// then draw shared meshes with a per-object model matrix + Material.
class Renderer
{
public:
    Renderer();

    // shadow pass: render depth only, from the light's point of view.
    // Bracket calls to drawShadow() with ShadowMap::beginCapture()/endCapture().
    void beginShadowPass(const glm::mat4& lightSpaceMatrix);
    void drawShadow(const Mesh& mesh, const glm::mat4& model);

    // main pass: full shading, sampling the shadow map captured just before.
    // shadowsEnabled only gates the shader's use of the shadow map - the
    // capture itself should still run every frame regardless, so the map
    // never contains stale data from whenever it was last disabled
    void beginFrame(const Camera& camera, float aspect, const glm::mat4& lightSpaceMatrix,
                     const glm::vec3& lightDir, const ShadowMap& shadowMap, bool shadowsEnabled);
    void draw(const Mesh& mesh, const glm::mat4& model, const Material& material);

private:
    Shader m_shader;       // basic.vert/frag - the main lit shader
    Shader m_shadowShader; // shadow.vert/frag - depth-only
};

#endif

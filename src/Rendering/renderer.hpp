#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <map>
#include "Game/animation_state.hpp"
#include "shader.hpp"
#include "material.hpp"
#include "frustum.hpp"

class Mesh;
class ModelAsset;
class Camera;
class ShadowMap;

// Thin draw-call layer: two shaders (a depth-only one for the shadow pass,
// a lit one for the main pass), view/projection/light set once per frame,
// then draw shared meshes with a per-object model matrix + Material.
class Renderer
{
public:
    Renderer();
    ~Renderer();
    struct Stats { int poseUpdates=0, modelDraws=0, culledModels=0; size_t paletteBytes=0; };
    Stats stats;
    void setCamera(const Camera& camera,float aspect);
    bool visibleSphere(const glm::vec3& center,float radius,bool shadow=false) const;

    void drawModel(const ModelAsset& asset, const void* instance, const glm::mat4& model,
                   const AnimationState& animation, bool shadow);


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
    struct Pose {
        const ModelAsset* asset=nullptr;
        AnimationState animation;
        unsigned int buffer=0, texture=0;
        ~Pose();
    };
    std::map<const ModelAsset*,std::unique_ptr<Mesh>> m_models;
    std::map<const void*,Pose> m_poses;
    Shader m_shader, m_shadowShader, m_skinShader, m_skinShadowShader;
    Shader* m_active=nullptr;
    Frustum m_cameraFrustum, m_shadowFrustum;
    glm::vec3 m_eye{0};
    void use(Shader& shader);

};

#endif

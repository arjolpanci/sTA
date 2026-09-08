#ifndef RENDERER_H
#define RENDERER_H

#include <glm/glm.hpp>
#include <memory>
#include <map>
#include <vector>
#include "Game/animation_state.hpp"
#include "shader.hpp"
#include "material.hpp"
#include "frustum.hpp"

class Mesh;
class ModelAsset;
class Texture;
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
    // Cut-out/double-sided geometry has to be captured the same way it is
    // shaded, or its depth silhouette is not the shape that gets drawn.
    void drawShadow(const Mesh& mesh, const glm::mat4& model, const Material& material);

    // Instanced counterparts: one draw for every copy of a model in the
    // buffer, which is how repeated scenery is drawn in both passes.
    void drawInstanced(const Mesh& mesh, unsigned int instances, int count, const Material& material);
    void drawShadowInstanced(const Mesh& mesh, unsigned int instances, int count, const Material& material);

    // main pass: full shading, sampling the shadow map captured just before.
    // shadowsEnabled only gates the shader's use of the shadow map - the
    // capture itself should still run every frame regardless, so the map
    // never contains stale data from whenever it was last disabled
    void beginFrame(const Camera& camera, float aspect, const glm::mat4& lightSpaceMatrix,
                     const glm::vec3& lightDir, const ShadowMap& shadowMap, bool shadowsEnabled);
    void draw(const Mesh& mesh, const glm::mat4& model, const Material& material);

private:
    // One GPU mesh per glTF material, plus the textures those materials name.
    struct Model {
        struct Surface { std::unique_ptr<Mesh> mesh; Material material; };
        std::vector<Surface> surfaces;
        std::map<int,std::unique_ptr<Texture>> textures;
    };
    struct Pose {
        const ModelAsset* asset=nullptr;
        AnimationState animation;
        unsigned int buffer=0, texture=0;
        ~Pose();
    };
    std::map<const ModelAsset*,Model> m_models;
    std::map<const void*,Pose> m_poses;
    Shader m_shader, m_shadowShader, m_skinShader, m_skinShadowShader, m_instanceShader, m_instanceShadowShader;
    Shader* m_active=nullptr;
    Frustum m_cameraFrustum, m_shadowFrustum;
    glm::vec3 m_eye{0};
    bool m_culling=true;
    void use(Shader& shader);
    void setCulling(bool enabled);
    // The two passes bind the same material state through different shaders.
    void applyCutout(Shader& shader, const Material& material);
    void bindMaterial(Shader& shader, const Material& material, bool shadowPass);
    const Model& modelFor(const ModelAsset& asset);

};

#endif

#include "renderer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#include "camera.hpp"
#include "mesh.hpp"
#include "shadow_map.hpp"
#include "texture.hpp"
#include "model_asset.hpp"

Renderer::Renderer()
    : m_shader("resources/shaders/basic.vert", "resources/shaders/basic.frag"),
      m_shadowShader("resources/shaders/shadow.vert", "resources/shaders/shadow.frag"),
      m_skinShader("resources/shaders/skinned.vert", "resources/shaders/basic.frag"),
      m_skinShadowShader("resources/shaders/skinned_shadow.vert", "resources/shaders/shadow.frag")
{
}

void Renderer::use(Shader& shader) {
    if(m_active!=&shader){shader.use();m_active=&shader;}
}
void Renderer::setCulling(bool enabled) {
    if(m_culling==enabled)return;
    m_culling=enabled;
    if(enabled)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
}
void Renderer::applyCutout(Shader& shader,const Material& material) {
    shader.setBool("useTexture",material.albedoMap!=nullptr);
    shader.setFloat("alphaCutoff",material.alphaCutoff);
    if(material.albedoMap)material.albedoMap->bind(0);
    setCulling(!material.doubleSided);
}
void Renderer::beginShadowPass(const glm::mat4& lightSpaceMatrix)
{
    m_shadowFrustum=Frustum(lightSpaceMatrix);
    m_active=nullptr;setCulling(true);
    for(auto* shader:{&m_shadowShader,&m_skinShadowShader}) {
        use(*shader);shader->setMat4("lightSpaceMatrix",lightSpaceMatrix);
    }
    m_skinShadowShader.setInt("skinPalette",2);
}
void Renderer::drawShadow(const Mesh& mesh,const glm::mat4& model)
{
    drawShadow(mesh,model,Material{});
}
void Renderer::drawShadow(const Mesh& mesh,const glm::mat4& model,const Material& material)
{
    use(m_shadowShader);m_shadowShader.setMat4("model",model);
    // ShadowMap::beginCapture() culls front faces, which would erase a
    // single-sided leaf card entirely whenever the light faces it.
    applyCutout(m_shadowShader,material);m_shadowShader.setInt("tex",0);
    mesh.draw();
}
void Renderer::beginFrame(const Camera& camera,float aspect,const glm::mat4& lightSpaceMatrix,
                           const glm::vec3& lightDir,const ShadowMap& shadowMap,bool shadowsEnabled)
{
    setCamera(camera,aspect);
    // Terrain/water own their shaders, so reset the active-program cache at pass boundaries.
    m_active=nullptr;
    for(auto* shader:{&m_shader,&m_skinShader}) {
        use(*shader);
        shader->setMat4("view",camera.viewMatrix());
        shader->setMat4("projection",glm::perspective(glm::radians(60.0f),aspect,.1f,3000.0f));
        shader->setMat4("lightSpaceMatrix",lightSpaceMatrix);
        shader->setVec3("lightDir",lightDir);shader->setVec3("viewPos",camera.position());
        shader->setBool("shadowsEnabled",shadowsEnabled);
        shader->setInt("tex",0);shader->setInt("shadowMap",1);shader->setInt("normalMap",3);
    }
    setCulling(true);
    m_skinShader.setInt("skinPalette",2);
    shadowMap.bindForSampling(1);
}
void Renderer::draw(const Mesh& mesh,const glm::mat4& model,const Material& material)
{
    use(m_shader);m_shader.setMat4("model",model);m_shader.setVec3("color",material.albedo);
    m_shader.setFloat("shininess",material.shininess);m_shader.setBool("facade",material.facade);
    applyCutout(m_shader,material);
    m_shader.setBool("useNormalMap",material.normalMap!=nullptr);
    if(material.normalMap)material.normalMap->bind(3);
    // Leave unit 0 current: the terrain and water shaders bind their own
    // textures there without selecting a unit first.
    glActiveTexture(GL_TEXTURE0);
    mesh.draw();
}
Renderer::Pose::~Pose(){if(texture)glDeleteTextures(1,&texture);if(buffer)glDeleteBuffers(1,&buffer);}
Renderer::~Renderer()=default;
void Renderer::drawModel(const ModelAsset& asset,const void* instance,const glm::mat4& model,
                         const AnimationState& animation,bool shadow)
{
    const glm::vec3 center=model*glm::vec4(0,.5f,0,1);
    float scale=std::max({glm::length(glm::vec3(model[0])),glm::length(glm::vec3(model[1])),glm::length(glm::vec3(model[2]))});
    // A generous character sphere covers arm swings, jumping and death clips.
    float radius=(asset.animated()?1.7f:glm::length(asset.size())*.5f)*scale;
    if(!visibleSphere(center,radius,shadow)){++stats.culledModels;return;}
    ++stats.modelDraws;
    auto& mesh=m_models[&asset];
    if(!mesh)mesh=std::make_unique<Mesh>(asset.animated()?asset.skinVertices():asset.vertices(),true,asset.animated());
    if(!asset.animated()) {
        if(shadow)drawShadow(*mesh,model);else draw(*mesh,model,Material{});
        return;
    }
    auto sampled=animation;
    if(sampled.blend>=1) {
        // Only the pose is throttled: movement and physics still run at 60 Hz.
        // Full rate within 25 m; 30 Hz farther out; 10 Hz beyond 75 m.
        float distance=glm::length(center-m_eye);
        float interval=distance>75?.1f:distance>25?1.0f/30:0;
        if(interval>0)sampled.time=std::floor(sampled.time/interval)*interval;
        sampled.previous.clear();sampled.previousTime=0;
    }
    const auto& state=sampled;
    auto& pose=m_poses[instance];
    if(!pose.buffer || pose.asset!=&asset || pose.animation.clip!=state.clip || pose.animation.previous!=state.previous ||
       pose.animation.time!=state.time || pose.animation.previousTime!=state.previousTime || pose.animation.blend!=state.blend) {
        auto palette=asset.skinPalette(state.clip,state.time,state.previous,state.previousTime,state.blend);
        ++stats.poseUpdates;stats.paletteBytes+=palette.size()*sizeof(glm::vec4);
        if(!pose.buffer) {glGenBuffers(1,&pose.buffer);glGenTextures(1,&pose.texture);}
        glBindBuffer(GL_TEXTURE_BUFFER,pose.buffer);
        glBufferData(GL_TEXTURE_BUFFER,palette.size()*sizeof(glm::vec4),palette.data(),GL_STREAM_DRAW);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_BUFFER,pose.texture);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,pose.buffer);
        pose.asset=&asset;pose.animation=state;
    }
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_BUFFER,pose.texture);
    auto& shader=shadow?m_skinShadowShader:m_skinShader;use(shader);shader.setMat4("model",model);
    if(!shadow) {
        shader.setVec3("color",glm::vec3(1));shader.setFloat("shininess",0);
        shader.setBool("facade",false);shader.setBool("useTexture",false);shader.setBool("useNormalMap",false);
        shader.setFloat("alphaCutoff",0);
    }
    mesh->draw();
}

bool Renderer::visibleSphere(const glm::vec3& center,float radius,bool shadow) const {
    return (shadow?m_shadowFrustum:m_cameraFrustum).intersectsSphere(center,radius);
}

void Renderer::setCamera(const Camera& camera,float aspect) {
    m_eye=camera.position();
    m_cameraFrustum=Frustum(glm::perspective(glm::radians(60.0f),aspect,.1f,3000.0f)*camera.viewMatrix());
}

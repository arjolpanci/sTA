#include "island_renderer.hpp"
#include "Game/terrain.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "shadow_map.hpp"
#include "sky.hpp"
#include "frustum.hpp"
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

IslandRenderer::IslandRenderer(const Terrain& terrain)
    : m_terrainShader("resources/shaders/basic.vert", "resources/shaders/terrain.frag"),
      m_waterShader("resources/shaders/water.vert", "resources/shaders/water.frag"),
      m_extent(terrain.extent()), m_resolution(float(terrain.resolution())), m_seaLevel(terrain.seaLevel())
{
    for (int z=0; z<terrain.resolution()-1; z+=64)
        for (int x=0; x<terrain.resolution()-1; x+=64)
        {
            auto vertices=terrain.vertices(x,z,64);
            glm::vec3 min(std::numeric_limits<float>::max()),max(-std::numeric_limits<float>::max());
            for(size_t i=0;i<vertices.size();i+=8){glm::vec3 p(vertices[i],vertices[i+1],vertices[i+2]);min=glm::min(min,p);max=glm::max(max,p);}
            m_chunks.push_back({(min+max)*.5f,glm::length(max-min)*.5f,std::make_unique<Mesh>(vertices)});
        }
    std::vector<float> data;
    data.reserve(terrain.heights().size()*2);
    for (size_t i=0; i<terrain.heights().size(); ++i) {data.push_back(terrain.heights()[i]); data.push_back(terrain.roads()[i]);}
    glGenTextures(1,&m_terrainTexture);
    glBindTexture(GL_TEXTURE_2D,m_terrainTexture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,terrain.resolution(),terrain.resolution(),0,GL_RG,GL_FLOAT,data.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenTextures(1,&m_roadTexture);
    glBindTexture(GL_TEXTURE_2D,m_roadTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8,terrain.roadMaskResolution(),terrain.roadMaskResolution(),0,GL_RED,GL_UNSIGNED_BYTE,terrain.roadMask().data());
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

    // A finite mesh extends beyond the fog/far plane from every reachable coast.
    std::vector<float> water;
    auto vertex=[&](float x,float z) {water.insert(water.end(),{x,0,z,0,1,0,0,0});};
    for (int z=-4096; z<4096; z+=32) for(int x=-4096; x<4096; x+=32) {
        vertex(float(x),float(z)); vertex(float(x),float(z+32)); vertex(float(x+32),float(z));
        vertex(float(x+32),float(z)); vertex(float(x),float(z+32)); vertex(float(x+32),float(z+32));
    }
    m_water=std::make_unique<Mesh>(water);
}
IslandRenderer::~IslandRenderer() {glDeleteTextures(1,&m_terrainTexture);glDeleteTextures(1,&m_roadTexture);}
void IslandRenderer::common(Shader& shader,const Camera& camera,float aspect)
{
    shader.use();
    shader.setMat4("model",glm::mat4(1));
    shader.setMat4("view",camera.viewMatrix());
    shader.setMat4("projection",glm::perspective(glm::radians(60.0f),aspect,.1f,3000.0f));
    shader.setVec3("viewPos",camera.position());
    shader.setFloat("terrainExtent",m_extent);
    shader.setFloat("terrainResolution",m_resolution);
    shader.setFloat("seaLevel",m_seaLevel);
    shader.setInt("terrainData",0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,m_terrainTexture);
}
void IslandRenderer::drawShadow(Renderer& renderer,const glm::vec3& focus)
{
    for(const auto& chunk:m_chunks)
        if (renderer.visibleSphere(chunk.center,chunk.radius,true) && glm::length(glm::vec2(chunk.center.x-focus.x,chunk.center.z-focus.z))<360)
            renderer.drawShadow(*chunk.mesh,glm::mat4(1));
}
void IslandRenderer::drawTerrain(const Camera& camera,float aspect,const glm::mat4& lightSpace,
                                  const Lighting& lighting,const ShadowMap& shadows,bool enabled)
{
    common(m_terrainShader,camera,aspect);
    m_terrainShader.setMat4("lightSpaceMatrix",lightSpace);
    m_terrainShader.setVec3("lightDir",lighting.direction);
    m_terrainShader.setVec3("sunColor",lighting.sunColor);
    m_terrainShader.setVec3("ambientColor",lighting.ambientColor);
    m_terrainShader.setVec3("fogColor",lighting.fogColor);
    m_terrainShader.setBool("shadowsEnabled",enabled);
    m_terrainShader.setInt("roadMask",2);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,m_roadTexture); glActiveTexture(GL_TEXTURE0);
    m_terrainShader.setInt("shadowMap",1); m_terrainShader.setFloat("shadowTexelWorld",shadows.texelWorld());
    shadows.bindForSampling(1);
    Frustum frustum(glm::perspective(glm::radians(60.0f),aspect,.1f,3000.0f)*camera.viewMatrix());
    for(const auto& chunk:m_chunks)
        if(frustum.intersectsSphere(chunk.center,chunk.radius) && glm::length(glm::vec2(chunk.center.x-camera.position().x,chunk.center.z-camera.position().z))<2100)
            chunk.mesh->draw();
}
void IslandRenderer::drawWater(const Camera& camera,float aspect,const Lighting& lighting,float time,float waveStrength)
{
    common(m_waterShader,camera,aspect);
    m_waterShader.setVec3("lightDir",lighting.direction);
    m_waterShader.setVec3("sunColor",lighting.sunColor);
    m_waterShader.setVec3("ambientColor",lighting.ambientColor);
    m_waterShader.setVec3("fogColor",lighting.fogColor);
    m_waterShader.setVec3("waterOrigin", {std::floor(camera.position().x/32)*32, 0, std::floor(camera.position().z/32)*32});
    m_waterShader.setFloat("time",time);
    m_waterShader.setFloat("waveStrength",waveStrength);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    m_water->draw();
    glEnable(GL_CULL_FACE); glDisable(GL_BLEND);
}

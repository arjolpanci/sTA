#include "island_renderer.hpp"
#include "Game/terrain.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "shadow_map.hpp"
#include "sky.hpp"
#include "frustum.hpp"
#include <limits>
#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

IslandRenderer::IslandRenderer(const Terrain& terrain)
    : m_terrain(terrain), m_terrainShader("resources/shaders/basic.vert", "resources/shaders/terrain.frag"),
      m_waterShader("resources/shaders/water.vert", "resources/shaders/water.frag"),
      m_extent(terrain.extent()), m_resolution(float(terrain.resolution())), m_seaLevel(terrain.seaLevel())
{
    // Only a lightweight horizon exists initially. Full-resolution detail is
    // uploaded under a per-frame budget and has a fixed resident ceiling.
    for (int z=0; z<terrain.resolution()-1; z+=32)
        for (int x=0; x<terrain.resolution()-1; x+=32) {
            float low=1e9f,high=-1e9f;
            for(int j=z;j<=z+32;++j) for(int i=x;i<=x+32;++i) {
                const float h=terrain.heights()[j*terrain.resolution()+i];
                low=std::min(low,h); high=std::max(high,h);
            }
            low-=40; // skirts participate in conservative culling bounds
            glm::vec3 center((x+16)*terrain.spacing()-terrain.extent()/2,(low+high)/2,
                             (z+16)*terrain.spacing()-terrain.extent()/2);
            float radius=glm::length(glm::vec3(16*terrain.spacing(),(high-low)/2,16*terrain.spacing()));
            m_chunks.push_back({center,radius,x,z,8,std::make_unique<Mesh>(terrain.vertices(x,z,32,8,true)),nullptr});
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
size_t IslandRenderer::residentBytes() const
{
    size_t bytes=0;for(const auto& c:m_chunks) if(c.mesh) bytes+=size_t(c.mesh->vertexCount())*8*sizeof(float);
    return bytes;
}
size_t IslandRenderer::residentChunks() const
{
    return size_t(std::count_if(m_chunks.begin(),m_chunks.end(),[](const Chunk& c){return bool(c.mesh);}));
}
void IslandRenderer::updateStreaming(Renderer& renderer,const glm::vec3& eye,const glm::vec3& focus)
{
    struct Request {size_t index;float priority;};
    std::vector<Request> wanted;
    for(size_t i=0;i<m_chunks.size();++i) {
        auto& c=m_chunks[i];
        const float distance=glm::length(glm::vec2(c.center.x-eye.x,c.center.z-eye.z));
        const float playerDistance=glm::length(glm::vec2(c.center.x-focus.x,c.center.z-focus.z));
        if(distance>1150+c.radius && playerDistance>320+c.radius) c.mesh.reset();
        if(playerDistance<280+c.radius || (distance<950+c.radius && renderer.visibleSphere(c.center,c.radius)))
            wanted.push_back({i,std::min(distance,playerDistance+150)});
    }
    std::sort(wanted.begin(),wanted.end(),[](const Request& a,const Request& b){return a.priority<b.priority;});
    if(wanted.size()>ResidentLimit) wanted.resize(ResidentLimit);
    // Evict cached tiles before admitting replacements; camera turns cannot
    // temporarily double the memory footprint.
    size_t count=residentChunks();
    for(size_t i=0;i<m_chunks.size() && count>=ResidentLimit;++i)
        if(m_chunks[i].mesh && std::none_of(wanted.begin(),wanted.end(),[i](const Request& r){return r.index==i;})) {
            m_chunks[i].mesh.reset(); --count;
        }
    if(m_pending.valid()) {
        if(m_pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        for(auto& result:m_pending.get()) {
            size_t index=std::get<0>(result);
            auto& c=m_chunks[index];
            if(std::none_of(wanted.begin(),wanted.end(),[index](const Request& r){return r.index==index;})) continue;
            if(!c.mesh && count>=ResidentLimit) continue;
            if(!c.mesh) ++count;
            c.mesh=std::make_unique<Mesh>(std::get<2>(result)); c.stride=std::get<1>(result);
        }
    }
    std::vector<std::tuple<size_t,int,int,int>> jobs;
    for(const auto& request:wanted) {
        const auto& c=m_chunks[request.index];
        const int stride=request.priority<500?1:4;
        if(c.mesh && c.stride==stride) continue;
        if(jobs.size()>=2 || (!c.mesh && count+jobs.size()>=ResidentLimit)) break;
        jobs.emplace_back(request.index,c.x,c.z,stride);
    }
    if(!jobs.empty()) m_pending=std::async(std::launch::async,[this,jobs=std::move(jobs)]() {
        std::vector<BuiltTile> results;
        for(const auto& job:jobs)
            results.emplace_back(std::get<0>(job),std::get<3>(job),m_terrain.vertices(std::get<1>(job),std::get<2>(job),32,std::get<3>(job),true));
        return results;
    });
}
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
            renderer.drawShadow(*(chunk.mesh?chunk.mesh:chunk.coarse),glm::mat4(1));
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
        if(frustum.intersectsSphere(chunk.center,chunk.radius))
            (chunk.mesh?chunk.mesh:chunk.coarse)->draw();
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

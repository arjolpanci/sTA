#include "scene_renderer.hpp"
#include "renderer.hpp"
#include "model_asset.hpp"
#include "texture.hpp"
#include "Game/model_catalog.hpp"
#include "Game/world.hpp"
#include "Game/terrain.hpp"
#include <glad/glad.h>
#include <map>
#include <tuple>
#include <limits>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

std::vector<SceneRenderer::Model::Surface> SceneRenderer::loadSurfaces(const ModelAsset& asset)
{
    std::map<int,const Texture*> textures;
    auto texture=[&](int image)->const Texture* {
        if(image<0) return nullptr;
        auto known=textures.find(image);
        if(known!=textures.end()) return known->second;
        const auto& data=asset.image(size_t(image));
        m_textures.push_back(std::make_unique<Texture>(data.pixels.data(),data.width,data.height,data.channels));
        return textures[image]=m_textures.back().get();
    };
    std::vector<Model::Surface> surfaces;
    for(size_t s=0;s<asset.surfaceCount();++s) {
        auto vertices=asset.surfaceVertices(s);
        if(vertices.empty()) continue;
        const auto& surface=asset.surface(s);
        Material material;
        material.albedoMap=surface.bakedColor?nullptr:texture(surface.baseColorImage);
        material.normalMap=texture(surface.normalImage);
        material.alphaCutoff=surface.alphaCutoff;
        material.doubleSided=surface.doubleSided;
        surfaces.push_back({std::make_unique<Mesh>(vertices,true),material});
    }
    return surfaces;
}

void SceneRenderer::build(Batch& batch)
{
    struct Data {std::vector<float> vertices;glm::vec3 min{1e9f},max{-1e9f};} data;
    const auto cube=Mesh::cubeVertices();
    auto append=[&](const StaticBox& box) {
        if(box.modelProxy) return;


        auto model=Mesh::boxMatrix(box.center,box.size,box.yaw);
        auto rotation=glm::mat3(glm::rotate(glm::mat4(1),glm::radians(box.yaw),glm::vec3(0,1,0)));
        for(size_t i=0;i<cube.size();i+=8) {
            glm::vec3 p=model*glm::vec4(cube[i],cube[i+1],cube[i+2],1);
            glm::vec3 n=rotation*glm::vec3(cube[i+3],cube[i+4],cube[i+5]);
            data.min=glm::min(data.min,p); data.max=glm::max(data.max,p);
            data.vertices.insert(data.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,cube[i+6],cube[i+7],box.color.r,box.color.g,box.color.b});
        }
    };
    for(const auto* b:batch.boxes) append(*b);

    // Road paint is laid over the terrain rather than resting on it: each
    // rectangle is cut into cells no wider than a metre and every corner takes
    // the terrain's own height, so a crossing at a graded junction follows the
    // camber instead of floating over it.
    const Terrain& terrain=m_world.terrain();
    for(const auto* saved:batch.markings) {
        const auto& marking=*saved;

        const int nx=std::max(1,int(std::ceil(marking.size.x/1.0f))), nz=std::max(1,int(std::ceil(marking.size.y/1.0f)));
        auto corner=[&](int i,int j) {
            glm::vec3 p(marking.center.x-marking.size.x*.5f+marking.size.x*float(i)/float(nx),0,
                        marking.center.y-marking.size.y*.5f+marking.size.y*float(j)/float(nz));
            // Just clear of the surface: enough to win the depth test at range,
            // little enough that it never reads as a kerb.
            p.y=terrain.heightAt(p.x,p.z)+.035f;
            return p;
        };
        for(int j=0;j<nz;++j) for(int i=0;i<nx;++i) {
            const glm::vec3 quad[4]={corner(i,j),corner(i+1,j),corner(i+1,j+1),corner(i,j+1)};
            // Counter-clockwise seen from above, like every other surface here.
            for(int index:{0,2,1,0,3,2}) {
                const glm::vec3 p=quad[index];
                const glm::vec3 n=terrain.normalAt(p.x,p.z);
                data.min=glm::min(data.min,p);data.max=glm::max(data.max,p);
                data.vertices.insert(data.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,0,0,marking.color.r,marking.color.g,marking.color.b});
            }
        }
    }
    batch.mesh=std::make_unique<Mesh>(data.vertices,true);
}

SceneRenderer::SceneRenderer(const World& world) : m_world(world)
{
    std::map<std::tuple<int,int,bool>,size_t> groups;
    auto group=[&](float x,float z,bool facade)->Batch& {
        const auto key=std::make_tuple(int(std::floor(x/128)),int(std::floor(z/128)),facade);
        auto it=groups.find(key);
        if(it==groups.end()) {
            size_t index=m_batches.size(); groups[key]=index;
            Material material; material.facade=facade;
            m_batches.push_back({glm::vec3(0),0,facade?1600.0f:850.0f,material,nullptr,{},{}});
            return m_batches.back();
        }
        return m_batches[it->second];
    };
    for(const auto& box:world.boxes()) if(!box.modelProxy) group(box.center.x,box.center.z,box.facade).boxes.push_back(&box);
    for(const auto& box:world.decorations()) group(box.center.x,box.center.z,box.facade).boxes.push_back(&box);
    for(const auto& marking:world.markings()) group(marking.center.x,marking.center.y,false).markings.push_back(&marking);
    for(auto& batch:m_batches) {
        glm::vec3 low(1e9f),high(-1e9f);
        for(const auto* box:batch.boxes) {
            float r=glm::length(box->size)*.5f;
            low=glm::min(low,box->center-glm::vec3(r)); high=glm::max(high,box->center+glm::vec3(r));
        }
        for(const auto* marking:batch.markings) {
            glm::vec3 center(marking->center.x,world.terrain().heightAt(marking->center.x,marking->center.y),marking->center.y);
            float r=glm::length(marking->size)*.5f+2;
            low=glm::min(low,center-glm::vec3(r)); high=glm::max(high,center+glm::vec3(r));
        }
        batch.center=(low+high)*.5f; batch.radius=glm::length(high-low)*.5f;
    }
    // One entry per model: its geometry once, then every placement of it.
    // Trees first, then the kerbside props, then the manhole cover.
    m_models.resize(TreeModels.size()+PropModels.size()+1);
    auto place=[&](size_t model,const glm::vec3& feet,float yaw,float height,const glm::vec3& size) {
        auto matrix=glm::translate(glm::mat4(1),feet)*glm::rotate(glm::mat4(1),glm::radians(yaw),glm::vec3(0,1,0))
                   *glm::scale(glm::mat4(1),glm::vec3(height));
        // Models are normalized to one unit tall, so a placed instance's radius
        // is the model's own proportions scaled to the height it was planted at.
        m_models[model].cells[{int(std::floor(feet.x/128)),int(std::floor(feet.z/128))}].push_back({matrix,feet+glm::vec3(0,height*.5f,0),glm::length(size)*.5f*height});
    };
    for(size_t i=0;i<TreeModels.size();++i) {
        ModelAsset asset("resources/models/trees/"+std::string(TreeModels[i])+".glb",false);
        m_models[i].surfaces=loadSurfaces(asset);
        const auto size=asset.size();
        for(const auto& tree:world.trees())
            if(size_t(tree.model)==i) place(i,tree.feet,tree.yaw,tree.height,size);
    }
    for(size_t i=0;i<PropModels.size()+1;++i) {
        const bool manhole=i==PropModels.size();
        const std::string name=manhole?ManholeModel:PropModels[i].name;
        ModelAsset asset("resources/models/props/"+name+".glb",false);
        const size_t model=TreeModels.size()+i;
        m_models[model].surfaces=loadSurfaces(asset);
        // Street furniture stops reading well before the trees behind it do.
        m_models[model].distance=330;
        const auto size=asset.size();
        for(const auto& prop:world.props())
            if((prop.model<0)==manhole && (manhole || size_t(prop.model)==i))
                place(model,prop.feet,prop.yaw,prop.height,size);
    }
    for(auto& model:m_models)
        if(!model.cells.empty()) glGenBuffers(1,&model.buffer);
}

size_t SceneRenderer::residentBytes() const
{
    size_t bytes=0;for(const auto& b:m_batches) if(b.mesh) bytes+=size_t(b.mesh->vertexCount())*11*sizeof(float);
    return bytes;
}
size_t SceneRenderer::residentChunks() const
{
    return size_t(std::count_if(m_batches.begin(),m_batches.end(),[](const Batch& b){return bool(b.mesh);}));
}
void SceneRenderer::updateStreaming(Renderer& renderer,const glm::vec3& camera,const glm::vec3& focus)
{
    std::vector<std::pair<float,size_t>> wanted;
    for(size_t i=0;i<m_batches.size();++i) {
        auto& batch=m_batches[i];
        float d=glm::length(glm::vec2(batch.center.x-camera.x,batch.center.z-camera.z));
        float near=glm::length(glm::vec2(batch.center.x-focus.x,batch.center.z-focus.z));
        if(d>batch.distance+batch.radius+256 && near>320+batch.radius) batch.mesh.reset();
        if(near<280+batch.radius || (d<batch.distance+batch.radius && renderer.visibleSphere(batch.center,batch.radius)))
            wanted.push_back({std::min(d,near+150),i});
    }
    std::sort(wanted.begin(),wanted.end());
    if(wanted.size()>ResidentLimit) wanted.resize(ResidentLimit);
    size_t count=residentChunks();
    for(size_t i=0;i<m_batches.size() && count>=ResidentLimit;++i)
        if(m_batches[i].mesh && std::none_of(wanted.begin(),wanted.end(),[i](const auto& r){return r.second==i;})) {
            m_batches[i].mesh.reset(); --count;
        }
    int uploads=0;
    for(const auto& request:wanted) {
        auto& batch=m_batches[request.second];
        if(batch.mesh) continue;
        if(count>=ResidentLimit || uploads>=2) break;
        build(batch); ++count; ++uploads;
    }
}

SceneRenderer::~SceneRenderer()
{
    for(auto& model:m_models)
        if(model.buffer) glDeleteBuffers(1,&model.buffer);
}

int SceneRenderer::gather(Model& model,Renderer& renderer,const glm::vec3& eye,bool shadow,float range) const
{
    if(!model.buffer) return 0;
    m_visible.clear();
    for(int z=int(std::floor((eye.z-range-32)/128));z<=int(std::floor((eye.z+range+32)/128));++z)
    for(int x=int(std::floor((eye.x-range-32)/128));x<=int(std::floor((eye.x+range+32)/128));++x) {
    auto cell=model.cells.find({x,z}); if(cell==model.cells.end()) continue;
    for(const auto& instance:cell->second)
        if(glm::length(glm::vec2(instance.center.x-eye.x,instance.center.z-eye.z))<range+instance.radius &&
           renderer.visibleSphere(instance.center,instance.radius,shadow))
            m_visible.push_back(instance.matrix);
    }
    if(m_visible.empty()) return 0;
    // Orphan-and-refill: the driver keeps the old storage alive for any draw
    // still reading it, so this never stalls on the pass before.
    glBindBuffer(GL_ARRAY_BUFFER,model.buffer);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(m_visible.size()*sizeof(glm::mat4)),m_visible.data(),GL_STREAM_DRAW);
    return int(m_visible.size());
}

void SceneRenderer::draw(Renderer& renderer,const glm::vec3& camera) const
{
    for(const auto& batch:m_batches)
        if(batch.mesh && renderer.visibleSphere(batch.center,batch.radius) && glm::length(glm::vec2(batch.center.x-camera.x,batch.center.z-camera.z))<batch.distance+batch.radius)
            renderer.draw(*batch.mesh,glm::mat4(1),batch.material);
    for(auto& model:m_models) {
        const int count=gather(model,renderer,camera,false,model.distance);
        if(!count)continue;
        for(const auto& surface:model.surfaces)
            renderer.drawInstanced(*surface.mesh,model.buffer,count,surface.material);
    }
}

void SceneRenderer::drawShadow(Renderer& renderer,const glm::vec3& focus) const
{
    for(const auto& batch:m_batches)
        if(batch.mesh && renderer.visibleSphere(batch.center,batch.radius,true) && glm::length(glm::vec2(batch.center.x-focus.x,batch.center.z-focus.z))<210+batch.radius)
            renderer.drawShadow(*batch.mesh,glm::mat4(1),batch.material);
    for(auto& model:m_models) {
        const int count=gather(model,renderer,focus,true,210);
        if(!count)continue;
        for(const auto& surface:model.surfaces)
            renderer.drawShadowInstanced(*surface.mesh,model.buffer,count,surface.material);
    }
}

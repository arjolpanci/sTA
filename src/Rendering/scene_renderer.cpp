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

SceneRenderer::SceneRenderer(const World& world)
{
    struct Data {
        std::vector<float> vertices;
        glm::vec3 min{std::numeric_limits<float>::max()}, max{-std::numeric_limits<float>::max()};
        bool facade=false;
    };
    std::map<std::tuple<int,int,bool>,Data> groups;
    const auto cube=Mesh::cubeVertices();
    auto append=[&](const StaticBox& box) {
        if(box.modelProxy) return;
        auto& data=groups[{int(std::floor(box.center.x/128)),int(std::floor(box.center.z/128)),box.facade}];
        data.facade=box.facade;
        auto model=Mesh::boxMatrix(box.center,box.size,box.yaw);
        auto rotation=glm::mat3(glm::rotate(glm::mat4(1),glm::radians(box.yaw),glm::vec3(0,1,0)));
        for(size_t i=0;i<cube.size();i+=8) {
            glm::vec3 p=model*glm::vec4(cube[i],cube[i+1],cube[i+2],1);
            glm::vec3 n=rotation*glm::vec3(cube[i+3],cube[i+4],cube[i+5]);
            data.min=glm::min(data.min,p); data.max=glm::max(data.max,p);
            data.vertices.insert(data.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,cube[i+6],cube[i+7],box.color.r,box.color.g,box.color.b});
        }
    };
    for(const auto& b:world.boxes()) append(b);
    for(const auto& b:world.decorations()) append(b);

    // Road paint is laid over the terrain rather than resting on it: each
    // rectangle is cut into cells no wider than a metre and every corner takes
    // the terrain's own height, so a crossing at a graded junction follows the
    // camber instead of floating over it.
    const Terrain& terrain=world.terrain();
    for(const auto& marking:world.markings()) {
        auto& data=groups[{int(std::floor(marking.center.x/128)),int(std::floor(marking.center.y/128)),false}];
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
    for(auto& entry:groups) {
        auto& data=entry.second;
        Material material; material.facade=data.facade;
        m_batches.push_back({(data.min+data.max)*.5f,glm::length(data.max-data.min)*.5f,
            data.facade?1600.0f:850.0f,material,std::make_unique<Mesh>(data.vertices,true)});
    }

    // One entry per model: its geometry once, then every placement of it.
    // Trees first, then the kerbside props, then the manhole cover.
    m_models.resize(TreeModels.size()+PropModels.size()+1);
    auto place=[&](size_t model,const glm::vec3& feet,float yaw,float height,const glm::vec3& size) {
        auto matrix=glm::translate(glm::mat4(1),feet)*glm::rotate(glm::mat4(1),glm::radians(yaw),glm::vec3(0,1,0))
                   *glm::scale(glm::mat4(1),glm::vec3(height));
        // Models are normalized to one unit tall, so a placed instance's radius
        // is the model's own proportions scaled to the height it was planted at.
        m_models[model].instances.push_back({matrix,feet+glm::vec3(0,height*.5f,0),glm::length(size)*.5f*height});
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
        if(!model.instances.empty()) glGenBuffers(1,&model.buffer);
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
    for(const auto& instance:model.instances)
        if(glm::length(glm::vec2(instance.center.x-eye.x,instance.center.z-eye.z))<range+instance.radius &&
           renderer.visibleSphere(instance.center,instance.radius,shadow))
            m_visible.push_back(instance.matrix);
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
        if(renderer.visibleSphere(batch.center,batch.radius) && glm::length(glm::vec2(batch.center.x-camera.x,batch.center.z-camera.z))<batch.distance+batch.radius)
            renderer.draw(*batch.mesh,glm::mat4(1),batch.material);
    for(auto& model:m_models) {
        const int count=gather(model,renderer,camera,false,model.distance);
        for(const auto& surface:model.surfaces)
            renderer.drawInstanced(*surface.mesh,model.buffer,count,surface.material);
    }
}

void SceneRenderer::drawShadow(Renderer& renderer,const glm::vec3& focus) const
{
    for(const auto& batch:m_batches)
        if(renderer.visibleSphere(batch.center,batch.radius,true) && glm::length(glm::vec2(batch.center.x-focus.x,batch.center.z-focus.z))<210+batch.radius)
            renderer.drawShadow(*batch.mesh,glm::mat4(1),batch.material);
    for(auto& model:m_models) {
        const int count=gather(model,renderer,focus,true,210);
        for(const auto& surface:model.surfaces)
            renderer.drawShadowInstanced(*surface.mesh,model.buffer,count,surface.material);
    }
}

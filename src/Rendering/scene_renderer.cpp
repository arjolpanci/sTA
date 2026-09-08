#include "scene_renderer.hpp"
#include "renderer.hpp"
#include "model_asset.hpp"
#include "texture.hpp"
#include "Game/model_catalog.hpp"
#include "Game/world.hpp"
#include <map>
#include <tuple>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

SceneRenderer::SceneRenderer(const World& world)
{
    struct Data {
        std::vector<float> vertices;
        glm::vec3 min{std::numeric_limits<float>::max()}, max{-std::numeric_limits<float>::max()};
        Material material;
        float distance = 850;
    };
    // Key: cell x/z, then the material identity - facade walls, or one tree
    // model's surface. Boxes and trees never share a batch.
    std::map<std::tuple<int,int,bool,int,int>,Data> groups;
    auto group=[&](const glm::vec3& at,bool facade,int model,int surface)->Data& {
        return groups[{int(std::floor(at.x/128)),int(std::floor(at.z/128)),facade,model,surface}];
    };
    const auto cube=Mesh::cubeVertices();
    auto append=[&](const StaticBox& box) {
        if(box.modelProxy) return;
        auto& data=group(box.center,box.facade,-1,0);
        data.material.facade=box.facade;
        data.distance=box.facade?1600:850;
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

    // One CPU copy of every surface, plus one GPU texture per image the model's
    // materials name, shared by every instance of that model.
    struct Surface { std::vector<float> vertices; Material material; };
    auto load=[&](const std::string& path) {
        ModelAsset asset(path,false);
        std::map<int,const Texture*> textures;
        auto texture=[&](int image)->const Texture* {
            if(image<0) return nullptr;
            auto known=textures.find(image);
            if(known!=textures.end()) return known->second;
            const auto& data=asset.image(size_t(image));
            m_textures.push_back(std::make_unique<Texture>(data.pixels.data(),data.width,data.height,data.channels));
            return textures[image]=m_textures.back().get();
        };
        std::vector<Surface> surfaces;
        for(size_t s=0;s<asset.surfaceCount();++s) {
            auto vertices=asset.surfaceVertices(s);
            if(vertices.empty()) continue;
            const auto& surface=asset.surface(s);
            Material material;
            material.albedoMap=surface.bakedColor?nullptr:texture(surface.baseColorImage);
            material.normalMap=texture(surface.normalImage);
            material.alphaCutoff=surface.alphaCutoff;
            material.doubleSided=surface.doubleSided;
            surfaces.push_back({std::move(vertices),material});
        }
        return surfaces;
    };
    std::array<std::vector<Surface>,TreeModels.size()> trees;
    for(size_t i=0;i<trees.size();++i)
        trees[i]=load("resources/models/trees/"+std::string(TreeModels[i])+".glb");
    for(const auto& tree:world.trees()) {
        auto rotation=glm::rotate(glm::mat4(1),glm::radians(tree.yaw),glm::vec3(0,1,0));
        auto matrix=glm::translate(glm::mat4(1),tree.feet)*rotation*glm::scale(glm::mat4(1),glm::vec3(tree.height));
        const auto& surfaces=trees.at(tree.model);
        for(size_t s=0;s<surfaces.size();++s) {
            auto& data=group(tree.feet,false,tree.model,int(s));
            data.material=surfaces[s].material;
            data.distance=500;
            const auto& vertices=surfaces[s].vertices;
            for(size_t i=0;i<vertices.size();i+=11) {
                glm::vec3 p=matrix*glm::vec4(vertices[i],vertices[i+1],vertices[i+2],1);
                glm::vec3 n=glm::mat3(rotation)*glm::vec3(vertices[i+3],vertices[i+4],vertices[i+5]);
                data.min=glm::min(data.min,p);data.max=glm::max(data.max,p);
                data.vertices.insert(data.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,vertices[i+6],vertices[i+7],vertices[i+8],vertices[i+9],vertices[i+10]});
            }
        }
    }
    // Kerbside props: the manhole cover shares the road's cell but sits flat on
    // it, so it is model index -1 rather than an entry in PropModels.
    std::array<std::vector<Surface>,PropModels.size()+1> props;
    for(size_t i=0;i<PropModels.size();++i)
        props[i]=load("resources/models/props/"+std::string(PropModels[i].name)+".glb");
    props.back()=load("resources/models/props/"+std::string(ManholeModel)+".glb");
    for(const auto& prop:world.props()) {
        const auto& surfaces=props.at(prop.model<0?props.size()-1:size_t(prop.model));
        auto rotation=glm::rotate(glm::mat4(1),glm::radians(prop.yaw),glm::vec3(0,1,0));
        auto matrix=glm::translate(glm::mat4(1),prop.feet)*rotation*glm::scale(glm::mat4(1),glm::vec3(prop.height));
        for(size_t s=0;s<surfaces.size();++s) {
            auto& data=group(prop.feet,false,int(PropModels.size())+prop.model+2,int(s));
            data.material=surfaces[s].material;
            // Street furniture is small enough to stop reading well before the
            // trees and buildings behind it do.
            data.distance=330;
            const auto& vertices=surfaces[s].vertices;
            for(size_t i=0;i<vertices.size();i+=11) {
                glm::vec3 p=matrix*glm::vec4(vertices[i],vertices[i+1],vertices[i+2],1);
                glm::vec3 n=glm::mat3(rotation)*glm::vec3(vertices[i+3],vertices[i+4],vertices[i+5]);
                data.min=glm::min(data.min,p);data.max=glm::max(data.max,p);
                data.vertices.insert(data.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,vertices[i+6],vertices[i+7],vertices[i+8],vertices[i+9],vertices[i+10]});
            }
        }
    }

    for(auto& entry:groups) {
        auto& data=entry.second;
        m_batches.push_back({(data.min+data.max)*.5f,glm::length(data.max-data.min)*.5f,
            data.distance,data.material,std::make_unique<Mesh>(data.vertices,true)});
    }
}
SceneRenderer::~SceneRenderer()=default;
void SceneRenderer::draw(Renderer& renderer,const glm::vec3& camera) const
{
    for(const auto& batch:m_batches)
        if(renderer.visibleSphere(batch.center,batch.radius) && glm::length(glm::vec2(batch.center.x-camera.x,batch.center.z-camera.z))<batch.distance+batch.radius)
            renderer.draw(*batch.mesh,glm::mat4(1),batch.material);
}
void SceneRenderer::drawShadow(Renderer& renderer,const glm::vec3& focus) const
{
    for(const auto& batch:m_batches)
        if(renderer.visibleSphere(batch.center,batch.radius,true) && glm::length(glm::vec2(batch.center.x-focus.x,batch.center.z-focus.z))<210+batch.radius)
            renderer.drawShadow(*batch.mesh,glm::mat4(1),batch.material);
}

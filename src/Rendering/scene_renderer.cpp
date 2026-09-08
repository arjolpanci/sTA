#include "scene_renderer.hpp"
#include "renderer.hpp"
#include "model_asset.hpp"
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
    };
    std::map<std::tuple<int,int,bool>,Data> groups;
    const auto cube=Mesh::cubeVertices();
    auto append=[&](const StaticBox& box) {
        if(box.modelProxy) return;
        auto& group=groups[{int(std::floor(box.center.x/128)),int(std::floor(box.center.z/128)),box.facade}];
        auto model=Mesh::boxMatrix(box.center,box.size,box.yaw);
        auto rotation=glm::mat3(glm::rotate(glm::mat4(1),glm::radians(box.yaw),glm::vec3(0,1,0)));
        for(size_t i=0;i<cube.size();i+=8) {
            glm::vec3 p=model*glm::vec4(cube[i],cube[i+1],cube[i+2],1);
            glm::vec3 n=rotation*glm::vec3(cube[i+3],cube[i+4],cube[i+5]);
            group.min=glm::min(group.min,p); group.max=glm::max(group.max,p);
            group.vertices.insert(group.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,cube[i+6],cube[i+7],box.color.r,box.color.g,box.color.b});
        }
    };
    for(const auto& b:world.boxes()) append(b);
    for(const auto& b:world.decorations()) append(b);
    std::array<std::vector<float>,TreeModels.size()> trees;
    for(size_t i=0;i<trees.size();++i)
        trees[i]=ModelAsset("resources/models/trees/"+std::string(TreeModels[i])+".glb",false).vertices();
    for(const auto& tree:world.trees()) {
        auto& group=groups[{int(std::floor(tree.feet.x/128)),int(std::floor(tree.feet.z/128)),false}];
        auto rotation=glm::rotate(glm::mat4(1),glm::radians(tree.yaw),glm::vec3(0,1,0));
        auto matrix=glm::translate(glm::mat4(1),tree.feet)*rotation*glm::scale(glm::mat4(1),glm::vec3(tree.height));
        const auto& vertices=trees.at(tree.model);
        for(size_t i=0;i<vertices.size();i+=11) {
            glm::vec3 p=matrix*glm::vec4(vertices[i],vertices[i+1],vertices[i+2],1);
            glm::vec3 n=glm::mat3(rotation)*glm::vec3(vertices[i+3],vertices[i+4],vertices[i+5]);
            group.min=glm::min(group.min,p);group.max=glm::max(group.max,p);
            group.vertices.insert(group.vertices.end(),{p.x,p.y,p.z,n.x,n.y,n.z,vertices[i+6],vertices[i+7],vertices[i+8],vertices[i+9],vertices[i+10]});
        }
    }
    for(auto& entry:groups) {
        auto& data=entry.second;
        m_batches.push_back({(data.min+data.max)*.5f,glm::length(data.max-data.min)*.5f,
            std::get<2>(entry.first),std::make_unique<Mesh>(data.vertices,true)});
    }
}
void SceneRenderer::draw(Renderer& renderer,const glm::vec3& camera) const
{
    for(const auto& batch:m_batches)
        if(glm::length(glm::vec2(batch.center.x-camera.x,batch.center.z-camera.z))<(batch.facade?1600:850)+batch.radius)
            renderer.draw(*batch.mesh,glm::mat4(1),Material{glm::vec3(1),nullptr,0,batch.facade});
}
void SceneRenderer::drawShadow(Renderer& renderer,const glm::vec3& focus) const
{
    for(const auto& batch:m_batches)
        if(glm::length(glm::vec2(batch.center.x-focus.x,batch.center.z-focus.z))<210+batch.radius)
            renderer.drawShadow(*batch.mesh,glm::mat4(1));
}

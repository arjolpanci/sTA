#include "Rendering/model_asset.hpp"
#include "Game/animation_state.hpp"
#include "Rendering/frustum.hpp"
#include "Core/performance_history.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <algorithm>

void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void check(const std::vector<float>& vertices) {
    require(!vertices.empty() && vertices.size()%33==0,"Expected triangle geometry");
    for(float v:vertices)require(std::isfinite(v),"Non-finite model data");
    for(size_t i=0;i<vertices.size();i+=11) {
        float n=glm::length(glm::vec3(vertices[i+3],vertices[i+4],vertices[i+5]));
        require(std::abs(n-1)<.01f,"Invalid posed normal");
        require(glm::length(glm::vec3(vertices[i],vertices[i+1],vertices[i+2]))<20,"Exploded geometry");
    }
}
int main() {
    PerformanceHistory performance;
    performance.record(true,{50,2,47});performance.record(false,{1,0,1});
    require(performance.summary().count==1 && performance.summary().mean.frameMs==50,"Paused frames must not overwrite gameplay timing");
    for(int i=0;i<120;++i)performance.record(true,{10,1,8});
    require(performance.summary().count==120 && performance.summary().p95Ms==10,"Performance window failed to roll over");
    Frustum camera(glm::perspective(glm::radians(60.0f),1.0f,.1f,100.0f));
    require(camera.intersectsSphere({0,0,-10},1),"Visible sphere was culled");
    require(!camera.intersectsSphere({0,0,10},1),"Sphere behind camera was not culled");
    require(!camera.intersectsSphere({30,0,-10},1),"Sphere outside side plane was not culled");
    require(!camera.intersectsSphere({0,0,-110},1),"Sphere beyond far plane was not culled");
    require(camera.intersectsSphere({0,0,0},1),"Sphere crossing near plane must be retained");
    require(camera.intersectsSphere({6,0,-10},1),"Sphere crossing side plane must be retained");
    Frustum light(glm::ortho(-20.0f,20.0f,-20.0f,20.0f,-20.0f,20.0f));
    require(light.intersectsSphere({0,0,10},1),"Off-camera shadow caster must remain visible to light");
    int models=0,characters=0;
    for(const auto& file:std::filesystem::recursive_directory_iterator("resources/models")) {
        if(file.path().extension()!=".glb")continue;
        ModelAsset model(file.path().string());++models;
        require(model.size().x>0 && model.size().z>0 && std::abs(model.size().y-1)<.001f,"Invalid normalized bounds");
        auto base=model.vertices(model.hasAnimation("Idle")?"Idle":"");check(base);
        float low=100,high=-100;
        for(size_t i=1;i<base.size();i+=11){low=std::min(low,base[i]);high=std::max(high,base[i]);}
        require(std::abs(low)<.001f && std::abs(high-1)<.001f,"Model feet/height normalization failed");
        if(model.hasAnimation("Idle")) {
            ++characters;require(model.hasAnimation("Walk") && model.hasAnimation("Run"),"Missing locomotion");
            auto start=model.vertices("Walk",0),middle=model.vertices("Walk",model.duration("Walk")*.37f);
            check(start);check(middle);float change=0;
            for(size_t i=0;i<start.size();i+=11)change+=glm::length(glm::vec3(start[i]-middle[i],start[i+1]-middle[i+1],start[i+2]-middle[i+2]));
            require(change>1,"Walking animation does not deform the model");
            auto loop=model.vertices("Walk",model.duration("Walk"));require(loop==start,"Animation loop seam/time wrapping failed");
            auto blend=model.vertices("Run",.2f,"Idle",.3f,0);auto old=model.vertices("Idle",.3f); for(size_t i=0;i<blend.size();++i)require(std::abs(blend[i]-old[i])<.0001f,"Crossfade start must preserve previous pose");
            const auto paletteSize=model.skinPalette("Idle",0).size();
            const auto name=file.path().filename().string();
            if(name.find("men-")==0 || name.find("women-")==0)
                require(paletteSize==31*7,"NPC materials must share one 31-bone palette");
            auto bind=model.skinVertices();
            require(bind.size()/19==base.size()/11,"GPU bind mesh differs from CPU topology");
            for(const auto& clip:model.animations()) {
                const float time=model.duration(clip)*.43f;
                auto cpu=model.vertices(clip,time,"Idle",.2f,.6f);check(cpu);
                auto palette=model.skinPalette(clip,time,"Idle",.2f,.6f);
                // Reproduce the vertex shader independently, including multi-part skin offsets.
                for(size_t vertex=0;vertex<cpu.size()/11;vertex+=37) {
                    size_t b=vertex*19,c=vertex*11;glm::vec4 p(0);glm::vec3 n(0);
                    for(int j=0;j<4;++j) {
                        float weight=bind[b+15+j];if(weight<=0)continue;
                        size_t bone=size_t(bind[b+11+j])*7;require(bone+6<palette.size(),"GPU bone index outside palette");
                        glm::mat4 matrix(palette[bone],palette[bone+1],palette[bone+2],palette[bone+3]);
                        glm::mat3 normal(glm::vec3(palette[bone+4]),glm::vec3(palette[bone+5]),glm::vec3(palette[bone+6]));
                        p+=weight*(matrix*glm::vec4(bind[b],bind[b+1],bind[b+2],1));
                        n+=weight*(normal*glm::vec3(bind[b+3],bind[b+4],bind[b+5]));
                    }
                    require(glm::length(glm::vec3(p)-glm::vec3(cpu[c],cpu[c+1],cpu[c+2]))<.0001f,"GPU position differs from CPU reference");
                    require(glm::length(glm::normalize(n)-glm::vec3(cpu[c+3],cpu[c+4],cpu[c+5]))<.0001f,"GPU normal differs from CPU reference");
                }
            }
        }
        std::cout<<file.path().filename()<<" size "<<model.size().x<<","<<model.size().y<<","<<model.size().z<<" triangles "<<base.size()/33<<"\n";
    }
    require(models==49 && characters==9,"Asset catalog is incomplete");
    AnimationState state;state.update("Walk",.1f);require(state.previous=="Idle" && state.blend<1,"Missing transition");
    state.update("Walk",.1f);require(state.blend==1,"Transition did not finish");
    state.update("Idle",0);require(state.previous=="Walk" && state.time==0,"Transition did not reset clock");
    bool rejected=false;try{ModelAsset bad("resources/models/missing.glb");}catch(const std::exception&){rejected=true;}require(rejected,"Missing assets must fail explicitly");
    std::cout<<"All imported models and skeletal animations passed\n";
}

#include "Rendering/model_asset.hpp"
#include "Game/animation_state.hpp"
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
            for(const auto& clip:model.animations())check(model.vertices(clip,model.duration(clip)*.43f));
        }
        std::cout<<file.path().filename()<<" size "<<model.size().x<<","<<model.size().y<<","<<model.size().z<<" triangles "<<base.size()/33<<"\n";
    }
    require(models==39 && characters==9,"Asset catalog is incomplete");
    AnimationState state;state.update("Walk",.1f);require(state.previous=="Idle" && state.blend<1,"Missing transition");
    state.update("Walk",.1f);require(state.blend==1,"Transition did not finish");
    state.update("Idle",0);require(state.previous=="Walk" && state.time==0,"Transition did not reset clock");
    bool rejected=false;try{ModelAsset bad("resources/models/missing.glb");}catch(const std::exception&){rejected=true;}require(rejected,"Missing assets must fail explicitly");
    std::cout<<"All imported models and skeletal animations passed\n";
}

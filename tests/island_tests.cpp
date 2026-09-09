#include "Game/world.hpp"
#include "Game/model_catalog.hpp"
#include <set>
#include "Game/vertical_motion.hpp"
#include <iostream>
#include <stdexcept>

struct Context {
    std::function<bool(const CollisionBox&)> collides;
    std::function<float(float,float)> groundHeightAt;
};
int main() {
    World world;
    auto require=[](bool ok,const char* message){if(!ok) throw std::runtime_error(message);};
    std::set<std::string> kinds;
    for(const auto& place:world.landmarks()) kinds.insert(place.kind);
    for(const char* kind:{"spawn","hospital","police","park","beach","forest","trail","lookout","bridge"})
        require(kinds.count(kind)>0,"Missing city landmark category");
    require(world.props().size()>500,"Baked street dressing is loaded");
    std::set<int> cars,trees;
    for(const auto& spawn:world.vehicleSpawns())cars.insert(spawn.type);
    for(const auto& tree:world.trees()) {
        trees.insert(tree.model);
        require(world.collides(CollisionBox::fromCenterHalf(tree.feet+glm::vec3(0,1,0),{.1f,.1f,.1f})),"Tree trunk collision is missing");
    }
    require(cars.size()==VehicleModels.size(),"Not all vehicle models appear in the map");
    require(trees.size()==TreeModels.size() && world.trees().size()>1000,"Tree variation or saved placement is incomplete");
    require(world.roads().size()>50,"Authored road network is loaded");
    require(world.vehicleSpawns().size()>25,"Island traffic and parked cars are loaded");
    require(world.groundHeightAt(-450,-180,19)>17.99f,"Bridge deck supports an actor above the strait");
    require(world.groundHeightAt(-450,-180,1)<0,"Bridge does not teleport a swimmer onto its deck");
    require(!world.collides(CollisionBox::fromCenterHalf({-450,0,-180},{.3f,.9f,.3f})),"Passage beneath bridge is open");
    int segments=0, failures=0;
    for (const Road& road:world.roads())
        for(size_t i=1;i<road.route.size();++i)
        for(bool reverse : {false,true}) {
            auto from=road.route[reverse ? i : i-1], to=road.route[reverse ? i-1 : i];
            glm::vec3 direction=glm::normalize(glm::vec3(to.x-from.x,0,to.z-from.z));
            float yaw=glm::degrees(std::atan2(direction.x,direction.z));
            glm::vec3 pos=from;
            pos.y=world.groundHeightAt(pos.x,pos.z,from.y+.45f);
            VerticalMotion motion;
            auto bounds=[&](){return CollisionBox::fromCenterHalf(pos+glm::vec3(0,.8f,0),road.width<=2?glm::vec3(.3f,.8f,.3f):glm::vec3(.95f,.8f,2.2f),yaw);};
            Context ctx{[&](const CollisionBox& b){return world.collides(b);},[&](float x,float z){
                auto b=bounds();b.center.x=x;b.center.z=z;
                return world.supportHeight(b,pos.y+MAX_STEP_UP);
            }};
            float distance=glm::length(glm::vec2(to.x-from.x,to.z-from.z));
            int steps=int(std::ceil(distance/.15f));
            for(int step=0;step<steps;++step) {
                moveHorizontal(pos,direction*(distance/steps),motion.grounded,bounds,ctx);
                resolveVerticalMotion(motion,pos.y,1.0f/60,ctx.groundHeightAt(pos.x,pos.z),[&](){return ctx.collides(bounds());});
            }
            if(glm::length(glm::vec2(pos.x-to.x,pos.z-to.z))>.3f || pos.y<0) {
                std::cerr<<"Road blocked from "<<from.x<<","<<from.z<<" to "<<to.x<<","<<to.z<<" at "<<pos.x<<","<<pos.y<<","<<pos.z<<'\n';
                for(const auto& box:world.boxes()) if(glm::length(glm::vec2(box.center.x-pos.x,box.center.z-pos.z))<20)
                    std::cerr<<"Nearby solid "<<box.center.x<<","<<box.center.y<<","<<box.center.z<<" size "<<box.size.x<<","<<box.size.y<<","<<box.size.z<<"\n";
                std::cerr<<"Ground target "<<world.groundHeightAt(to.x,to.z)<<" ground current "<<world.groundHeightAt(pos.x,pos.z)<<"\n";
                ++failures;
            }
            ++segments;
        }
    if(failures) { std::cerr<<failures<<" blocked road traversals\n"; return 1; }
    std::cout<<segments<<" road/bridge traversals passed in both directions with a vehicle footprint\n";
}

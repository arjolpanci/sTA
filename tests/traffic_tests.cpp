#include <cstdlib>
#include <iostream>
#include "Core/input.hpp"
#include "Rendering/camera.hpp"
#include "Game/vehicle.hpp"
#include "Game/pedestrian.hpp"
#include "Game/world.hpp"

int main() {
    World world;
    Input input;
    Camera camera;
    auto route = World::trafficLoop(-120, -60);
    Vehicle car(VehicleType::Sedan, route.front(), 90);
    car.maxSpeed = 6;
    car.setPatrol(WaypointPath(route));
    float distance = 0;
    for (int frame = 0; frame < 7200; ++frame) {
        glm::vec3 previous = car.position();
        ActorContext ctx{input, camera, false, [&](const CollisionBox& b) {return world.collides(b);},
            [&](const glm::vec3& p) {return world.surfaceNormal(p);}, [&](float x,float z) {
                auto b = car.collisionBox(); b.center.x=x; b.center.z=z;
                return world.supportHeight(b, car.position().y + MAX_STEP_UP);
            }};
        car.update(ctx, 1.0f/60);
        distance += glm::length(car.position() - previous);
        if (world.collides(car.collisionBox()) || car.position().y > 8.01f) {
            std::cerr << "Traffic left road or penetrated geometry at frame " << frame << " pos " << car.position().x << "," << car.position().y << "," << car.position().z << " yaw " << car.yaw() << '\n'; return 1;
        }
    }
    if (distance < 350) {std::cerr << "Traffic stopped making progress: " << distance << '\n'; return 1;}
    Pedestrian ped({11,8.16f,11}, WaypointPath({{11,8.16f,11},{49,8.16f,11},{49,8.16f,49},{11,8.16f,49}}), {0.5f,0.5f,0.5f});
    distance = 0;
    for (int frame = 0; frame < 7200; ++frame) {
        glm::vec3 previous = ped.collisionBox().center;
        ActorContext ctx{input, camera, false, [&](const CollisionBox& b) {return world.collides(b);},
            [&](const glm::vec3& p) {return world.surfaceNormal(p);}, [&](float x,float z) {
                auto b = ped.collisionBox(); b.center.x=x; b.center.z=z;
                return world.supportHeight(b, b.center.y-b.half.y + MAX_STEP_UP);
            }};
        ped.update(ctx, 1.0f/60);
        distance += glm::length(ped.collisionBox().center - previous);
        if (world.collides(ped.collisionBox())) {std::cerr << "Pedestrian penetrated geometry\n"; return 1;}
    }
    if (distance < 180) {std::cerr << "Pedestrian stopped making progress: " << distance << '\n'; return 1;}
    // Exercise the actual baked inter-district patrols, not just a flat block.
    int longRoutes=0;
    for (const auto& spawn : world.vehicleSpawns()) {
        Vehicle routed(static_cast<VehicleType>(spawn.type),spawn.route.front(),spawn.yaw);
        if (world.collides(routed.collisionBox())) {std::cerr << "Baked vehicle spawn intersects the world at " << spawn.route.front().x << "," << spawn.route.front().y << "," << spawn.route.front().z << " terrain " << world.terrain().heightAt(spawn.route.front().x,spawn.route.front().z) << "\n"; return 1;}
        if (spawn.speed<=0 || spawn.route.size()<=5) continue;
        ++longRoutes;
        routed.maxSpeed=spawn.speed; routed.setPatrol(WaypointPath(spawn.route));
        int stuck=0;
        float traveled=0;
        for(int frame=0;frame<36000;++frame) {
            auto previous=routed.position();
            ActorContext ctx{input,camera,false,[&](const CollisionBox& b){return world.collides(b);},
                [&](const glm::vec3& p){return world.surfaceNormal(p);},[&](float x,float z){
                    auto b=routed.collisionBox();b.center.x=x;b.center.z=z;
                    return world.supportHeight(b,b.center.y-b.half.y+MAX_STEP_UP);
                }};
            routed.update(ctx,1.0f/60);
            float movement=glm::length(routed.position()-previous); traveled+=movement;
            stuck=movement<.001f ? stuck+1 : 0;
            if(stuck>600 || routed.position().y<0) {
                std::cerr << "Baked inter-district traffic stuck at " << routed.position().x << "," << routed.position().y << "," << routed.position().z << '\n';
                return 1;
            }
        }
        if(traveled<1800) {std::cerr << "Inter-district patrol made insufficient progress\n"; return 1;}
    }
    if(longRoutes<6) {std::cerr<<"Missing inter-district traffic circuits\n";return 1;}
    camera.follow({0,1.5f,0});
    camera.avoidObstacles([](const glm::vec3& p) {return p.z > 2;});
    if (camera.position().z > 2) {std::cerr << "Camera clips obstacle\n"; return 1;}
    std::cout << "Block patrols, ten-minute inter-district routes and camera collision passed\n";
}

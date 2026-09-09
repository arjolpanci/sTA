#include "Game/terrain.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main() {
    Terrain terrain;
    auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
    require(terrain.resolution()==1025 && terrain.extent()==6144, "Baked island dimensions");
    require(terrain.roadMaskResolution()==4097, "Fine road coverage resolution");
    require(terrain.vertices(0,0,32,8).size()==4*4*6*8, "Coarse LOD reduces triangles by 64 times");
    require(terrain.vertices(0,0,32,4).size()==8*8*6*8, "Medium LOD reduces triangles by 16 times");
    require(terrain.vertices(0,0,32,8,true).size()>terrain.vertices(0,0,32,8).size(), "LOD boundary skirts are generated");
    bool rejected=false;try {terrain.vertices(0,0,32,0);} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"Invalid terrain stride is rejected");
    require(terrain.seaLevel()==0, "Sea datum");
    require(std::abs(terrain.heightAt(0,0)-8)<.01f, "Downtown terrace");
    require(std::abs(terrain.heightAt(780,-660)-12)<.01f, "East terrace");
    require(terrain.heightAt(-450,-180)<0, "Bridge crosses water, not filled terrain");
    require(terrain.heightAt(1590,250)>1 && terrain.heightAt(1590,250)<4,"Sunrise Beach has a broad sandy shelf");
    require(terrain.heightAt(1000,1570)>1 && terrain.heightAt(1000,1570)<4,"South Beach is above sea level");
    for(int z=-1500;z<=1500;z+=100) require(terrain.heightAt(-450,float(z))<0,"Open bay separates the two islands");
    float highest=-100;
    for (int z=0; z<terrain.resolution(); ++z) for (int x=0; x<terrain.resolution(); ++x) {
        float h=terrain.heights()[z*terrain.resolution()+x];
        highest=std::max(h,highest);
        if(x==0 || z==0 || x==1024 || z==1024) require(h < -30,"Map perimeter is submerged");
    }
    require(highest>300,"Mountain relief");
    auto vertices=terrain.vertices(210,130,16);
    for (size_t i=0; i<vertices.size(); i+=24) {
        glm::vec3 a(vertices[i],vertices[i+1],vertices[i+2]);
        glm::vec3 b(vertices[i+8],vertices[i+9],vertices[i+10]);
        glm::vec3 c(vertices[i+16],vertices[i+17],vertices[i+18]);
        auto p=a*.2f+b*.3f+c*.5f;
        require(std::abs(p.y-terrain.heightAt(p.x,p.z))<.002f,"Collision exactly matches rendered triangle");
        require(glm::cross(b-a,c-a).y>0,"Terrain winding faces upward");
    }
    std::cout << "Baked island dimensions, coast, terraces, relief, and triangle collision passed\n";
}

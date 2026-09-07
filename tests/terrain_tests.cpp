#include "Game/terrain.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main() {
    Terrain terrain;
    auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
    require(terrain.resolution()==513 && terrain.extent()==2048, "Baked island dimensions");
    require(terrain.seaLevel()==0, "Sea datum");
    require(std::abs(terrain.heightAt(0,0)-8)<.01f, "Downtown terrace");
    require(std::abs(terrain.heightAt(390,90)-26)<.01f, "East terrace");
    require(terrain.heightAt(-418,120)<0, "Bridge crosses water, not filled terrain");
    float highest=-100;
    for (int z=0; z<terrain.resolution(); ++z) for (int x=0; x<terrain.resolution(); ++x) {
        float h=terrain.heights()[z*terrain.resolution()+x];
        highest=std::max(h,highest);
        if(x==0 || z==0 || x==512 || z==512) require(h < -30,"Map perimeter is submerged");
    }
    require(highest>150,"Mountain relief");
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

#include "model_preview.hpp"
#include "model_asset.hpp"
#include "renderer.hpp"
#include "shadow_map.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "Game/model_catalog.hpp"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <stdexcept>

void captureModelPreviews(Renderer& renderer,ShadowMap& shadows,int width,int height)
{
    Mesh floor(Mesh::planeVertices(1));
    const auto light=glm::normalize(glm::vec3(.4f,1,.3f));
    // Keep CPU asset identities alive across contact sheets, like the live actors.
    std::vector<std::shared_ptr<const ModelAsset>> retained;
    auto sheet=[&](const char* filename,const std::vector<std::string>& files,int columns,float spacing,float distance,float modelHeight,float time) {
        struct Item {std::shared_ptr<const ModelAsset> asset;glm::mat4 matrix;AnimationState pose;};
        std::vector<Item> items;
        for(size_t i=0;i<files.size();++i) {
            auto asset=ModelAsset::load("resources/models/"+files[i]+".glb");retained.push_back(asset);
            float h=modelHeight>0?modelHeight:std::min(1.9f/asset->size().x,4.8f/asset->size().z);
            glm::vec3 position((float(i%columns)-(columns-1)*.5f)*spacing,0,(float(i/columns)-float((files.size()-1)/columns)*.5f)*spacing);
            auto matrix=glm::translate(glm::mat4(1),position)*glm::rotate(glm::mat4(1),glm::radians(25.0f),glm::vec3(0,1,0))*glm::scale(glm::mat4(1),glm::vec3(h));
            AnimationState pose;pose.clip=i%3==0?"Idle":i%3==1?"Walk":"Run";pose.time=time;
            items.push_back({asset,matrix,pose});
        }
        Camera camera;camera.maxDistance=100;camera.processScroll(7-distance);camera.processMouse(0,120);camera.follow({0,modelHeight>0?modelHeight*.4f:1,0});
        auto lightMatrix=ShadowMap::lightSpaceMatrix(light,{0,0,0},40);
        renderer.setCamera(camera,float(width)/height);
        shadows.beginCapture();renderer.beginShadowPass(lightMatrix);
        for(const auto& item:items)renderer.drawModel(*item.asset,item.asset.get(),item.matrix,item.pose,true);
        shadows.endCapture(width,height);
        glClearColor(.60f,.73f,.79f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        renderer.beginFrame(camera,float(width)/height,lightMatrix,light,shadows,true);
        renderer.draw(floor,glm::scale(glm::mat4(1),glm::vec3(100,1,100)),Material{glm::vec3(.32f,.36f,.39f)});
        for(const auto& item:items)renderer.drawModel(*item.asset,item.asset.get(),item.matrix,item.pose,false);
        std::vector<unsigned char> pixels(size_t(width)*height*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        std::ofstream file(filename,std::ios::binary);file<<"P6\n"<<width<<" "<<height<<"\n255\n";
        for(int row=height-1;row>=0;--row)file.write(reinterpret_cast<const char*>(pixels.data()+size_t(row)*width*3),width*3);
        if(!file)throw std::runtime_error("Cannot save model preview");
    };
    std::vector<std::string> files{"characters/player"};
    for(const auto sex:{"men-","women-"})for(int i=0;i<4;++i)files.push_back("characters/"+std::string(sex)+std::to_string(i));
    sheet("smoke-characters.ppm",files,3,2.8f,12,1.8f,.15f);
    sheet("smoke-characters-next.ppm",files,3,2.8f,12,1.8f,.48f);
    files.clear();for(const auto name:VehicleModels)files.push_back("cars/"+std::string(name));
    sheet("smoke-cars.ppm",files,6,5.8f,31,0,0);
    files.clear();for(const auto name:TreeModels)files.push_back("trees/"+std::string(name));
    sheet("smoke-trees.ppm",files,4,7.5f,33,6,0);
}

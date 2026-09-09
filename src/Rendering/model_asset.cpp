#include "model_asset.hpp"
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
struct Transform { glm::vec3 t{0}, s{1}; glm::quat r{1,0,0,0}; };
std::string shortName(const char* name) {
    std::string s=name?name:"";
    auto bar=s.find_last_of('|'); if(bar!=std::string::npos) s=s.substr(bar+1);
    for(const std::string prefix:{"Man_","Female_"}) if(s.find(prefix)==0) s=s.substr(prefix.size());
    return s;
}
glm::vec3 displayColor(glm::vec3 c) {
    // Existing renderer writes directly to an sRGB display without framebuffer conversion.
    for(int i=0;i<3;++i) c[i]=c[i]<=.0031308f?12.92f*c[i]:1.055f*std::pow(c[i],1/2.4f)-.055f;
    return c;
}
}
struct ModelAsset::Impl {
    struct Vertex { glm::vec3 p,n,color; glm::vec2 uv; glm::uvec4 joints{0}; glm::vec4 weights{0}; };
    struct Part { size_t node; cgltf_skin* skin; size_t surface; std::vector<Vertex> vertices; std::vector<size_t> indices; size_t boneOffset=0; };
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data{nullptr,cgltf_free};
    std::vector<Part> parts;
    struct PaletteBone { size_t node; glm::mat4 inverseBind; };
    std::vector<PaletteBone> paletteBones;
    std::vector<Surface> surfaces;
    std::vector<ImageData> images;
    glm::vec3 offset{0}, dimensions{1}; float scale=1;
    const cgltf_animation* animation(const std::string& name) const {
        if(name.empty()) return nullptr;
        for(size_t i=0;i<data->animations_count;++i)
            if(shortName(data->animations[i].name)==name || (data->animations[i].name && name==data->animations[i].name)) return &data->animations[i];
        throw std::runtime_error("Missing model animation: "+name);
    }
    float duration(const cgltf_animation* a) const {
        float end=0;
        if(a) for(size_t i=0;i<a->samplers_count;++i) {
            auto input=a->samplers[i].input; float t;
            cgltf_accessor_read_float(input,input->count-1,&t,1); end=std::max(end,t);
        }
        return end;
    }
    std::vector<Transform> pose(const std::string& clip,float time) const {
        std::vector<Transform> result(data->nodes_count);
        for(size_t i=0;i<result.size();++i) {
            const auto& n=data->nodes[i];result[i]={glm::make_vec3(n.translation),glm::make_vec3(n.scale),glm::make_quat(n.rotation)};
        }
        const auto* a=animation(clip); if(!a) return result;
        float end=duration(a); time=end>0?std::fmod(std::max(0.0f,time),end):0;
        for(size_t i=0;i<a->channels_count;++i) {
            const auto& c=a->channels[i]; const auto& s=*c.sampler;
            if(!c.target_node) continue;
            size_t lo=0,hi=s.input->count-1;
            while(lo<hi) {size_t mid=(lo+hi+1)/2;float t;cgltf_accessor_read_float(s.input,mid,&t,1);if(t<=time)lo=mid;else hi=mid-1;}
            size_t next=std::min(lo+1,s.input->count-1); float t0,t1;
            cgltf_accessor_read_float(s.input,lo,&t0,1);cgltf_accessor_read_float(s.input,next,&t1,1);
            float f=t1>t0?std::clamp((time-t0)/(t1-t0),0.0f,1.0f):0;
            if(s.interpolation==cgltf_interpolation_type_step) f=0;
            if(s.interpolation==cgltf_interpolation_type_cubic_spline) throw std::runtime_error("Cubic animation is not supported by the selected asset pipeline");
            glm::vec4 v0(0),v1(0);size_t count=c.target_path==cgltf_animation_path_type_rotation?4:3;
            cgltf_accessor_read_float(s.output,lo,glm::value_ptr(v0),count);cgltf_accessor_read_float(s.output,next,glm::value_ptr(v1),count);
            auto& target=result[c.target_node-data->nodes];
            if(c.target_path==cgltf_animation_path_type_rotation) target.r=glm::normalize(glm::slerp(glm::quat(v0.w,v0.x,v0.y,v0.z),glm::quat(v1.w,v1.x,v1.y,v1.z),f));
            else if(c.target_path==cgltf_animation_path_type_translation) target.t=glm::mix(glm::vec3(v0),glm::vec3(v1),f);
            else if(c.target_path==cgltf_animation_path_type_scale) target.s=glm::mix(glm::vec3(v0),glm::vec3(v1),f);
        }
        return result;
    }
    std::vector<glm::mat4> transforms(const std::string& clip,float time,const std::string& previous,float previousTime,float blend) const {
        auto local=pose(clip,time);
        if(blend<1 && !previous.empty()) {
            auto old=pose(previous,previousTime);
            for(size_t i=0;i<local.size();++i) {
                local[i].t=glm::mix(old[i].t,local[i].t,blend);local[i].s=glm::mix(old[i].s,local[i].s,blend);
                local[i].r=glm::slerp(old[i].r,local[i].r,blend);
            }
        }
        std::vector<glm::mat4> global(local.size()); std::vector<bool> ready(local.size());
        std::function<glm::mat4(size_t)> world=[&](size_t i) {
            if(ready[i]) return global[i];
            const auto& n=data->nodes[i];const auto& t=local[i];
            glm::mat4 mat=n.has_matrix?glm::make_mat4(n.matrix):glm::translate(glm::mat4(1),t.t)*glm::mat4_cast(t.r)*glm::scale(glm::mat4(1),t.s);
            global[i]=n.parent?world(n.parent-data->nodes)*mat:mat;ready[i]=true;return global[i];
        };
        for(size_t i=0;i<local.size();++i) world(i);
        return global;
    }
    std::vector<float> vertices(const std::string& clip,float time,const std::string& previous,float previousTime,float blend,int surfaceFilter) const {
        const auto global=transforms(clip,time,previous,previousTime,blend);
        std::vector<float> out;
        size_t count=0; for(const auto& p:parts) if(surfaceFilter<0 || p.surface==size_t(surfaceFilter)) count+=p.indices.size();
        out.reserve(count*11);
        for(const auto& part:parts) {
            if(surfaceFilter>=0 && part.surface!=size_t(surfaceFilter)) continue;
            std::vector<glm::mat4> joints;std::vector<glm::mat3> normals;
            if(part.skin) for(size_t i=0;i<part.skin->joints_count;++i) {
                glm::mat4 bind(1);if(part.skin->inverse_bind_matrices)cgltf_accessor_read_float(part.skin->inverse_bind_matrices,i,glm::value_ptr(bind),16);
                auto mat=global[part.skin->joints[i]-data->nodes]*bind;
                joints.push_back(mat);normals.push_back(glm::transpose(glm::inverse(glm::mat3(mat))));
            }
            auto rigid=global[part.node];auto rigidNormal=glm::transpose(glm::inverse(glm::mat3(rigid)));
            std::vector<glm::vec3> positions(part.vertices.size()), directions(part.vertices.size());
            for(size_t i=0;i<part.vertices.size();++i) {
                const auto& v=part.vertices[i];glm::vec3 p(0),n(0);
                if(part.skin) {
                    for(int j=0;j<4;++j) if(v.weights[j]>0) {p+=glm::vec3(joints.at(v.joints[j])*glm::vec4(v.p,1))*v.weights[j];n+=normals.at(v.joints[j])*v.n*v.weights[j];}
                } else {p=rigid*glm::vec4(v.p,1);n=rigidNormal*v.n;}
                positions[i]=(p-offset)*scale;directions[i]=glm::normalize(n);
            }
            for(size_t i:part.indices) {
                auto p=positions.at(i),n=directions.at(i),c=part.vertices[i].color;auto uv=part.vertices[i].uv;
                out.insert(out.end(),{p.x,p.y,p.z,n.x,n.y,n.z,uv.x,uv.y,c.r,c.g,c.b});
            }
        }
        return out;
    }
};

ModelAsset::ModelAsset(const std::string& path,bool centerFootprint):m(std::make_unique<Impl>()) {
    cgltf_options options{}; cgltf_data* raw=nullptr;
    if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success) throw std::runtime_error("Cannot parse model: "+path);
    m->data.reset(raw);
    if(cgltf_load_buffers(&options,raw,path.c_str())!=cgltf_result_success || cgltf_validate(raw)!=cgltf_result_success) throw std::runtime_error("Invalid model buffers: "+path);
    // Palette atlases are folded into vertex colors; anything larger is a real
    // texture map the renderer samples per fragment.
    constexpr int PALETTE_LIMIT=256;
    std::map<const cgltf_image*,int> imageIndex;
    auto decode=[&](const cgltf_image* img)->int {
        if(!img) return -1;
        auto known=imageIndex.find(img); if(known!=imageIndex.end()) return known->second;
        int w,h,channels;unsigned char* bytes=nullptr;stbi_set_flip_vertically_on_load(false);
        if(img->uri) bytes=stbi_load((std::filesystem::path(path).parent_path()/img->uri).string().c_str(),&w,&h,&channels,0);
        else if(img->buffer_view) bytes=stbi_load_from_memory(cgltf_buffer_view_data(img->buffer_view),int(img->buffer_view->size),&w,&h,&channels,0);
        if(!bytes) throw std::runtime_error("Cannot load model texture: "+path);
        m->images.push_back({w,h,channels,std::vector<unsigned char>(bytes,bytes+size_t(w)*h*channels)});stbi_image_free(bytes);
        return imageIndex[img]=int(m->images.size())-1;
    };
    std::map<const cgltf_material*,size_t> surfaceIndex;
    auto surfaceFor=[&](cgltf_material* material)->size_t {
        auto known=surfaceIndex.find(material); if(known!=surfaceIndex.end()) return known->second;
        Surface surface;
        if(material) {
            const auto& pbr=material->pbr_metallic_roughness;
            surface.albedo=displayColor(glm::make_vec3(pbr.base_color_factor));
            surface.doubleSided=material->double_sided;
            if(material->alpha_mode!=cgltf_alpha_mode_opaque)
                surface.alphaCutoff=material->alpha_mode==cgltf_alpha_mode_mask?material->alpha_cutoff:.5f;
            if(auto texture=pbr.base_color_texture.texture) {
                if(!texture->image) throw std::runtime_error("Missing model texture: "+path);
                surface.baseColorImage=decode(texture->image);
            }
            if(auto texture=material->normal_texture.texture) surface.normalImage=decode(texture->image);
            const auto* base=surface.baseColorImage>=0?&m->images[surface.baseColorImage]:nullptr;
            surface.bakedColor=surface.normalImage<0 && (!base || (base->width<=PALETTE_LIMIT && base->height<=PALETTE_LIMIT));
            // A cutout needs its alpha channel at draw time, so it can never bake.
            if(surface.alphaCutoff>0) surface.bakedColor=false;
        }
        m->surfaces.push_back(surface);
        return surfaceIndex[material]=m->surfaces.size()-1;
    };
    auto colorAt=[&](size_t surfaceId,glm::vec2 uv) {
        const auto& surface=m->surfaces[surfaceId];
        if(!surface.bakedColor || surface.baseColorImage<0) return surface.albedo;
        const auto& image=m->images[surface.baseColorImage];
        // Palette models address one solid texel per face, so a point sample
        // reproduces exactly what the fragment shader would have fetched.
        int x=std::clamp(int(uv.x*image.width),0,image.width-1),y=std::clamp(int(uv.y*image.height),0,image.height-1);
        auto pixel=&image.pixels[(size_t(y)*image.width+x)*image.channels];
        return surface.albedo*glm::vec3(pixel[0],pixel[1],pixel[2])/255.0f;
    };
    for(size_t ni=0;ni<raw->nodes_count;++ni) if(auto mesh=raw->nodes[ni].mesh) {
        for(size_t pi=0;pi<mesh->primitives_count;++pi) {
            auto& p=mesh->primitives[pi];if(p.type!=cgltf_primitive_type_triangles || p.targets_count)throw std::runtime_error("Unsupported model primitive: "+path);
            auto pos=cgltf_find_accessor(&p,cgltf_attribute_type_position,0),normal=cgltf_find_accessor(&p,cgltf_attribute_type_normal,0);
            auto uv=cgltf_find_accessor(&p,cgltf_attribute_type_texcoord,0),color=cgltf_find_accessor(&p,cgltf_attribute_type_color,0);
            auto joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,0),weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,0);
            if(!pos || !normal || (raw->nodes[ni].skin && (!joints || !weights))) throw std::runtime_error("Missing model attributes: "+path);
            Impl::Part part{ni,raw->nodes[ni].skin,surfaceFor(p.material),{},{}};
            for(size_t i=0;i<pos->count;++i) {
                Impl::Vertex v{};cgltf_accessor_read_float(pos,i,glm::value_ptr(v.p),3);cgltf_accessor_read_float(normal,i,glm::value_ptr(v.n),3);
                if(uv)cgltf_accessor_read_float(uv,i,glm::value_ptr(v.uv),2);
                v.color=colorAt(part.surface,v.uv);
                if(color) {glm::vec4 c(1);cgltf_accessor_read_float(color,i,glm::value_ptr(c),cgltf_num_components(color->type));v.color*=displayColor(glm::vec3(c));}
                if(joints)cgltf_accessor_read_uint(joints,i,glm::value_ptr(v.joints),4);
                if(weights) {cgltf_accessor_read_float(weights,i,glm::value_ptr(v.weights),4);float sum=glm::dot(v.weights,glm::vec4(1));if(sum<=0)throw std::runtime_error("Zero skin weights: "+path);v.weights/=sum;}
                part.vertices.push_back(v);
            }
            size_t count=p.indices?p.indices->count:pos->count;
            for(size_t i=0;i<count;++i)part.indices.push_back(p.indices?cgltf_accessor_read_index(p.indices,i):i);
            m->parts.push_back(std::move(part));
        }
    }
    if(m->parts.empty())throw std::runtime_error("Empty model: "+path);
    // Material primitives of one mesh all share a skin. Build its palette once,
    // instead of repeating every bone (and matrix inverse) for each material.
    std::map<const cgltf_skin*,size_t> skinOffsets;
    std::map<size_t,size_t> rigidOffsets;
    for(auto& part:m->parts) {
        if(part.skin) {
            auto known=skinOffsets.find(part.skin);
            if(known!=skinOffsets.end()){part.boneOffset=known->second;continue;}
            part.boneOffset=m->paletteBones.size();skinOffsets[part.skin]=part.boneOffset;
            for(size_t i=0;i<part.skin->joints_count;++i) {
                glm::mat4 bind(1);
                if(part.skin->inverse_bind_matrices)cgltf_accessor_read_float(part.skin->inverse_bind_matrices,i,glm::value_ptr(bind),16);
                m->paletteBones.push_back({size_t(part.skin->joints[i]-raw->nodes),bind});
            }
        } else {
            auto known=rigidOffsets.find(part.node);
            if(known!=rigidOffsets.end()){part.boneOffset=known->second;continue;}
            part.boneOffset=m->paletteBones.size();rigidOffsets[part.node]=part.boneOffset;
            m->paletteBones.push_back({part.node,glm::mat4(1)});
        }
    }
    auto vertices=m->vertices(hasAnimation("Idle")?"Idle":"",0,"",0,1,-1);
    glm::vec3 min(std::numeric_limits<float>::max()),max(-std::numeric_limits<float>::max());
    for(size_t i=0;i<vertices.size();i+=11) {glm::vec3 p(vertices[i],vertices[i+1],vertices[i+2]);min=glm::min(min,p);max=glm::max(max,p);}
    if(max.y-min.y<.00001f) throw std::runtime_error("Degenerate model: "+path);
    m->scale=1/(max.y-min.y);m->offset={(min.x+max.x)*.5f,min.y,(min.z+max.z)*.5f};m->dimensions=(max-min)*m->scale;
    if(!centerFootprint){m->offset.x=0;m->offset.z=0;}
}
ModelAsset::~ModelAsset()=default;
glm::vec3 ModelAsset::size() const{return m->dimensions;}
std::vector<std::string> ModelAsset::animations() const {std::vector<std::string> names;for(size_t i=0;i<m->data->animations_count;++i)names.push_back(shortName(m->data->animations[i].name));return names;}
bool ModelAsset::hasAnimation(const std::string& name) const {auto a=animations();return std::find(a.begin(),a.end(),name)!=a.end();}
float ModelAsset::duration(const std::string& name) const{return m->duration(m->animation(name));}
std::vector<float> ModelAsset::vertices(const std::string& clip,float time,const std::string& previous,float previousTime,float blend) const {return m->vertices(clip,time,previous,previousTime,std::clamp(blend,0.0f,1.0f),-1);}
size_t ModelAsset::surfaceCount() const {return m->surfaces.size();}
const ModelAsset::Surface& ModelAsset::surface(size_t index) const {return m->surfaces.at(index);}
const ModelAsset::ImageData& ModelAsset::image(size_t index) const {return m->images.at(index);}
std::vector<float> ModelAsset::surfaceVertices(size_t surface,const std::string& clip,float time,const std::string& previous,float previousTime,float blend) const {
    return m->vertices(clip,time,previous,previousTime,std::clamp(blend,0.0f,1.0f),int(surface));
}
std::shared_ptr<const ModelAsset> ModelAsset::load(const std::string& path) {
    static std::map<std::string,std::weak_ptr<const ModelAsset>> cache;
    auto asset=cache[path].lock();if(!asset){asset=std::make_shared<ModelAsset>(path);cache[path]=asset;}return asset;
}

bool ModelAsset::animated() const{return m->data->animations_count!=0;}
std::vector<float> ModelAsset::skinVertices() const {return skinBindVertices(-1);}
std::vector<float> ModelAsset::surfaceSkinVertices(size_t surface) const {return skinBindVertices(int(surface));}
// Each material references the shared skin offset assigned once during loading.
std::vector<float> ModelAsset::skinBindVertices(int surfaceFilter) const {
    std::vector<float> out;
    size_t count=0;for(const auto& part:m->parts)if(surfaceFilter<0||part.surface==size_t(surfaceFilter))count+=part.indices.size();
    out.reserve(count*19);
    for(const auto& part:m->parts) {
        if(surfaceFilter>=0 && part.surface!=size_t(surfaceFilter)) continue;
        for(size_t index:part.indices) {
            const auto& v=part.vertices[index];
            glm::vec4 joints=part.skin?glm::vec4(v.joints):glm::vec4(0);
            joints+=glm::vec4(float(part.boneOffset));auto weights=part.skin?v.weights:glm::vec4(1,0,0,0);
            out.insert(out.end(),{v.p.x,v.p.y,v.p.z,v.n.x,v.n.y,v.n.z,v.uv.x,v.uv.y,v.color.r,v.color.g,v.color.b,
                joints.x,joints.y,joints.z,joints.w,weights.x,weights.y,weights.z,weights.w});
        }
    }
    return out;
}
std::vector<glm::vec4> ModelAsset::skinPalette(const std::string& clip,float time,const std::string& previous,float previousTime,float blend) const {
    auto global=m->transforms(clip,time,previous,previousTime,std::clamp(blend,0.0f,1.0f));
    auto normalize=glm::scale(glm::mat4(1),glm::vec3(m->scale))*glm::translate(glm::mat4(1),-m->offset);
    std::vector<glm::vec4> palette;
    palette.reserve(m->paletteBones.size()*7);
    for(const auto& bone:m->paletteBones) {
        auto matrix=global[bone.node]*bone.inverseBind;
        auto normal=glm::transpose(glm::inverse(glm::mat3(matrix)));matrix=normalize*matrix;
        for(int c=0;c<4;++c)palette.push_back(matrix[c]);
        for(int c=0;c<3;++c)palette.push_back(glm::vec4(normal[c],0));
    }
    return palette;
}

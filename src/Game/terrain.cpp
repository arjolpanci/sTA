#include "terrain.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {
uint32_t readWord(std::istream& file) {
    unsigned char b[4];
    if (!file.read(reinterpret_cast<char*>(b), 4)) throw std::runtime_error("Truncated island heightmap");
    return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}
float readFloat(std::istream& file) {
    uint32_t bits = readWord(file); float value;
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value)) throw std::runtime_error("Non-finite island heightmap value");
    return value;
}
}
Terrain::Terrain(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Missing baked island: " + path + ". Run from the build directory; generation is tools/build_island.py.");
    char magic[8];
    if (!file.read(magic, 8) || std::string(magic, 8) != "STAISL2\n") throw std::runtime_error("Unsupported island heightmap format");
    m_resolution = static_cast<int>(readWord(file));
    m_spacing = readFloat(file); m_seaLevel = readFloat(file);
    readFloat(file); // authoring seed, metadata only
    if (m_resolution < 3 || m_resolution > 2049 || m_spacing < .1f || m_spacing > 100)
        throw std::runtime_error("Invalid island grid dimensions");
    size_t count = size_t(m_resolution) * m_resolution;
    m_heights.resize(count); m_roads.resize(count);
    for (float& h : m_heights) { h = readFloat(file); if (std::abs(h) > 10000) throw std::runtime_error("Invalid island elevation"); }
    for (float& r : m_roads) { r = readFloat(file); if (r < 0 || r > 1) throw std::runtime_error("Invalid island road mask"); }

    // Road coverage on its own finer grid: one byte per sample. The terrain
    // spacing is far too coarse to carry a five-metre road's edges.
    m_roadMaskResolution = static_cast<int>(readWord(file));
    if (m_roadMaskResolution < m_resolution || m_roadMaskResolution > 8193)
        throw std::runtime_error("Invalid island road mask grid");
    m_roadMask.resize(size_t(m_roadMaskResolution) * m_roadMaskResolution);
    if (!file.read(reinterpret_cast<char*>(m_roadMask.data()), std::streamsize(m_roadMask.size())))
        throw std::runtime_error("Truncated island road mask");
    if (file.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Unexpected island heightmap trailing data");
}
float Terrain::heightAt(float x, float z) const
{
    float gx = (x + extent() * .5f) / m_spacing, gz = (z + extent() * .5f) / m_spacing;
    if (gx < 0 || gz < 0 || gx > m_resolution-1 || gz > m_resolution-1) return -38.0f;
    int ix = std::min(static_cast<int>(gx), m_resolution-2), iz = std::min(static_cast<int>(gz), m_resolution-2);
    float u = gx - ix, v = gz - iz;
    float a=m_heights[iz*m_resolution+ix], b=m_heights[iz*m_resolution+ix+1];
    float c=m_heights[(iz+1)*m_resolution+ix], d=m_heights[(iz+1)*m_resolution+ix+1];
    return u + v <= 1 ? a + u*(b-a) + v*(c-a) : d + (1-u)*(c-d) + (1-v)*(b-d);
}
glm::vec3 Terrain::normalAt(float x, float z) const
{
    float d = m_spacing * .5f;
    return glm::normalize(glm::vec3(heightAt(x-d,z)-heightAt(x+d,z), 2*d, heightAt(x,z-d)-heightAt(x,z+d)));
}
std::vector<float> Terrain::vertices(int x0, int z0, int cells, int stride, bool skirts) const
{
    std::vector<float> result;
    if(stride<1 || cells<1 || x0<0 || z0<0 || x0+cells>=m_resolution || z0+cells>=m_resolution)
        throw std::invalid_argument("Invalid terrain tile");
    result.reserve(size_t((cells+stride-1)/stride)*((cells+stride-1)/stride)*6*8);
    auto vertex = [&](int x, int z, float drop=0) {
        float wx=x*m_spacing-extent()*.5f, wz=z*m_spacing-extent()*.5f;
        glm::vec3 n=normalAt(wx,wz);
        result.insert(result.end(), {wx,m_heights[z*m_resolution+x]-drop,wz,n.x,n.y,n.z,float(x)/(m_resolution-1),float(z)/(m_resolution-1)});
    };
    for (int z=z0; z<std::min(z0+cells,m_resolution-1); z+=stride)
        for (int x=x0; x<std::min(x0+cells,m_resolution-1); x+=stride) {
            int xx=std::min(x+stride,x0+cells),zz=std::min(z+stride,z0+cells);
            vertex(x,z); vertex(x,zz); vertex(xx,z);
            vertex(xx,z); vertex(x,zz); vertex(xx,zz);
        }
    if(skirts) {
        auto edge=[&](int ax,int az,int bx,int bz) {
            vertex(ax,az);vertex(bx,bz);vertex(ax,az,40);
            vertex(bx,bz);vertex(bx,bz,40);vertex(ax,az,40);
        };
        for(int i=0;i<cells;i+=stride) {
            int j=std::min(cells,i+stride);
            edge(x0+i,z0,x0+j,z0);edge(x0+j,z0+cells,x0+i,z0+cells);
            edge(x0,z0+j,x0,z0+i);edge(x0+cells,z0+i,x0+cells,z0+j);
        }
    }
    return result;
}

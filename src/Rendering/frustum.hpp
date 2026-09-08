#pragma once
#include <array>
#include <glm/glm.hpp>

// Conservative sphere test against the six OpenGL clip planes. Intersecting
// objects stay visible; the light has its own frustum for off-camera casters.
class Frustum {
public:
    Frustum()=default;
    explicit Frustum(const glm::mat4& viewProjection) {
        auto rows=glm::transpose(viewProjection);
        for(int axis=0;axis<3;++axis)for(int side=0;side<2;++side) {
            auto plane=rows[3]+(side?1.0f:-1.0f)*rows[axis];
            m_planes[axis*2+side]=plane/glm::length(glm::vec3(plane));
        }
        m_enabled=true;
    }
    bool intersectsSphere(const glm::vec3& center,float radius) const {
        if(!m_enabled)return true;
        for(const auto& plane:m_planes)
            if(glm::dot(glm::vec3(plane),center)+plane.w < -radius)return false;
        return true;
    }
private:
    bool m_enabled=false;
    std::array<glm::vec4,6> m_planes{};
};

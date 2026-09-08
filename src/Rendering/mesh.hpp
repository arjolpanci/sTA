#ifndef MESH_H
#define MESH_H

#include <vector>
#include <glm/glm.hpp>

// A GPU mesh: position(3), normal(3), uv(2), with optional per-vertex color(3).
// Meshes are meant to be shared: create one cube and draw it many times
// with different model matrices instead of one VAO per object.
class Mesh
{
public:
    explicit Mesh(const std::vector<float>& vertices, bool vertexColors = false, bool skinned = false);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

    // Draws the mesh once per matrix in an instance buffer (four vec4 columns
    // per instance, tightly packed). The pointers live in this mesh's VAO and
    // are re-specified per call, so one shared mesh can be drawn against any
    // caller's buffer without a VAO per instance set.
    void drawInstanced(unsigned int instanceBuffer, int instances) const;

    int vertexCount() const { return m_vertexCount; }

    // vertex data factories
    static std::vector<float> cubeVertices();                 // unit cube centered at origin
    static std::vector<float> planeVertices(float uvTiling);  // unit XZ quad at y=0, facing up

    // unit wedge/ramp: flush with the ground (y=-0.5) across the whole
    // footprint, rising to full height (y=+0.5) at +Z. Local +Z is "up the
    // slope" - Mesh::boxMatrix's yaw parameter re-orients that as needed.
    static std::vector<float> rampVertices();

    // model matrix for a box: translate to center, spin around Y, stretch the
    // unit cube/plane to size - shared by everything that draws a box
    static glm::mat4 boxMatrix(const glm::vec3& center, const glm::vec3& size, float yawDeg = 0.0f);

private:
    unsigned int m_VAO = 0, m_VBO = 0;
    int m_vertexCount = 0;
    bool m_vertexColors = false;
};

#endif

#ifndef MESH_H
#define MESH_H

#include <vector>

// A GPU mesh with a fixed vertex layout: position(3) normal(3) uv(2).
// Meshes are meant to be shared: create one cube and draw it many times
// with different model matrices instead of one VAO per object.
class Mesh
{
public:
    explicit Mesh(const std::vector<float>& vertices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

    // vertex data factories
    static std::vector<float> cubeVertices();                 // unit cube centered at origin
    static std::vector<float> planeVertices(float uvTiling);  // unit XZ quad at y=0, facing up

private:
    unsigned int m_VAO = 0, m_VBO = 0;
    int m_vertexCount = 0;
};

#endif

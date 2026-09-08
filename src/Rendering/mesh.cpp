#include "mesh.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Mesh::Mesh(const std::vector<float>& vertices, bool vertexColors, bool skinned) : m_vertexColors(vertexColors)
{
    int components = skinned ? 19 : vertexColors ? 11 : 8;
    m_vertexCount = static_cast<int>(vertices.size() / components);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = components * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    if (vertexColors) {
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
    }

    if(skinned) {
        glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,stride,(void*)(11*sizeof(float)));glEnableVertexAttribArray(4);
        glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,stride,(void*)(15*sizeof(float)));glEnableVertexAttribArray(5);
    }
    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
}

void Mesh::draw() const
{
    glBindVertexArray(m_VAO);
    if (!m_vertexColors) glVertexAttrib3f(3,1,1,1);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}

glm::mat4 Mesh::boxMatrix(const glm::vec3& center, const glm::vec3& size, float yawDeg)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
    if (yawDeg != 0.0f)
        m = glm::rotate(m, glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::scale(m, size);
}

namespace
{
    // appends one quad face as two triangles; corners a,b,c,d must be
    // counter-clockwise when viewed from outside (front face culling relies on it)
    void appendFace(std::vector<float>& v, const glm::vec3& n,
                    const glm::vec3& a, const glm::vec3& b,
                    const glm::vec3& c, const glm::vec3& d,
                    const glm::vec2 uv[4])
    {
        const glm::vec3 pos[6] = { a, b, c, a, c, d };
        const glm::vec2 uvs[6] = { uv[0], uv[1], uv[2], uv[0], uv[2], uv[3] };
        for (int i = 0; i < 6; ++i)
            v.insert(v.end(), { pos[i].x, pos[i].y, pos[i].z, n.x, n.y, n.z, uvs[i].x, uvs[i].y });
    }

    // appends one triangular face; corners must be counter-clockwise when
    // viewed from outside, same rule as appendFace
    void appendTri(std::vector<float>& v, const glm::vec3& n,
                   const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        const glm::vec3 pos[3] = { a, b, c };
        const glm::vec2 uvs[3] = { {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f} };
        for (int i = 0; i < 3; ++i)
            v.insert(v.end(), { pos[i].x, pos[i].y, pos[i].z, n.x, n.y, n.z, uvs[i].x, uvs[i].y });
    }
}

std::vector<float> Mesh::cubeVertices()
{
    std::vector<float> v;
    v.reserve(36 * 8);
    const glm::vec2 uv[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

    appendFace(v, { 0, 0, 1}, {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, uv); // front
    appendFace(v, { 0, 0,-1}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, uv); // back
    appendFace(v, {-1, 0, 0}, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, uv); // left
    appendFace(v, { 1, 0, 0}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, uv); // right
    appendFace(v, { 0, 1, 0}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, uv); // top
    appendFace(v, { 0,-1, 0}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, uv); // bottom
    return v;
}

std::vector<float> Mesh::rampVertices()
{
    std::vector<float> v;
    v.reserve(18 * 8);
    const glm::vec2 uv[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

    // low end (z=-0.5): flush with the ground, zero height, so bottom and
    // top coincide there and no face is needed to "cap" it
    const glm::vec3 lowLeft(-0.5f, -0.5f, -0.5f);
    const glm::vec3 lowRight(0.5f, -0.5f, -0.5f);
    // high end (z=+0.5): full height
    const glm::vec3 highBottomLeft(-0.5f, -0.5f, 0.5f);
    const glm::vec3 highBottomRight(0.5f, -0.5f, 0.5f);
    const glm::vec3 highTopLeft(-0.5f, 0.5f, 0.5f);
    const glm::vec3 highTopRight(0.5f, 0.5f, 0.5f);

    appendFace(v, { 0, -1, 0 }, lowLeft, lowRight, highBottomRight, highBottomLeft, uv);            // bottom
    appendFace(v, { 0, 0, 1 }, highBottomLeft, highBottomRight, highTopRight, highTopLeft, uv);     // tall-end wall
    appendFace(v, glm::normalize(glm::vec3(0, 1, -1)), lowRight, lowLeft, highTopLeft, highTopRight, uv); // sloped top
    appendTri(v, { -1, 0, 0 }, lowLeft, highBottomLeft, highTopLeft);   // left side
    appendTri(v, { 1, 0, 0 }, lowRight, highTopRight, highBottomRight); // right side
    return v;
}

std::vector<float> Mesh::planeVertices(float uvTiling)
{
    std::vector<float> v;
    v.reserve(6 * 8);
    const float t = uvTiling;
    const glm::vec2 uv[4] = { {0,0}, {t,0}, {t,t}, {0,t} };
    appendFace(v, {0, 1, 0}, {-0.5f, 0, 0.5f}, {0.5f, 0, 0.5f}, {0.5f, 0, -0.5f}, {-0.5f, 0, -0.5f}, uv);
    return v;
}

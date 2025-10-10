#ifndef BASICRENDERABLE_H
#define BASICRENDERABLE_H

#include <cstddef>
#include <glad/glad.h>
#include "shader.hpp"

class BasicRenderable
{
public:
	BasicRenderable(const float* vertices, std::size_t vertexByteSize,
		const unsigned int* indices, std::size_t indexCount,
		Shader* shader);
	~BasicRenderable();
	void addVertexAttribPointer(unsigned int index, int size, GLsizei stride, const void* pointer);

	void applyShader(Shader* shader);
	Shader* getShader() const;
	void addTexture(int textureUnit, char* imgPath, int width, int height, int nrChannels);

	void draw() const;

private:
	unsigned int m_VAO = 0, m_VBO = 0, m_EBO = 0;
	unsigned int m_texture[2];

	Shader* m_shader = nullptr;
	std::size_t m_vertexByteSize = 0;
	std::size_t m_indexCount = 0;
};

#endif
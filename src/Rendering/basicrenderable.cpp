#include "basicrenderable.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

BasicRenderable::BasicRenderable(const float* vertices, std::size_t vertexByteSize,
	const unsigned int* indices, std::size_t indexCount,
	Shader* shader)
	: m_shader(shader), m_vertexByteSize(vertexByteSize), m_indexCount(indexCount)
{	
	// Generate VAO, VBO, and EBO
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);

	glBindVertexArray(m_VAO);

	// VBO
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertexByteSize), vertices, GL_STATIC_DRAW);

	// EBO (index buffer)
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indexCount * sizeof(unsigned int)), indices, GL_STATIC_DRAW);

	// Unbinding renderable vertex array after setup
	glBindVertexArray(0);
}

void BasicRenderable::addVertexAttribPointer(unsigned int index, int size, GLsizei stride, const void* pointer)
{
	glBindVertexArray(m_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO); // ensure VBO bound
	// size = components per attribute (e.g. 3), stride in bytes (e.g. 6 * sizeof(float))
	glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, pointer);
	glEnableVertexAttribArray(index);
	glBindVertexArray(0);
}

void BasicRenderable::applyShader(Shader* shader)
{
	m_shader = shader;
}

Shader* BasicRenderable::getShader() const
{
	return m_shader;
}

void BasicRenderable::addTexture(int textureUnit, char* imgPath, int width, int height, int nrChannels)
{
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(imgPath, &width, &height, &nrChannels, 0);
	if (data)
	{
		glGenTextures(1, &m_texture[textureUnit]);
		glBindTexture(GL_TEXTURE_2D, m_texture[textureUnit]);
		// Set texture wrapping/filtering options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// Load texture data
		GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture at path: " << imgPath << std::endl;
	}

	stbi_image_free(data);

	return;
}

void BasicRenderable::draw() const
{
	if (m_shader)
	{
		m_shader->use();
	}

	for (int i= 0; i < 2; ++i)
	{
		if (m_texture[i])
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, m_texture[i]);
			if (m_shader)
			{
				m_shader->setInt("texture" + std::to_string(i), i);
			}
		}
	}

	glBindVertexArray(m_VAO);
	if (this->m_indexCount > 0)
	{
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, 0);
	}
	else {
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
}

BasicRenderable::~BasicRenderable()
{
	// Clean up resources
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteBuffers(1, &m_VBO);
	glDeleteBuffers(1, &m_EBO);
}
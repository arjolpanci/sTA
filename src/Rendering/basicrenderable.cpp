#include "basicrenderable.hpp"

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

void BasicRenderable::draw() const
{
	if (m_shader)
	{
		m_shader->use();
	}

	glBindVertexArray(m_VAO);
	// draw using index count we stored
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

BasicRenderable::~BasicRenderable()
{
	// Clean up resources
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteBuffers(1, &m_VBO);
	glDeleteBuffers(1, &m_EBO);
}
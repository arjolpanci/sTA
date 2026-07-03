#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader
{
public:
	enum Type
	{
		Vertex = 0,
		Fragment,
		Program
	};

	Shader(const char* vertexPath, const char* fragmentPath);

	void use();
	void deleteShader();

	void setBool(const std::string& name, bool value) const;
	void setInt(const std::string& name, int value) const;
	void setFloat(const std::string& name, float value) const;
	void setFloat4(const std::string& name, float val1, float val2, float val3, float val4) const;
	void setVec3(const std::string& name, const glm::vec3& value) const;
	void setMat4(const std::string& name, const glm::mat4& mat) const;
private:
	unsigned int m_ID;

	// uniform locations are cached: glGetUniformLocation does a string lookup
	// in the driver and we set uniforms for every object, every frame
	mutable std::unordered_map<std::string, int> m_uniformCache;

	int uniformLocation(const std::string& name) const;
	void checkCompileErrors(unsigned int shader, Type type);
};

#endif
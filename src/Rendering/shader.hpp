#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
private:
	unsigned int m_ID;

	void checkCompileErrors(unsigned int shader, Type type);
};

#endif
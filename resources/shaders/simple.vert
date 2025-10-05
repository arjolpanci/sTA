#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform float offset_X;
uniform float offset_Y;
out vec3 vertexColor;
out vec3 vertexPos;

void main()
{
	gl_Position = vec4(aPos.x + offset_X, aPos.y + offset_Y, aPos.z, 1.0);
	vertexColor = aColor;
	vertexPos = vec3(aPos.x + offset_X, aPos.y + offset_Y, aPos.z);
}
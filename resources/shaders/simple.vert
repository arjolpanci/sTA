#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

uniform float offset_X;
uniform float offset_Y;
uniform mat4 transform;

out vec3 vertexColor;
out vec2 TexCoord;

void main()
{
    gl_Position = transform * vec4(aPos.x + offset_X, aPos.y + offset_Y, aPos.z, 1.0);
    vertexColor = aColor;
    TexCoord = aTexCoord;
}
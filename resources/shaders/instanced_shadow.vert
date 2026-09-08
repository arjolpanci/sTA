#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUV;

layout (location = 6) in vec4 aInstance0;
layout (location = 7) in vec4 aInstance1;
layout (location = 8) in vec4 aInstance2;
layout (location = 9) in vec4 aInstance3;

uniform mat4 lightSpaceMatrix;

out vec2 vUV;

void main()
{
    vUV = aUV;
    gl_Position = lightSpaceMatrix * mat4(aInstance0, aInstance1, aInstance2, aInstance3) * vec4(aPos, 1.0);
}

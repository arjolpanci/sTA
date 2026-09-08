#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUV;

layout (location = 4) in vec4 aJoints;
layout (location = 5) in vec4 aWeights;
uniform samplerBuffer skinPalette;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

out vec2 vUV;

void main()
{
    vUV = aUV;
    vec4 position = vec4(0.0);
    for (int i = 0; i < 4; ++i) {
        if (aWeights[i] <= 0.0) continue;
        int base = int(aJoints[i]) * 7;
        mat4 bone = mat4(texelFetch(skinPalette, base), texelFetch(skinPalette, base+1),
                         texelFetch(skinPalette, base+2), texelFetch(skinPalette, base+3));
        position += aWeights[i] * (bone * vec4(aPos, 1.0));
    }
    gl_Position = lightSpaceMatrix * model * position;
}

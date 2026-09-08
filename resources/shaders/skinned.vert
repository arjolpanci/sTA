#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

layout (location = 4) in vec4 aJoints;
layout (location = 5) in vec4 aWeights;
uniform samplerBuffer skinPalette;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec3 vColor;
out vec3 vNormal;
out vec2 vUV;
out vec3 vFragPos;
out vec4 vFragPosLightSpace;

void main()
{
    vec4 position = vec4(0.0);
    vec3 normal = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        if (aWeights[i] <= 0.0) continue;
        int base = int(aJoints[i]) * 7;
        mat4 bone = mat4(texelFetch(skinPalette, base), texelFetch(skinPalette, base+1),
                         texelFetch(skinPalette, base+2), texelFetch(skinPalette, base+3));
        mat3 boneNormal = mat3(texelFetch(skinPalette, base+4).xyz,
                               texelFetch(skinPalette, base+5).xyz, texelFetch(skinPalette, base+6).xyz);
        position += aWeights[i] * (bone * vec4(aPos, 1.0));
        normal += aWeights[i] * (boneNormal * aNormal);
    }
    vec4 worldPos = model * position;
    vNormal = mat3(transpose(inverse(model))) * normalize(normal);
    vUV = aUV;
    vColor = aColor;
    vFragPos = worldPos.xyz;
    vFragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}

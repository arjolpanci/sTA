#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

// Per-instance model matrix, one advance per instance (divisor 1). Scenery is
// the same handful of models repeated thousands of times, so the geometry is
// uploaded once and only these 64 bytes vary.
layout (location = 6) in vec4 aInstance0;
layout (location = 7) in vec4 aInstance1;
layout (location = 8) in vec4 aInstance2;
layout (location = 9) in vec4 aInstance3;

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
    mat4 model = mat4(aInstance0, aInstance1, aInstance2, aInstance3);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vUV = aUV;
    vColor = aColor;
    vFragPos = worldPos.xyz;
    vFragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}

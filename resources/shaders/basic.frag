#version 330 core
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

uniform vec3 color;
uniform bool useTexture;
uniform sampler2D tex;

// simple fixed directional light (sun)
const vec3 lightDir = normalize(vec3(0.4, 1.0, 0.3));

void main()
{
    vec3 base = color;
    if (useTexture)
        base *= texture(tex, vUV).rgb;

    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);
    vec3 lit = base * (0.35 + 0.65 * diffuse);
    FragColor = vec4(lit, 1.0);
}

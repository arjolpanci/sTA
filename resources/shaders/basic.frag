#version 330 core
in vec3 vNormal;
in vec2 vUV;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

out vec4 FragColor;

// material
uniform vec3 color;
uniform bool useTexture;
uniform sampler2D tex;
uniform float shininess; // 0 disables the specular highlight

// lighting (set once per frame, not per material)
uniform vec3 lightDir;   // normalized, points toward the light
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;

// percentage-closer filtering over a 3x3 texel neighborhood, softening the
// hard edge a single shadow-map sample would otherwise produce
float calcShadow(vec4 fragPosLightSpace, vec3 normal)
{
    if (!shadowsEnabled)
        return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // [-1,1] -> [0,1]

    if (projCoords.z > 1.0)
        return 0.0; // beyond the shadow frustum's far plane: treat as lit

    // slope-scaled bias: grazing angles need more bias to avoid acne
    float bias = max(0.0025 * (1.0 - dot(normal, lightDir)), 0.0006);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main()
{
    vec3 base = color;
    if (useTexture)
        base *= texture(tex, vUV).rgb;

    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, lightDir), 0.0);

    float specular = 0.0;
    if (shininess > 0.0 && diffuse > 0.0)
    {
        vec3 viewDir = normalize(viewPos - vFragPos);
        vec3 halfway = normalize(lightDir + viewDir);
        specular = pow(max(dot(normal, halfway), 0.0), shininess);
    }

    float shadow = calcShadow(vFragPosLightSpace, normal);

    // ambient represents indirect/sky light, so it isn't blocked by the
    // direct light's shadow - only the diffuse+specular term is
    vec3 ambient = base * 0.35;
    vec3 direct = base * diffuse + vec3(specular);
    vec3 finalColor = ambient + (1.0 - shadow) * direct * 0.65;
    FragColor = vec4(finalColor, 1.0);
}

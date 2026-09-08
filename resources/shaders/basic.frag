#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec2 vUV;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

out vec4 FragColor;

// material
uniform vec3 color;
uniform bool useTexture;
uniform bool facade;
uniform sampler2D tex;
uniform float shininess;    // 0 disables the specular highlight
uniform bool useNormalMap;
uniform sampler2D normalMap;
uniform float alphaCutoff;  // 0 disables the cutout test

// lighting (set once per frame, not per material)
uniform vec3 lightDir;   // normalized, points toward the light
uniform vec3 sunColor;     // direct light, warm near the horizon
uniform vec3 ambientColor; // sky light: what a shadowed surface still receives
uniform vec3 fogColor;     // horizon haze, matched to the sky
uniform float night;       // 0 in daylight, 1 after dusk
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;

// Tangent frame rebuilt from screen-space derivatives, so a normal map costs
// no extra vertex attribute and no change to Mesh's layout. Degenerate UVs
// (a stretched or unwrapped face) fall back to the interpolated normal.
mat3 cotangentFrame(vec3 normal, vec3 fragPos, vec2 uv)
{
    vec3 dp1 = dFdx(fragPos), dp2 = dFdy(fragPos);
    vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
    vec3 perp1 = cross(dp2, normal), perp2 = cross(normal, dp1);
    vec3 tangent = perp1 * duv1.x + perp2 * duv2.x;
    vec3 bitangent = perp1 * duv1.y + perp2 * duv2.y;
    float scale = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));
    return mat3(tangent * scale, bitangent * scale, normal);
}

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
    vec3 base = color * vColor;
    if (useTexture)
    {
        vec4 texel = texture(tex, vUV);
        // Cut out before anything else: a discarded leaf fragment should never
        // reach the lighting or the depth buffer at all
        if (texel.a < alphaCutoff)
            discard;
        base *= texel.rgb;
    }

    vec3 normal = normalize(vNormal);
    // Leaf cards are drawn double-sided, so half of them face away from their
    // own normal - lighting them needs the geometric side, not the authored one
    if (!gl_FrontFacing)
        normal = -normal;
    if (useNormalMap)
    {
        vec3 tangentNormal = texture(normalMap, vUV).rgb * 2.0 - 1.0;
        normal = normalize(cotangentFrame(normal, vFragPos, vUV) * tangentNormal);
    }
    float windowGlow = 0.0;
    if (facade && abs(normal.y) < 0.5)
    {
        vec2 wall = vec2(abs(normal.x) > 0.5 ? vFragPos.z : vFragPos.x, vFragPos.y);
        vec2 cell = fract(wall / vec2(2.6, 3.2));
        float window = step(0.20, cell.x) * step(cell.x, 0.80) * step(0.30, cell.y) * step(cell.y, 0.83) * step(2.8, wall.y);
        float lit = step(0.77, fract(sin(dot(floor(wall / vec2(2.6, 3.2)), vec2(12.9898,78.233))) * 43758.5453));
        vec3 glass = mix(vec3(0.12, 0.23, 0.29), vec3(0.78, 0.66, 0.38), lit);
        base = mix(base, glass, window);
        base *= 1.0 - 0.12 * step(0.96, cell.y);
        windowGlow = window * lit * 0.13;
    }
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
    vec3 ambient = base * ambientColor;
    vec3 direct = (base * diffuse + vec3(specular)) * sunColor;
    vec3 finalColor = ambient + (1.0 - shadow) * direct;
    // Lit windows barely register at noon and carry the skyline at night.
    finalColor += vec3(1.0, 0.79, 0.45) * windowGlow * (0.35 + 3.0 * night);
    float fog = smoothstep(600.0, 2700.0, length(viewPos - vFragPos));
    finalColor = mix(finalColor, fogColor, fog);
    FragColor = vec4(finalColor, 1.0);
}

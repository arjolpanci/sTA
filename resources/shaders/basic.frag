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
uniform sampler2DShadow shadowMap; // comparison sampler: the hardware does the depth test
uniform bool shadowsEnabled;
uniform mat4 lightSpaceMatrix;
uniform float shadowTexelWorld;    // world size of one shadow texel

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

// Percentage-closer filtering over a rotated 4x4 kernel. Each tap is itself a
// hardware 2x2 comparison, so this is 64 effective samples, and the per-pixel
// rotation turns what would be banding into noise the eye reads as a penumbra.
float calcShadow(vec3 worldPos, vec3 normal, vec3 geometricNormal)
{
    if (!shadowsEnabled)
        return 0.0;

    // Normal-offset bias: push the lookup off the surface along its own normal
    // instead of biasing depth. Depth bias has to grow with slope until it
    // detaches the shadow from its caster; this does not.
    float slope = clamp(1.0 - dot(geometricNormal, lightDir), 0.0, 1.0);
    vec3 offset = geometricNormal * shadowTexelWorld * (1.2 + 2.6 * slope);
    vec4 lightSpace = lightSpaceMatrix * vec4(worldPos + offset, 1.0);

    vec3 projCoords = lightSpace.xyz / lightSpace.w;
    projCoords = projCoords * 0.5 + 0.5; // [-1,1] -> [0,1]
    if (projCoords.z > 1.0)
        return 0.0; // beyond the shadow frustum's far plane: treat as lit

    float depth = projCoords.z - 0.00045;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    float lit = 0.0;
    for (int x = -1; x <= 2; ++x)
        for (int y = -1; y <= 2; ++y)
            lit += texture(shadowMap, vec3(projCoords.xy + rotation * (vec2(x, y) - 0.5) * texelSize, depth));
    return 1.0 - lit / 16.0;
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

    // The geometric normal drives the bias: a normal-mapped one can point into
    // the surface and would push the lookup the wrong way.
    vec3 geometric = normalize(vNormal);
    if (!gl_FrontFacing) geometric = -geometric;
    float shadow = calcShadow(vFragPos, normal, geometric);

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

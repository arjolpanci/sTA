#version 330 core
in vec3 vRay;
out vec4 FragColor;

uniform vec3 sunDirection;   // normalized, points toward the sun
uniform float sunElevation;  // sine of the sun's angle above the horizon
uniform float hours;
uniform float time;           // seconds, drives the wind
uniform float cloudCoverage;  // 0 clear, 1 overcast
uniform float cloudDensity;   // how opaque a cloud gets once it forms
uniform float cloudSpeed;     // wind, metres per second

const float PI = 3.14159265;
// Rayleigh scattering is wavelength-dependent - blue scatters an order of
// magnitude more than red, which is the whole reason the sky is blue and
// sunsets are not. Mie is the grey haze from larger aerosols.
const vec3 BETA_RAYLEIGH = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const float BETA_MIE = 21e-6;
const float SCALE_RAYLEIGH = 8000.0;  // atmospheric scale heights, metres
const float SCALE_MIE = 1200.0;

// Optical mass through the atmosphere for a ray at this elevation. The added
// term is what stops the horizon going singular as the cosine reaches zero.
float opticalDepth(float cosine, float scaleHeight)
{
    float angle = degrees(acos(clamp(cosine, -1.0, 1.0)));
    return scaleHeight / (max(cosine, 0.0) + 0.15 * pow(max(93.885 - angle, 0.1), -1.253));
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float valueNoise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

// Five octaves, each half the amplitude and twice the frequency, with a slight
// rotation between them so the layers do not line up into a visible grid.
float fbm(vec2 p)
{
    const mat2 turn = mat2(1.6, 1.2, -1.2, 1.6);
    float sum = 0.0, amplitude = 0.5;
    for (int i = 0; i < 5; ++i)
    {
        sum += amplitude * valueNoise(p);
        p = turn * p;
        amplitude *= 0.5;
    }
    return sum;
}

// Cloud cover where the view ray crosses a flat layer overhead. A slab would
// be more correct and much more expensive; at this altitude the difference
// only shows within a few degrees of the horizon, where the layer is faded out
// anyway.
float clouds(vec3 ray, out float shading)
{
    shading = 0.0;
    if (cloudCoverage <= 0.001 || ray.y < 0.02) return 0.0;

    const float LAYER = 900.0;
    vec2 p = ray.xz / ray.y * LAYER * 0.0016 + vec2(cloudSpeed * time * 0.0016, 0.0);
    float density = fbm(p);
    // Coverage is a threshold on the noise, so raising it grows existing clouds
    // outward instead of fading a uniform grey over the whole sky.
    float cover = smoothstep(1.0 - cloudCoverage, 1.0 - cloudCoverage + 0.28, density);

    // Cheap self-shadowing: sample again a little way toward the sun. Where
    // there is more cloud in that direction, this part of it is in shade.
    vec2 toward = normalize(sunDirection.xz + vec2(1e-4)) * 0.08;
    shading = clamp(1.0 - (fbm(p + toward) - density) * 2.4, 0.25, 1.0);

    // Fade the layer out toward the horizon, where a flat plane would stretch
    // to infinity, and out at night.
    return cover * cloudDensity * smoothstep(0.02, 0.22, ray.y);
}

void main()
{
    vec3 ray = normalize(vRay);
    float up = ray.y;
    float cosTheta = dot(ray, sunDirection);

    // Phase functions: how much light scatters toward the viewer per angle.
    float rayleighPhase = 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
    const float g = 0.76;
    float miePhase = 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + cosTheta * cosTheta))
                   / ((2.0 + g * g) * pow(max(1.0 + g * g - 2.0 * g * cosTheta, 1e-4), 1.5));

    // How much air the view ray and the sunlight each travel through. When the
    // sun is low its light crosses far more atmosphere, the blue is scattered
    // out of it long before it arrives, and what is left is red.
    float viewRayleigh = opticalDepth(up, SCALE_RAYLEIGH);
    float viewMie = opticalDepth(up, SCALE_MIE);
    float sunRayleigh = opticalDepth(sunElevation, SCALE_RAYLEIGH);
    float sunMie = opticalDepth(sunElevation, SCALE_MIE);

    vec3 sunTransmittance = exp(-(BETA_RAYLEIGH * sunRayleigh + BETA_MIE * sunMie));
    vec3 viewTransmittance = exp(-(BETA_RAYLEIGH * viewRayleigh + BETA_MIE * viewMie));
    vec3 scattered = (BETA_RAYLEIGH * viewRayleigh * rayleighPhase + BETA_MIE * viewMie * miePhase)
                   * sunTransmittance * 26.0;

    float daylight = smoothstep(-0.16, 0.10, sunElevation);
    vec3 color = scattered * daylight;
    // Single scattering alone leaves a low sun looking olive: the blue is gone
    // but the green is not. Warming the whole sky as the sun drops stands in
    // for the multiple scattering that reddens a real sunset.
    color *= mix(vec3(1.0), vec3(1.22, 0.88, 0.82), clamp(1.0 - sunElevation / 0.32, 0.0, 1.0) * daylight);

    // Night: a dim zenith gradient plus a stable star field, fading out as soon
    // as there is any real skylight to wash it out.
    float night = 1.0 - daylight;
    if (night > 0.001)
    {
        vec3 grid = floor(ray * 420.0);
        float star = step(0.9975, hash(grid.xz + grid.y * 13.0)) * smoothstep(0.0, 0.25, up);
        color += (vec3(0.020, 0.028, 0.052) * (0.35 + 0.65 * max(up, 0.0)) + vec3(star)) * night;
    }

    // The sun itself, softened by the same transmittance that reddens it. It
    // has to out-run the tonemapper below, which is already compressing a sky
    // that is bright in its own right - hence an intensity this far above one.
    float disc = smoothstep(0.99955, 0.99988, cosTheta);
    float glow = pow(max(cosTheta, 0.0), 220.0) * 0.5 + pow(max(cosTheta, 0.0), 12.0) * 0.06;
    float above = smoothstep(-0.06, 0.04, sunElevation);
    color += (disc * 90.0 + glow) * sunTransmittance * above;

    // Clouds sit in front of all of that, lit by the same sun colour.
    float shading;
    float cover = clouds(ray, shading);
    if (cover > 0.001)
    {
        vec3 sunlit = sunTransmittance * (2.6 * above + 0.05);
        vec3 cloudColor = mix(vec3(0.28, 0.32, 0.40) * (0.25 + 0.75 * daylight), sunlit, shading);
        color = mix(color, cloudColor, clamp(cover, 0.0, 1.0));
    }

    // Ground half: no terrain reaches the horizon everywhere, so the lower
    // hemisphere fades to the same haze the distance fog uses.
    vec3 haze = mix(color, vec3(dot(color, vec3(0.33))) * vec3(0.85, 0.88, 0.92), 0.55);
    color = mix(color, haze, smoothstep(0.04, -0.12, up));

    // Tonemap, then gamma: the renderer writes straight to an sRGB display.
    color = color / (color + vec3(1.35));
    FragColor = vec4(pow(max(color, 0.0), vec3(1.0 / 2.2)), 1.0);
}

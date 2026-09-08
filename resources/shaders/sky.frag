#version 330 core
in vec3 vRay;
out vec4 FragColor;

uniform vec3 sunDirection;   // normalized, points toward the sun
uniform float sunElevation;  // sine of the sun's angle above the horizon
uniform float hours;

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
                   * sunTransmittance * 22.0;

    float daylight = smoothstep(-0.16, 0.10, sunElevation);
    vec3 color = scattered * daylight;

    // Night: a dim zenith gradient plus a stable star field, fading out as soon
    // as there is any real skylight to wash it out.
    float night = 1.0 - daylight;
    if (night > 0.001)
    {
        vec3 grid = floor(ray * 420.0);
        float star = step(0.9975, hash(grid.xz + grid.y * 13.0)) * smoothstep(0.0, 0.25, up);
        color += (vec3(0.020, 0.028, 0.052) * (0.35 + 0.65 * max(up, 0.0)) + vec3(star)) * night;
    }

    // The sun itself, softened by the same transmittance that reddens it.
    float disc = smoothstep(0.99965, 0.99992, cosTheta);
    float glow = pow(max(cosTheta, 0.0), 320.0) * 0.35;
    color += (disc * 14.0 + glow) * sunTransmittance * smoothstep(-0.06, 0.04, sunElevation);

    // Ground half: no terrain reaches the horizon everywhere, so the lower
    // hemisphere fades to the same haze the distance fog uses.
    vec3 haze = mix(color, vec3(dot(color, vec3(0.33))) * vec3(0.85, 0.88, 0.92), 0.55);
    color = mix(color, haze, smoothstep(0.04, -0.12, up));

    // Tonemap, then gamma: the renderer writes straight to an sRGB display.
    color = color / (color + vec3(0.85));
    FragColor = vec4(pow(max(color, 0.0), vec3(1.0 / 2.2)), 1.0);
}

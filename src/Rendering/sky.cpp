#include "sky.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

#include "camera.hpp"

namespace {
// The sun rises in the east, peaks slightly south and sets in the west; the
// tilt is what keeps it off a perfect vertical arc and gives long shadows for
// most of the day rather than only at dawn.
glm::vec3 sunDirection(float hours)
{
    const float angle = (hours / 24.0f) * 6.2831853f - 1.5707963f; // 06:00 sunrise
    const float tilt = 0.32f;
    glm::vec3 direction(std::cos(angle), std::sin(angle), 0.0f);
    return glm::normalize(glm::vec3(direction.x, direction.y, tilt * (1.0f - std::abs(direction.y))));
}
glm::vec3 mix(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }
}

Lighting Lighting::atTime(float hours)
{
    Lighting lighting;
    lighting.direction = sunDirection(hours);
    lighting.sunElevation = lighting.direction.y;

    // Daylight fades out below the horizon rather than switching off, so dusk
    // has a usable minute of half-light like it does outdoors.
    const float day = std::clamp((lighting.sunElevation + 0.10f) / 0.28f, 0.0f, 1.0f);
    const float low = std::clamp(1.0f - lighting.sunElevation / 0.35f, 0.0f, 1.0f); // reddening near the horizon

    // Calibrated so ambient + sun lands near 1.0 on a surface facing the noon
    // sun: the day/night swing is the new part, not a brighter scene.
    const glm::vec3 noon(1.0f, 0.98f, 0.94f), horizon(1.0f, 0.52f, 0.24f);
    lighting.sunColor = mix(noon, horizon, low * low) * (0.10f + 0.66f * day);
    // Moonlight keeps the night navigable and cold rather than black.
    const glm::vec3 night(0.045f, 0.06f, 0.11f);
    lighting.ambientColor = mix(night, mix(glm::vec3(0.29f, 0.27f, 0.28f), glm::vec3(0.31f, 0.33f, 0.37f), 1.0f - low), day);
    lighting.fogColor = mix(glm::vec3(0.05f, 0.07f, 0.12f),
                            mix(glm::vec3(0.74f, 0.52f, 0.38f), glm::vec3(0.60f, 0.73f, 0.79f), 1.0f - low), day);
    if (lighting.sunElevation < 0.0f)
    {
        // Below the horizon the "sun" would light the underside of everything.
        lighting.direction.y = std::max(lighting.direction.y, 0.02f);
        lighting.direction = glm::normalize(lighting.direction);
    }
    return lighting;
}

Sky::Sky() : m_shader("resources/shaders/sky.vert", "resources/shaders/sky.frag")
{
    // The vertices are generated in the vertex shader, but core profile still
    // needs some VAO bound for a draw call to be legal.
    glGenVertexArrays(1, &m_VAO);
}

Sky::~Sky() { glDeleteVertexArrays(1, &m_VAO); }

void Sky::draw(const Camera& camera, float aspect, const Lighting& lighting, float hours,
               const Clouds& clouds, float time)
{
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 3000.0f);
    m_shader.use();
    m_shader.setMat4("inverseViewProjection", glm::inverse(projection * glm::mat4(glm::mat3(camera.viewMatrix()))));
    m_shader.setVec3("sunDirection", lighting.direction);
    m_shader.setVec3("fogColor", lighting.fogColor);
    m_shader.setFloat("sunElevation", lighting.sunElevation);
    m_shader.setFloat("hours", hours);
    m_shader.setFloat("time", time);
    m_shader.setFloat("cloudCoverage", clouds.coverage);
    m_shader.setFloat("cloudDensity", clouds.density);
    m_shader.setFloat("cloudSpeed", clouds.speed);

    // No depth: the sky is the background, and everything else overwrites it.
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

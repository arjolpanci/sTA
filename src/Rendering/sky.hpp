#pragma once
#include <glm/glm.hpp>
#include <memory>
#include "shader.hpp"

class Camera;
class Mesh;

// Everything the scene needs to know about the sun and the air, derived once
// per frame from the time of day. Passed around as one value so adding a term
// (moonlight, weather) never changes another signature.
struct Lighting
{
    glm::vec3 direction{ 0.4f, 1.0f, 0.3f }; // normalized, points toward the sun
    glm::vec3 sunColor{ 1.0f };              // direct light, warm near the horizon
    glm::vec3 ambientColor{ 0.35f };         // sky light, what shadowed surfaces get
    glm::vec3 fogColor{ 0.60f, 0.73f, 0.79f }; // distance haze, matches the horizon
    float sunElevation = 1.0f;               // sin of the sun's angle above the horizon

    // Sun position, light colours and haze for a time of day in hours [0,24).
    static Lighting atTime(float hours);
};

// Analytic sky: single-scattering Rayleigh and Mie evaluated per fragment, so
// dawn and dusk come out of the same model that makes midday blue rather than
// out of a hand-tuned gradient. Drawn as one full-screen triangle behind
// everything else, with the view ray rebuilt from the inverse view-projection.
class Sky
{
public:
    Sky();
    ~Sky();
    void draw(const Camera& camera, float aspect, const Lighting& lighting, float hours);

private:
    Shader m_shader;
    unsigned int m_VAO = 0;
};

#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>

class Texture;

// Describes how a surface should be shaded. Deliberately not tied to
// Renderer::draw()'s signature - callers pass this by reference, so growing
// it (a normal/bump map, roughness, emissive) later never means touching
// draw() again the way separate color/texture parameters would have.
struct Material
{
    glm::vec3 albedo{ 1.0f };            // base color, multiplied by albedoMap if set
    const Texture* albedoMap = nullptr;  // optional; null = flat albedo color only
    float shininess = 0.0f;              // Blinn-Phong specular exponent; 0 disables the highlight
    bool facade = false;
};

#endif

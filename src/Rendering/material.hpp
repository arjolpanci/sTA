#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>

class Texture;

// Describes how a surface should be shaded. Deliberately not tied to
// Renderer::draw()'s signature - callers pass this by reference, so growing
// it (roughness, emissive) later never means touching draw() again the way
// separate color/texture parameters would have.
struct Material
{
    glm::vec3 albedo{ 1.0f };            // base color, multiplied by albedoMap if set
    const Texture* albedoMap = nullptr;  // optional; null = flat albedo color only
    float shininess = 0.0f;              // Blinn-Phong specular exponent; 0 disables the highlight
    bool facade = false;

    // Tangent-space normal map (OpenGL convention, +Y up). The tangent frame is
    // rebuilt in the fragment shader from screen-space derivatives, so this needs
    // no extra vertex attribute and no change to Mesh's layout.
    const Texture* normalMap = nullptr;

    // Alpha cutout for foliage cards: fragments whose albedo alpha falls below
    // this are discarded outright. 0 disables the test, which is what every
    // opaque surface wants - a leaf texture needs ~0.5. Cutout rather than
    // blending, so leaves still write depth and need no back-to-front sorting.
    float alphaCutoff = 0.0f;

    // Leaf cards are single-sided geometry seen from both sides; drawing them
    // with face culling on makes half of every canopy vanish.
    bool doubleSided = false;
};

#endif

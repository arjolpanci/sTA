#pragma once
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"

// Shared articulated placeholder silhouette; both passes use identical poses.
inline void drawCharacter(Renderer& renderer, const Mesh& cube, glm::vec3 feet,
                          float height, float yaw, glm::vec3 shirt, float gait, bool shadow)
{
    glm::mat4 root = glm::translate(glm::mat4(1), feet);
    root = glm::rotate(root, glm::radians(yaw), {0,1,0});
    root = glm::scale(root, glm::vec3(height / 1.8f));
    float swing = std::sin(gait) * 0.10f;
    auto part = [&](glm::vec3 offset, glm::vec3 size, glm::vec3 color) {
        auto model = glm::scale(glm::translate(root, offset), size);
        if (shadow) renderer.drawShadow(cube, model);
        else renderer.draw(cube, model, Material{color});
    };
    part({0,1.10f,0}, {0.34f,0.64f,0.28f}, shirt);
    part({0,1.59f,0}, {0.32f,0.36f,0.30f}, {0.70f,0.51f,0.37f});
    part({0,1.77f,-0.02f}, {0.33f,0.09f,0.30f}, {0.16f,0.12f,0.10f});
    for (int side : {-1,1}) {
        part({side * 0.09f,0.42f,side*swing}, {0.15f,0.72f,0.18f}, {0.16f,0.20f,0.26f});
        part({side * 0.09f,0.06f,0.025f+side*swing}, {0.16f,0.12f,0.24f}, {0.10f,0.12f,0.14f});
        part({side * 0.23f,1.06f,-side*swing}, {0.10f,0.58f,0.16f}, shirt * 0.85f);
    }
}

#include "game.hpp"

#include <algorithm>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/texture.hpp"

namespace
{
    void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    // model matrix for a box: translate to center, spin around Y, stretch the
    // unit cube/plane to size
    glm::mat4 boxMatrix(const glm::vec3& center, const glm::vec3& size, float yawDeg = 0.0f)
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
        if (yawDeg != 0.0f)
            m = glm::rotate(m, glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
        return glm::scale(m, size);
    }
}

Game::Game() = default;

Game::~Game()
{
    // GL resources must be destroyed while the context still exists
    m_renderer.reset();
    m_cubeMesh.reset();
    m_groundMesh.reset();
    m_groundTexture.reset();
    if (m_window)
        glfwTerminate();
}

bool Game::init()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(1600, 900, "small Theft Auto", NULL, NULL);
    if (!m_window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    m_input.attach(m_window);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // meshes are wound CCW, skip drawing back faces
    glEnable(GL_MULTISAMPLE);

    m_renderer = std::make_unique<Renderer>();
    m_cubeMesh = std::make_unique<Mesh>(Mesh::cubeVertices());
    m_groundMesh = std::make_unique<Mesh>(Mesh::planeVertices(40.0f));
    m_groundTexture = std::make_unique<Texture>("resources/textures/asphalt.jpg");

    // parked cars; their AABBs are registered as static world colliders
    m_vehicles.emplace_back(VehicleType::Taxi, glm::vec3(8.0f, 0.0f, 6.0f), 0.0f);
    m_vehicles.emplace_back(VehicleType::Sedan, glm::vec3(-12.0f, 0.0f, 12.0f), 90.0f);
    m_vehicles.emplace_back(VehicleType::Van, glm::vec3(18.0f, 0.0f, -14.0f), 180.0f);
    for (const Vehicle& vehicle : m_vehicles)
        m_world.addCollider(vehicle.aabb());

    m_player.position = glm::vec3(0.0f, 0.0f, 0.0f);
    return true;
}

int Game::run()
{
    if (!init())
        return -1;

    // fixed-timestep loop: simulation always steps at SIM_RATE regardless of
    // how fast frames render, so physics stays deterministic
    const double SIM_DT = 1.0 / 60.0;
    double accumulator = 0.0;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(m_window))
    {
        double now = glfwGetTime();
        // clamp so a stall (window drag, breakpoint) doesn't trigger a
        // catch-up spiral of hundreds of updates
        double frameTime = std::min(now - lastTime, 0.25);
        lastTime = now;
        accumulator += frameTime;

        glfwPollEvents();
        if (m_input.keyDown(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(m_window, true);

        // camera look is per-frame (smoothest), simulation is fixed-step
        m_camera.processMouse(m_input.mouseDX(), m_input.mouseDY());
        m_camera.processScroll(m_input.scrollDY());

        while (accumulator >= SIM_DT)
        {
            update(static_cast<float>(SIM_DT));
            accumulator -= SIM_DT;
        }

        m_camera.follow(m_player.position + glm::vec3(0.0f, 1.5f, 0.0f));
        render();
        m_input.endFrame();
    }
    return 0;
}

void Game::update(float dt)
{
    m_player.update(m_input, m_camera, m_world, dt);
}

void Game::render()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;

    glClearColor(0.53f, 0.75f, 0.92f, 1.0f); // sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_renderer->beginFrame(m_camera, aspect);

    // ground
    glm::vec2 ground = m_world.groundSize();
    m_renderer->draw(*m_groundMesh, boxMatrix({ 0.0f, 0.0f, 0.0f }, { ground.x, 1.0f, ground.y }),
                     glm::vec3(1.0f), m_groundTexture.get());

    // buildings and walls
    for (const StaticBox& box : m_world.boxes())
        m_renderer->draw(*m_cubeMesh, boxMatrix(box.center, box.size), box.color);

    // player
    glm::vec3 playerCenter = m_player.position + glm::vec3(0.0f, m_player.size.y * 0.5f, 0.0f);
    m_renderer->draw(*m_cubeMesh, boxMatrix(playerCenter, m_player.size, m_player.yaw),
                     glm::vec3(0.85f, 0.30f, 0.20f));

    // vehicles: each part is the shared cube, transformed into car space
    for (const Vehicle& vehicle : m_vehicles)
    {
        glm::mat4 carMatrix = glm::translate(glm::mat4(1.0f), vehicle.position());
        carMatrix = glm::rotate(carMatrix, glm::radians(vehicle.yaw()), glm::vec3(0.0f, 1.0f, 0.0f));
        for (const VehiclePart& part : vehicle.parts())
        {
            glm::mat4 model = glm::scale(glm::translate(carMatrix, part.offset), part.size);
            m_renderer->draw(*m_cubeMesh, model, part.color);
        }
    }

    glfwSwapBuffers(m_window);
}

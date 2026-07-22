#include "game.hpp"

#include <algorithm>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "Rendering/mesh.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/texture.hpp"
#include "Game/pedestrian.hpp"
#include "Game/waypoint_path.hpp"

namespace
{
    void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height)
    {
        glViewport(0, 0, width, height);
    }
}

Game::Game() = default;

Game::~Game()
{
    // GL resources must be destroyed while the context still exists
    if (m_debugUIReady)
        m_debugUI.shutdown();
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

    // player
    auto player = std::make_unique<Player>();
    player->position = glm::vec3(0.0f, 0.0f, 0.0f);
    m_player = player.get();
    m_actors.push_back(std::move(player));
    m_controlled = m_player;

    // parked cars
    auto addVehicle = [this](VehicleType type, const glm::vec3& pos, float yaw) {
        auto vehicle = std::make_unique<Vehicle>(type, pos, yaw);
        m_vehicles.push_back(vehicle.get());
        m_actors.push_back(std::move(vehicle));
    };
    addVehicle(VehicleType::Taxi, glm::vec3(8.0f, 0.0f, 6.0f), 0.0f);
    addVehicle(VehicleType::Sedan, glm::vec3(-12.0f, 0.0f, 12.0f), 90.0f);
    addVehicle(VehicleType::Van, glm::vec3(18.0f, 0.0f, -14.0f), 180.0f);

    // traffic: cars that patrol a loop entirely on their own (see
    // Vehicle::update - same physics as player-driven, just AI steering
    // instead of real input). Loop hugs the inside of the border walls,
    // clear of every building in World.
    auto addTraffic = [this](VehicleType type, const glm::vec3& start, float yaw,
                              std::vector<glm::vec3> waypoints, float cruiseSpeed) {
        auto vehicle = std::make_unique<Vehicle>(type, start, yaw);
        vehicle->maxSpeed = cruiseSpeed;
        vehicle->setPatrol(WaypointPath(std::move(waypoints)));
        m_vehicles.push_back(vehicle.get());
        m_actors.push_back(std::move(vehicle));
    };
    std::vector<glm::vec3> perimeterLoop = {
        { -70.0f, 0.0f, -47.0f }, { 70.0f, 0.0f, -47.0f }, { 70.0f, 0.0f, 47.0f }, { -70.0f, 0.0f, 47.0f }
    };
    std::vector<glm::vec3> perimeterLoopReversed(perimeterLoop.rbegin(), perimeterLoop.rend());
    addTraffic(VehicleType::Sedan, glm::vec3(-70.0f, 0.0f, -47.0f), 90.0f, perimeterLoop, 8.0f);
    addTraffic(VehicleType::Van, glm::vec3(-70.0f, 0.0f, 47.0f), 90.0f, perimeterLoopReversed, 7.0f);

    // pedestrians: simple wandering NPCs, patrolling short hand-placed routes
    // clear of every building
    auto addPedestrian = [this](const glm::vec3& start, std::vector<glm::vec3> waypoints,
                                 const glm::vec3& color, float speed) {
        auto ped = std::make_unique<Pedestrian>(start, WaypointPath(std::move(waypoints)), color);
        ped->walkSpeed = speed;
        m_actors.push_back(std::move(ped));
    };
    addPedestrian(glm::vec3(-10.0f, 0.0f, -10.0f),
                  { { -10.0f, 0.0f, -10.0f }, { 10.0f, 0.0f, -10.0f }, { 10.0f, 0.0f, 10.0f }, { -10.0f, 0.0f, 10.0f } },
                  glm::vec3(0.75f, 0.60f, 0.50f), 2.0f);
    addPedestrian(glm::vec3(4.0f, 0.0f, 2.0f),
                  { { 4.0f, 0.0f, 2.0f }, { 14.0f, 0.0f, 10.0f } },
                  glm::vec3(0.50f, 0.65f, 0.55f), 1.6f);
    addPedestrian(glm::vec3(20.0f, 0.0f, -10.0f),
                  { { 20.0f, 0.0f, -10.0f }, { 20.0f, 0.0f, 10.0f } },
                  glm::vec3(0.60f, 0.50f, 0.70f), 1.8f);

    // debug UI: panels are registered here, once, by whatever owns the data
    // they show. Adding a new panel elsewhere never touches this file.
    m_debugUI.init(m_window);
    m_debugUIReady = true;

    m_debugUI.addPanel("Debug", [this]() {
        ImGui::Text("FPS: %.0f (%.2f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        if (Vehicle* driving = drivenVehicle())
            ImGui::Text("Driving (speed %.1f)", driving->speed());
        else
            ImGui::Text("Player pos: %.1f, %.1f, %.1f", m_player->position.x, m_player->position.y, m_player->position.z);
        ImGui::Text("Actors: %zu", m_actors.size());
        ImGui::Separator();
        ImGui::Checkbox("Show collision boxes", &m_showColliders);
        ImGui::Checkbox("Show ImGui demo window", &m_showImGuiDemo);
        ImGui::Separator();
        ImGui::TextDisabled("F1 menu, F enter/exit vehicle");
        if (m_showImGuiDemo)
            ImGui::ShowDemoWindow(&m_showImGuiDemo);
    });

    m_debugUI.addPanel("Player", [this]() {
        ImGui::SliderFloat("Walk speed", &m_player->walkSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Run speed", &m_player->runSpeed, 1.0f, 25.0f);
    });

    m_debugUI.addPanel("Camera", [this]() {
        ImGui::SliderFloat("Sensitivity", &m_camera.sensitivity, 0.02f, 0.5f);
        ImGui::SliderFloat("Min distance", &m_camera.minDistance, 1.0f, 10.0f);
        ImGui::SliderFloat("Max distance", &m_camera.maxDistance, 5.0f, 30.0f);
        ImGui::SliderFloat("Min pitch", &m_camera.minPitch, -30.0f, 0.0f);
        ImGui::SliderFloat("Max pitch", &m_camera.maxPitch, 30.0f, 89.0f);
    });

    m_debugUI.addPanel("Vehicle", [this]() {
        Vehicle* vehicle = drivenVehicle();
        if (!vehicle)
        {
            ImGui::TextDisabled("Not driving. Walk up to a car and press F.");
            return;
        }
        ImGui::Text("Speed: %.1f", vehicle->speed());
        ImGui::SliderFloat("Acceleration", &vehicle->acceleration, 2.0f, 40.0f);
        ImGui::SliderFloat("Brake decel.", &vehicle->brakeDeceleration, 2.0f, 40.0f);
        ImGui::SliderFloat("Friction", &vehicle->friction, 0.0f, 20.0f);
        ImGui::SliderFloat("Max speed", &vehicle->maxSpeed, 5.0f, 40.0f);
        ImGui::SliderFloat("Max reverse speed", &vehicle->maxReverseSpeed, 2.0f, 20.0f);
        ImGui::SliderFloat("Turn rate (deg/s)", &vehicle->turnRateDeg, 20.0f, 180.0f);
    });

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
        if (m_input.keyPressed(GLFW_KEY_F1))
            m_debugUI.toggle();

        // poll every frame regardless of the UI, so edge-detection doesn't
        // miss a press/release that happened while the menu was open
        bool enterExitPressed = m_input.keyPressed(GLFW_KEY_F);
        if (enterExitPressed && !m_debugUI.visible())
            enterOrExitVehicle();

        // camera look is per-frame (smoothest), simulation is fixed-step;
        // suppressed while the debug UI is open so the mouse drives it instead
        if (!m_debugUI.visible())
        {
            m_camera.processMouse(m_input.mouseDX(), m_input.mouseDY());
            m_camera.processScroll(m_input.scrollDY());
        }

        while (accumulator >= SIM_DT)
        {
            update(static_cast<float>(SIM_DT));
            accumulator -= SIM_DT;
        }

        Vehicle* driving = drivenVehicle();
        glm::vec3 followTarget = driving
            ? driving->position() + glm::vec3(0.0f, 1.2f, 0.0f)
            : m_player->position + glm::vec3(0.0f, 1.5f, 0.0f);
        m_camera.follow(followTarget);
        render();
        m_input.endFrame();
    }
    return 0;
}

void Game::enterOrExitVehicle()
{
    if (Vehicle* current = drivenVehicle())
    {
        // exit: step out to the vehicle's right side, facing the same way it is
        glm::vec3 right = glm::normalize(glm::cross(current->forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
        m_player->position = current->position() + right * 2.2f;
        m_player->yaw = current->yaw();
        m_controlled = m_player;
        return;
    }

    // enter: nearest vehicle within range
    const float ENTER_RANGE = 3.5f;
    Vehicle* nearest = nullptr;
    float nearestDist = ENTER_RANGE;
    for (Vehicle* vehicle : m_vehicles)
    {
        float dist = glm::length(vehicle->position() - m_player->position);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = vehicle;
        }
    }
    if (nearest)
        m_controlled = nearest;
}

std::function<bool(const AABB&)> Game::collisionPredicateFor(const Actor* self) const
{
    return [this, self](const AABB& box) {
        if (m_world.collides(box))
            return true;
        for (const auto& actor : m_actors)
        {
            Actor* other = actor.get();
            if (other == self)
                continue;
            // the player isn't a physical obstacle while riding inside a vehicle
            if (other == static_cast<Actor*>(m_player) && m_controlled != static_cast<Actor*>(m_player))
                continue;
            if (other->aabb().intersects(box))
                return true;
        }
        return false;
    };
}

Vehicle* Game::drivenVehicle() const
{
    if (m_controlled == nullptr || m_controlled == static_cast<Actor*>(m_player))
        return nullptr;
    // m_controlled is always either m_player or one of m_vehicles, so this is safe
    return static_cast<Vehicle*>(m_controlled);
}

void Game::update(float dt)
{
    // suppressed while the debug UI is open, so tweaking a slider doesn't
    // also walk the player or drive the car
    if (m_debugUI.visible())
        return;

    for (auto& actor : m_actors)
    {
        ActorContext ctx{ m_input, m_camera, actor.get() == m_controlled, collisionPredicateFor(actor.get()) };
        actor->update(ctx, dt);
    }
}

void Game::render()
{
    m_debugUI.beginFrame(); // builds this frame's panels (no-op while hidden)

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;

    glClearColor(0.53f, 0.75f, 0.92f, 1.0f); // sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_renderer->beginFrame(m_camera, aspect);

    // ground
    glm::vec2 ground = m_world.groundSize();
    m_renderer->draw(*m_groundMesh, Mesh::boxMatrix({ 0.0f, 0.0f, 0.0f }, { ground.x, 1.0f, ground.y }),
                     glm::vec3(1.0f), m_groundTexture.get());

    // buildings and walls
    for (const StaticBox& box : m_world.boxes())
        m_renderer->draw(*m_cubeMesh, Mesh::boxMatrix(box.center, box.size), box.color);

    // every actor draws itself; Player no-ops while riding in a vehicle
    for (const auto& actor : m_actors)
        actor->render(*m_renderer, *m_cubeMesh, actor.get() == m_controlled);

    // collision debug view: every AABB actually used by the collision
    // predicates, drawn as a wireframe so it can be checked against the
    // visible geometry
    if (m_showColliders)
    {
        auto drawAABBWire = [this](const AABB& box, const glm::vec3& color) {
            glm::vec3 center = (box.min + box.max) * 0.5f;
            glm::vec3 size = box.max - box.min;
            m_renderer->draw(*m_cubeMesh, Mesh::boxMatrix(center, size), color);
        };

        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        for (const StaticBox& box : m_world.boxes())
            drawAABBWire(AABB::fromCenterHalf(box.center, box.size * 0.5f), glm::vec3(0.1f, 1.0f, 0.2f));

        for (const auto& actor : m_actors)
        {
            // the player isn't a physical presence while riding, so its
            // stale on-foot box would be misleading here
            if (actor.get() == static_cast<Actor*>(m_player) && m_controlled != static_cast<Actor*>(m_player))
                continue;
            glm::vec3 color = actor.get() == m_controlled ? glm::vec3(1.0f, 0.9f, 0.1f) : glm::vec3(0.2f, 0.6f, 1.0f);
            drawAABBWire(actor->aabb(), color);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }

    m_debugUI.render(); // draws the UI on top of everything above

    glfwSwapBuffers(m_window);
}

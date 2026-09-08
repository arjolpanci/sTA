#include "game.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "Rendering/mesh.hpp"
#include "Rendering/model_preview.hpp"
#include "Rendering/island_renderer.hpp"
#include "Rendering/scene_renderer.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/shadow_map.hpp"
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
    m_islandRenderer.reset();
    m_sceneRenderer.reset();
    m_rampMesh.reset();
    m_mapTexture.reset();
    m_shadowMap.reset();
    if (m_window)
        glfwTerminate();
}

bool Game::init()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    if (m_smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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
    m_islandRenderer = std::make_unique<IslandRenderer>(m_world.terrain());
    m_sceneRenderer = std::make_unique<SceneRenderer>(m_world);
    m_rampMesh = std::make_unique<Mesh>(Mesh::rampVertices());
    m_mapTexture = std::make_unique<Texture>("resources/maps/island-overview.png");
    m_shadowMap = std::make_unique<ShadowMap>();

    // player
    auto player = std::make_unique<Player>();
    player->position = glm::vec3(0.0f, 8.0f, 0.0f);
    m_player = player.get();
    m_actors.push_back(std::move(player));
    m_controlled = m_player;

    // Actor placements and patrols are part of the saved map asset.
    for (const auto& spawn : m_world.vehicleSpawns())
    {
        auto car=std::make_unique<Vehicle>(static_cast<VehicleType>(spawn.type), spawn.route.front(), spawn.yaw);
        if (spawn.speed > 0) {car->maxSpeed=spawn.speed; car->setPatrol(WaypointPath(spawn.route));}
        m_vehicles.push_back(car.get()); m_actors.push_back(std::move(car));
    }
    int pedestrianModel=0;
    for (const auto& spawn : m_world.pedestrianSpawns())
    {
        auto ped=std::make_unique<Pedestrian>(spawn.route.front(), WaypointPath(spawn.route), spawn.color, pedestrianModel++);
        ped->walkSpeed=spawn.speed; m_actors.push_back(std::move(ped));
    }

    // debug UI: panels are registered here, once, by whatever owns the data
    // they show. Adding a new panel elsewhere never touches this file.
    m_debugUI.init(m_window);
    m_debugUIReady = true;

    m_debugUI.addPanel("Overview", [this]() {
        ImGui::Text("FPS: %.0f (%.2f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        if (Vehicle* driving = drivenVehicle())
            ImGui::Text("Driving (speed %.1f)", driving->speed());
        else
        {
            ImGui::Text("Player pos: %.1f, %.1f, %.1f", m_player->position.x, m_player->position.y, m_player->position.z);
            ImGui::Text("Grounded: %s", m_player->isGrounded() ? "yes" : "no");
        }
        ImGui::Text("Actors: %zu  |  Vehicles: %zu", m_actors.size(), m_vehicles.size());
        ImGui::Text("Courier mission: %s", m_deliveryActive ? "Active" : "Inactive (free roam)");
        if (ImGui::Button("Open mission controls")) m_debugUI.selectPanel("Missions");
        ImGui::TextWrapped("Use the sidebar to tune movement, inspect vehicles, or start a mission. Changes apply immediately; F1 resumes play.");
        ImGui::Separator();
        ImGui::Checkbox("Show collision boxes", &m_showColliders);
        ImGui::Checkbox("Show ImGui demo window", &m_showImGuiDemo);
        ImGui::Separator();
        ImGui::TextDisabled("F1 menu, F enter/exit vehicle");

    });

    m_debugUI.addPanel("Missions", [this]() {
        ImGui::TextUnformatted("Courier run");
        ImGui::TextWrapped("Drive any car to the teal destination and stop for 1.5 seconds to earn $150. Five stops repeat around the city.");
        ImGui::Text("Status: %s", m_deliveryActive ? "Active" : "Inactive - free roam");
        ImGui::Text("Run earnings: $%d  |  Deliveries: %d", m_cash, m_deliveries);
        if (m_deliveryActive)
        {
            const auto& stop = m_deliveryStops[m_deliveryIndex];
            ImGui::Text("Stop %zu / %zu  |  X %.0f, Z %.0f", m_deliveryIndex + 1, m_deliveryStops.size(), stop.x, stop.z);
            ImGui::ProgressBar(m_deliveryHold / 1.5f, {-1, 0}, "Delivery hold");
            if (ImGui::Button("Stop courier run")) stopCourierRun();
            ImGui::TextWrapped("Stopping removes the objective and markers. Earnings remain here until you start a new run.");
        }
        else
        {
            if (ImGui::Button("Start courier run")) startCourierRun();
            ImGui::SameLine();
            if (ImGui::Button("Start and play")) { startCourierRun(); m_debugUI.setVisible(false); }
            ImGui::TextWrapped("A new run starts at stop 1 with zero deliveries and earnings. Missions are always inactive at launch.");
        }
    });

    m_debugUI.addPanel("Player", [this]() {
        ImGui::SliderFloat("Walk speed", &m_player->walkSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Run speed", &m_player->runSpeed, 1.0f, 25.0f);
        ImGui::SliderFloat("Jump speed", &m_player->jumpSpeed, 3.0f, 20.0f);
        if (ImGui::Button("Reset movement tuning"))
        {
            Player defaults;
            m_player->walkSpeed = defaults.walkSpeed;
            m_player->runSpeed = defaults.runSpeed;
            m_player->jumpSpeed = defaults.jumpSpeed;
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Quick travel (on foot)");
        if (drivenVehicle()) ImGui::TextWrapped("Exit your vehicle before using quick travel.");
        else
        {
            auto travel = [this](const char* label, glm::vec3 destination) {
                if (!ImGui::Button(label)) return;
                glm::vec3 previous = m_player->position;
                m_player->position = destination;
                if (collisionPredicateFor(m_player)(m_player->collisionBox())) m_player->position = previous;
                else m_player->resetMotion();
            };
            travel("City spawn", {0, 8, 0});
            ImGui::SameLine(); travel("Ramp yard", {-98, 8.16f, 20});
            ImGui::SameLine(); travel("Park", {30, 8.16f, 20});
            travel("West bridge", {-480, 12, 120});
            ImGui::SameLine(); travel("Highland", {0, 32, -330});
            ImGui::SameLine(); travel("Mountain lookout", {130, 110, -500});
            travel("East gardens", {390, 26, 0});
            ImGui::SameLine(); travel("Beach", {0, m_world.groundHeightAt(0, 560), 560});
        }
    });

    m_debugUI.addPanel("Camera", [this]() {
        ImGui::SliderFloat("Sensitivity", &m_camera.sensitivity, 0.02f, 0.5f);
        ImGui::SliderFloat("Min distance", &m_camera.minDistance, 1.0f, 10.0f);
        ImGui::SliderFloat("Max distance", &m_camera.maxDistance, 5.0f, 1200.0f, "%.0f m", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Min pitch", &m_camera.minPitch, -30.0f, 0.0f);
        ImGui::SliderFloat("Max pitch", &m_camera.maxPitch, 30.0f, 89.0f);
        m_camera.maxDistance = std::max(m_camera.maxDistance, m_camera.minDistance);
        if (ImGui::Button("Reset camera tuning"))
        {
            Camera defaults;
            m_camera.sensitivity = defaults.sensitivity;
            m_camera.minDistance = defaults.minDistance;
            m_camera.maxDistance = defaults.maxDistance;
            m_camera.minPitch = defaults.minPitch;
            m_camera.maxPitch = defaults.maxPitch;
        }
    });

    m_debugUI.addPanel("Island", [this]() {
        const auto& terrain=m_world.terrain();
        auto position=m_controlled->collisionBox().center;
        ImGui::Text("Baked map: %.0f x %.0f m  |  Sea level %.0f m", terrain.extent(), terrain.extent(), terrain.seaLevel());
        ImGui::Text("Terrain here: %.1f m  |  Roads: %zu", terrain.heightAt(position.x,position.z),m_world.roads().size());
        ImGui::TextWrapped("Downtown in the center; West harbor across the bridges; East gardens on the eastern terrace; Highland and the lookout in the north. Quick travel is on the Player page.");
        if (ImGui::Button("Aerial camera")) {
            m_camera=Camera(); m_camera.maxDistance=1800;
            m_camera.processScroll(-1200); m_camera.processMouse(0,400);
            m_camera.follow(position);
        }
        ImGui::SameLine();
        if (ImGui::Button("Street camera")) {m_camera=Camera(); m_camera.follow(position);}
        float size=std::min(420.0f,ImGui::GetContentRegionAvail().x);
        ImGui::Image((ImTextureID)(intptr_t)m_mapTexture->id(),{size,size},{0,1},{1,0});
        ImGui::TextWrapped("Saved asset, never regenerated during startup. Authoring command: python3 tools/build_island.py");
    });

    m_debugUI.addPanel("Rendering", [this]() {
        ImGui::TextUnformatted("Baked island: 2048 m / 513 x 513 samples");
        ImGui::TextWrapped("The game loads resources/maps/island.bin and island.scene. Rebuild deliberately with tools/build_island.py; no terrain is generated at startup.");
        ImGui::SliderFloat("Wave strength", &m_waveStrength, 0.0f, 2.0f);
        ImGui::Checkbox("Show game HUD and minimap", &m_showHUD);
        ImGui::Checkbox("Show collision boxes", &m_showColliders);
        ImGui::Checkbox("Shadows enabled", &m_shadowsEnabled);
        ImGui::Checkbox("Show shadow map", &m_showShadowMapPreview);
        if (m_showShadowMapPreview)
            ImGui::Image((ImTextureID)(intptr_t)m_shadowMap->depthTexture(), ImVec2(300, 300));
    });

    m_debugUI.addPanel("Vehicle", [this]() {
        ImGui::TextWrapped("Inspect and tune any vehicle, including parked cars and AI traffic.");
        ImGui::SliderInt("Vehicle number", &m_debugVehicleIndex, 0, static_cast<int>(m_vehicles.size()) - 1);
        if (Vehicle* driving = drivenVehicle())
            if (ImGui::Button("Select driven vehicle"))
                m_debugVehicleIndex = static_cast<int>(std::find(m_vehicles.begin(), m_vehicles.end(), driving) - m_vehicles.begin());
        Vehicle* vehicle = m_vehicles[m_debugVehicleIndex];
        const auto pos = vehicle->position();
        ImGui::Text("Position: %.1f, %.1f, %.1f  |  Heading %.0f", pos.x, pos.y, pos.z, vehicle->yaw());
        ImGui::Text("Speed: %.1f km/h%s", vehicle->speed() * 3.6f, vehicle == drivenVehicle() ? " (player driving)" : "");
        ImGui::SliderFloat("Acceleration", &vehicle->acceleration, 2.0f, 40.0f);
        ImGui::SliderFloat("Brake decel.", &vehicle->brakeDeceleration, 2.0f, 40.0f);
        ImGui::SliderFloat("Friction", &vehicle->friction, 0.0f, 20.0f);
        ImGui::SliderFloat("Max speed", &vehicle->maxSpeed, 5.0f, 40.0f);
        ImGui::SliderFloat("Max reverse speed", &vehicle->maxReverseSpeed, 2.0f, 20.0f);
        ImGui::SliderFloat("Turn rate (deg/s)", &vehicle->turnRateDeg, 20.0f, 180.0f);
    });

    return true;
}

int Game::run(bool smokeTest, bool benchmark)
{
    m_smokeTest = smokeTest || benchmark;
    if (!init())
        return -1;

    if (benchmark)
    {
        glfwSwapInterval(0);
        using Clock=std::chrono::steady_clock;
        auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
        m_debugUI.setVisible(false);
        m_camera.follow(m_player->position+glm::vec3(0,1.5f,0));
        for(bool active:{false,true}) {
            std::vector<double> frames;double simulation=0,rendering=0,gpuWait=0,poses=0,culled=0,uploads=0;
            for(int frame=0;frame<140;++frame) {
                glfwPollEvents();auto a=Clock::now();
                if(active)update(1.0f/60);
                auto b=Clock::now();render();auto c=Clock::now();glFinish();auto d=Clock::now();
                if(frame>=20){frames.push_back(ms(a,d));simulation+=ms(a,b);rendering+=ms(b,c);gpuWait+=ms(c,d);poses+=m_renderer->stats.poseUpdates;culled+=m_renderer->stats.culledModels;uploads+=m_renderer->stats.paletteBytes;}
            }
            std::sort(frames.begin(),frames.end());
            std::cout<<std::fixed<<std::setprecision(2)<<"BENCH "<<(active?"active":"paused")
                <<" median_ms="<<frames[60]<<" p95_ms="<<frames[114]
                <<" update_ms="<<simulation/120<<" render_ms="<<rendering/120<<" gpu_wait_ms="<<gpuWait/120<<" poses="<<poses/120<<" culled_model_passes="<<culled/120<<" upload_kb="<<uploads/120/1024<<'\n';
        }
        return 0;
    }
    if (m_smokeTest)
    {
#ifdef STA_DEBUG_BUILD
        if (!m_debugUI.visible()) throw std::runtime_error("Debug UI must open in Debug builds");
#else
        if (m_debugUI.visible()) throw std::runtime_error("Debug UI must start hidden outside Debug builds");
#endif
        if (glfwGetInputMode(m_window, GLFW_CURSOR) != (m_debugUI.visible() ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED))
            throw std::runtime_error("Startup cursor mode does not match the debug UI");
        if (m_deliveryActive) throw std::runtime_error("Missions must start inactive");
        m_debugUI.setVisible(false);
        auto testCar = std::make_unique<Vehicle>(VehicleType::Sedan, m_deliveryStops.front(), 0);
        m_controlled = testCar.get();
        m_vehicles.push_back(testCar.get());
        m_actors.push_back(std::move(testCar));
        auto simulate = [this]() { for (int i = 0; i < 110; ++i) update(1.0f / 60); };
        simulate();
        if (m_cash != 0 || m_deliveries != 0) throw std::runtime_error("Inactive mission awarded a delivery");
        startCourierRun();
        simulate();
        if (m_cash != 150 || m_deliveries != 1) throw std::runtime_error("Triggered mission did not award a delivery");
        stopCourierRun();
        simulate();
        if (m_deliveryActive || m_deliveryHold != 0 || m_noticeTime != 0 || m_cash != 150)
            throw std::runtime_error("Stopping a mission did not clear transient state");
        startCourierRun();
        if (m_cash != 0 || m_deliveries != 0 || m_deliveryIndex != 0)
            throw std::runtime_error("New courier run did not reset progress");
        stopCourierRun();
        auto* recoveryCar=drivenVehicle();
        recoveryCar->recover({0,-3,700});
        simulate();
        if (recoveryCar->position().y<0 || m_waterRecoveryNotice<=0)
            throw std::runtime_error("Submerged vehicle recovery failed");
        m_controlled = m_player;
        m_player->position={0,-3,700};
        m_player->resetMotion();
        simulate();
        if (!m_player->isSwimming() || std::abs(m_player->position.y+.9f)>.01f)
            throw std::runtime_error("Swimming buoyancy failed");
        m_player->position={0,8,0}; m_player->resetMotion();
        m_waterRecoveryNotice=0;
        m_vehicles.pop_back();
        m_actors.pop_back();
        m_camera.follow(m_player->position + glm::vec3(0,1.5f,0));
        m_debugUI.selectPanel("Overview");
        render();
        m_capturePath = "smoke-debug.ppm";
        render();
        m_debugUI.selectPanel("Missions");
        m_capturePath = "smoke-missions.ppm";
        render();
        m_debugUI.setVisible(false);
        m_capturePath = nullptr;
        m_camera.processScroll(-5);
        m_camera.follow(m_player->position + glm::vec3(0,1.5f,0));
        render(); // let ImGui settle its first-frame automatic sizing
        m_capturePath = "smoke-city.ppm";
        render();
        m_player->position = {-98, 11.66f, 37};
        m_camera.processMouse(-500, 150);
        m_camera.follow(m_player->position + glm::vec3(0,1.5f,0));
        m_capturePath = "smoke-ramp.ppm";
        render();
        m_camera = Camera();
        m_camera.maxDistance=1800;
        m_camera.processScroll(-1350);
        m_camera.processMouse(250,400);
        m_camera.follow({0,30,0});
        m_capturePath="smoke-island.ppm";
        render();
        m_player->position={-480,12,120};
        m_camera=Camera(); m_camera.maxDistance=150;
        m_camera.processScroll(-65); m_camera.processMouse(600,150);
        m_camera.follow({-430,12,120});
        m_capturePath="smoke-bridge.ppm";
        render();
        m_player->position={0,m_world.groundHeightAt(0,560),560};
        m_camera=Camera(); m_camera.maxDistance=150;
        m_camera.processScroll(-25); m_camera.processMouse(0,100);
        m_camera.follow({0,2,560});
        m_capturePath="smoke-shore.ppm";
        render();
        m_worldTime+=2;
        m_capturePath="smoke-shore-next.ppm";
        render();
        int previewWidth,previewHeight;glfwGetFramebufferSize(m_window,&previewWidth,&previewHeight);
        captureModelPreviews(*m_renderer,*m_shadowMap,previewWidth,previewHeight);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL smoke test failed");
        std::cout << "Startup, mission lifecycle and rendering smoke tests passed: smoke-debug.ppm, smoke-missions.ppm, smoke-city.ppm, smoke-ramp.ppm, smoke-island.ppm, smoke-bridge.ppm, smoke-shore.ppm\n";
        return 0;
    }

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
        m_camera.avoidObstacles([this](const glm::vec3& point) {
            return point.y < m_world.terrain().seaLevel() + 0.2f || m_world.collides(CollisionBox::fromCenterHalf(point, glm::vec3(0.2f)));
        });
        render();
        m_input.endFrame();
    }
    return 0;
}

void Game::enterOrExitVehicle()
{
    if (Vehicle* current = drivenVehicle())
    {
        if (std::abs(current->speed()) > 2.0f) return;
        glm::vec3 right(-current->forward().z, 0.0f, current->forward().x);
        glm::vec3 oldPosition = m_player->position;
        for (glm::vec3 offset : {right * 2.2f, -right * 2.2f, -current->forward() * 3.5f})
        {
            m_player->position = current->position() + offset;
            float ground = m_world.groundHeightAt(m_player->position.x, m_player->position.z);
            if (std::abs(ground - current->position().y) > MAX_STEP_DOWN) continue;
            m_player->position.y = ground;
            if (collisionPredicateFor(m_player)(m_player->collisionBox())) continue;
            m_player->yaw = current->yaw();
            m_player->resetMotion();
            m_controlled = m_player;
            return;
        }
        m_player->position = oldPosition;
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
    if (nearest && std::abs(nearest->speed()) < 2.0f)
    {
        nearest->takeControl();
        m_controlled = nearest;
    }
}

std::function<bool(const CollisionBox&)> Game::collisionPredicateFor(const Actor* self) const
{
    return [this, self](const CollisionBox& box) {
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
            if (other->collisionBox().intersects(box))
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


    m_worldTime += dt;
    m_waterRecoveryNotice = std::max(0.0f, m_waterRecoveryNotice - dt);
    m_noticeTime = std::max(0.0f, m_noticeTime - dt);
    for (auto& actor : m_actors)
    {
        auto groundHeightAt = [this, &actor](float x, float z) {
            auto box = actor->collisionBox();
            box.center.x = x;
            box.center.z = z;
            return m_world.supportHeight(box, box.center.y - box.half.y + MAX_STEP_UP);
        };
        ActorContext ctx{ m_input, m_camera, actor.get() == m_controlled, collisionPredicateFor(actor.get()), [this](const glm::vec3& feet) { return m_world.surfaceNormal(feet); }, groundHeightAt, m_world.terrain().seaLevel() };
        actor->update(ctx, dt);
    }
    for (Vehicle* car : m_vehicles)
    {
        if (car->position().y >= m_world.terrain().seaLevel()-1.2f) continue;
        for (glm::vec3 offset : {glm::vec3(0), glm::vec3(0,0,6), glm::vec3(0,0,-6), glm::vec3(6,0,0), glm::vec3(-6,0,0)})
        {
            glm::vec3 candidate=car->spawnPosition()+offset;
            auto box=car->collisionBox(); box.center=candidate+glm::vec3(0,box.half.y,0); box.yaw=car->spawnYaw();
            if (collisionPredicateFor(car)(box)) continue;
            car->recover(candidate);
            if (car==drivenVehicle()) m_waterRecoveryNotice=5;
            break;
        }
    }
    Vehicle* driving = drivenVehicle();
    if (m_deliveryActive && driving && glm::length(driving->position() - m_deliveryStops[m_deliveryIndex]) < 4.5f && std::abs(driving->speed()) < 1.0f)
    {
        m_deliveryHold += dt;
        if (m_deliveryHold >= 1.5f)
        {
            ++m_deliveries;
            m_cash += 150;
            m_deliveryIndex = (m_deliveryIndex + 1) % m_deliveryStops.size();
            m_deliveryHold = 0.0f;
            m_noticeTime = 4.0f;
        }
    }
    else m_deliveryHold = 0.0f;

}

void Game::render()
{
    m_debugUI.beginFrame(); // HUD frame plus optional debug panels

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;

    m_renderer->stats={};
    m_renderer->setCamera(m_camera,aspect);
    glm::mat4 lightSpaceMatrix = ShadowMap::lightSpaceMatrix(m_sunDirection, m_controlled->collisionBox().center, 95.0f);

    // shadow pass: depth only, from the sun's point of view. Runs every
    // frame regardless of m_shadowsEnabled, which only gates whether the
    // main pass *samples* the result - otherwise the map would show stale
    // shadows (or undefined initial contents) from whenever it was last on.
    // Everything that can cast a shadow is drawn with the same model
    // matrices as the main pass below; the ground plane is skipped since
    // nothing is under it to shadow.
    m_shadowMap->beginCapture();
    m_renderer->beginShadowPass(lightSpaceMatrix);

    m_islandRenderer->drawShadow(*m_renderer, m_controlled->collisionBox().center);
    m_sceneRenderer->drawShadow(*m_renderer, m_controlled->collisionBox().center);

    for (const Ramp& ramp : m_world.ramps())
    {
        glm::vec3 center(ramp.footprintCenter.x, (ramp.lowHeight + ramp.highHeight) * 0.5f, ramp.footprintCenter.z);
        glm::vec3 size(ramp.footprintSize.x, ramp.highHeight - ramp.lowHeight, ramp.footprintSize.y);
        m_renderer->drawShadow(*m_rampMesh, Mesh::boxMatrix(center, size, ramp.alongX ? 90.0f : 0.0f));
    }

    for (const auto& actor : m_actors)
        actor->renderShadow(*m_renderer, *m_cubeMesh, actor.get() == m_controlled);

    m_shadowMap->endCapture(width, height);

    // main pass
    glClearColor(0.60f, 0.73f, 0.79f, 1.0f); // sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_islandRenderer->drawTerrain(m_camera, aspect, lightSpaceMatrix, m_sunDirection, *m_shadowMap, m_shadowsEnabled);
    m_renderer->beginFrame(m_camera, aspect, lightSpaceMatrix, m_sunDirection, *m_shadowMap, m_shadowsEnabled);
    m_sceneRenderer->draw(*m_renderer, m_camera.position());

    // ramps: the unit wedge rises along local +Z, flush with the ground at
    // -Z - yaw re-orients that to whichever world axis the ramp climbs
    for (const Ramp& ramp : m_world.ramps())
    {
        glm::vec3 center(ramp.footprintCenter.x, (ramp.lowHeight + ramp.highHeight) * 0.5f, ramp.footprintCenter.z);
        glm::vec3 size(ramp.footprintSize.x, ramp.highHeight - ramp.lowHeight, ramp.footprintSize.y);
        m_renderer->draw(*m_rampMesh, Mesh::boxMatrix(center, size, ramp.alongX ? 90.0f : 0.0f), Material{ ramp.color });
    }

    // A bright curbside destination marker, drawn above the road surface.
    glm::vec3 destination = m_deliveryStops[m_deliveryIndex];
    if (m_deliveryActive)
    for (int side : {-1, 1})
    {
        m_renderer->draw(*m_cubeMesh, Mesh::boxMatrix(destination + glm::vec3(side * 3.0f, 0.06f, 0), {0.18f, 0.08f, 6}), Material{{0.25f, 0.85f, 0.72f}});
        m_renderer->draw(*m_cubeMesh, Mesh::boxMatrix(destination + glm::vec3(0, 0.06f, side * 3.0f), {6, 0.08f, 0.18f}), Material{{0.25f, 0.85f, 0.72f}});
    }

    // every actor draws itself; Player no-ops while riding in a vehicle
    for (const auto& actor : m_actors)
        actor->render(*m_renderer, *m_cubeMesh, actor.get() == m_controlled);

    m_islandRenderer->drawWater(m_camera, aspect, m_sunDirection, m_worldTime, m_waveStrength);

    // collision debug view: every CollisionBox actually used by the collision
    // predicates, drawn as a wireframe so it can be checked against the
    // visible geometry
    if (m_showColliders)
    {
        m_renderer->beginFrame(m_camera, aspect, lightSpaceMatrix, m_sunDirection, *m_shadowMap, m_shadowsEnabled);
        auto drawCollisionBoxWire = [this](const CollisionBox& box, const glm::vec3& color) {
            glm::vec3 center = box.center;
            glm::vec3 size = box.half * 2.0f;
            // wireframes read better with the far edges left in, so the box is
            // drawn double-sided - the renderer owns that state now
            Material material{ color };
            material.doubleSided = true;
            m_renderer->draw(*m_cubeMesh, Mesh::boxMatrix(center, size, box.yaw), material);
        };

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        for (const StaticBox& box : m_world.boxes())
            drawCollisionBoxWire(CollisionBox::fromCenterHalf(box.center, box.size * 0.5f, box.yaw), glm::vec3(0.1f, 1.0f, 0.2f));

        // ramps: shown as their overall bounding volume (low to high end) -
        // the actual ramp collision follows the wedge surface
        for (const Ramp& ramp : m_world.ramps())
        {
            glm::vec3 center(ramp.footprintCenter.x, (ramp.lowHeight + ramp.highHeight) * 0.5f, ramp.footprintCenter.z);
            glm::vec3 half(ramp.footprintSize.x * 0.5f, (ramp.highHeight - ramp.lowHeight) * 0.5f, ramp.footprintSize.y * 0.5f);
            if (ramp.alongX)
                std::swap(half.x, half.z);
            drawCollisionBoxWire(CollisionBox::fromCenterHalf(center, half), glm::vec3(1.0f, 0.6f, 0.1f));
        }

        for (const auto& actor : m_actors)
        {
            // the player isn't a physical presence while riding, so its
            // stale on-foot box would be misleading here
            if (actor.get() == static_cast<Actor*>(m_player) && m_controlled != static_cast<Actor*>(m_player))
                continue;
            glm::vec3 color = actor.get() == m_controlled ? glm::vec3(1.0f, 0.9f, 0.1f) : glm::vec3(0.2f, 0.6f, 1.0f);
            drawCollisionBoxWire(actor->collisionBox(), color);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (m_showImGuiDemo && m_debugUI.visible()) ImGui::ShowDemoWindow(&m_showImGuiDemo);
    if (m_showHUD && !m_debugUI.visible()) renderHUD();
    m_debugUI.render(); // draws the UI on top of everything above

    if (m_capturePath)
    {
        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        std::ofstream file(m_capturePath, std::ios::binary);
        file << "P6\n" << width << " " << height << "\n255\n";
        for (int row = height - 1; row >= 0; --row)
            file.write(reinterpret_cast<const char*>(pixels.data() + static_cast<size_t>(row) * width * 3), width * 3);
        if (!file) throw std::runtime_error("Could not write smoke screenshot");
    }
    glfwSwapBuffers(m_window);
}

void Game::renderHUD()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({20, 20});
    ImGui::SetNextWindowBgAlpha(0.85f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Street HUD", nullptr, flags);
    ImGui::TextColored({0.38f, 0.9f, 0.78f, 1}, m_deliveryActive ? "SMALL THEFT AUTO / COURIER RUN" : "SMALL THEFT AUTO / FREE ROAM");
    if (m_deliveryActive) ImGui::Text("$%d   |   Deliveries %d", m_cash, m_deliveries);
    Vehicle* driving = drivenVehicle();
    glm::vec3 pos = m_controlled->collisionBox().center;
    glm::vec3 target = m_deliveryStops[m_deliveryIndex];
    ImGui::Separator();
    if (driving)
    {
        if (m_deliveryActive)
        {
            ImGui::Text("%3.0f km/h   |   Destination %.0f m", std::abs(driving->speed()) * 3.6f,
                        glm::length(glm::vec2(pos.x - target.x, pos.z - target.z)));
            ImGui::TextUnformatted("Stop in the teal marker to deliver ($150).");
            if (m_deliveryHold > 0) ImGui::ProgressBar(m_deliveryHold / 1.5f, {260, 8}, "");
        }
        else ImGui::Text("%3.0f km/h", std::abs(driving->speed()) * 3.6f);
        ImGui::TextDisabled("W/S gas / brake / reverse  |  A/D steer");
        ImGui::TextDisabled("F exit when stopped  |  Space handbrake");
    }
    else
    {
        if (m_deliveryActive) ImGui::TextUnformatted("Find a parked car. Drive to the teal destination.");
        bool near = false;
        for (const Vehicle* vehicle : m_vehicles)
            near |= glm::length(vehicle->position() - m_player->position) < 3.5f && std::abs(vehicle->speed()) < 2;
        if (near) ImGui::TextColored({1, 0.85f, 0.4f, 1}, "F  Enter vehicle");
        ImGui::TextDisabled("WASD move  |  Shift run  |  Space jump");
    }
    if (m_deliveryActive && m_noticeTime > 0) ImGui::TextColored({0.4f, 1, 0.7f, 1}, "Delivery complete! +$150. Next stop marked.");
    if (m_player->isSwimming() && !driving) ImGui::TextUnformatted("Swimming - head toward the beach to leave the water.");
    if (m_waterRecoveryNotice > 0) ImGui::TextUnformatted("Vehicle recovered from water to its starting street.");
    ImGui::TextDisabled("Mouse orbit  |  Scroll zoom  |  F1 debug");
    ImGui::End();

    // North-up map: world +Z points down, matching the street grid.
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 origin(io.DisplaySize.x - 240, 20);
    float mapScale=220.0f/m_world.terrain().extent();
    auto point = [origin,mapScale](float x, float z) { return ImVec2(origin.x + 110 + std::clamp(x * mapScale,-108.0f,108.0f), origin.y + 110 + std::clamp(z * mapScale,-108.0f,108.0f)); };
    draw->AddImage((ImTextureID)(intptr_t)m_mapTexture->id(), origin, {origin.x+220,origin.y+220}, {0,1}, {1,0});
    for (const StaticBox& box : m_world.boxes()) {
        if (!box.facade) continue;
        draw->AddRectFilled(point(box.center.x-box.size.x/2,box.center.z-box.size.z/2),
                            point(box.center.x+box.size.x/2,box.center.z+box.size.z/2),IM_COL32(165,166,147,255));
    }
    for (const Road& road : m_world.roads())
        for (size_t i=1;i<road.route.size();++i) {
            auto a=road.route[i-1],b=road.route[i],mid=(a+b)*.5f;
            if (m_world.terrain().heightAt(mid.x,mid.z)<0)
                draw->AddLine(point(a.x,a.z),point(b.x,b.z),IM_COL32(185,185,169,255),2);
        }
    for (const Vehicle* vehicle : m_vehicles)
        draw->AddCircleFilled(point(vehicle->position().x, vehicle->position().z), 1.8f, IM_COL32(239, 193, 85, 255));
    if (m_deliveryActive) draw->AddCircle(point(target.x, target.z), 5, IM_COL32(70, 245, 190, 255), 16, 2);
    float yaw = driving ? driving->yaw() : m_player->yaw;
    glm::vec2 f(std::sin(glm::radians(yaw)), std::cos(glm::radians(yaw)));
    glm::vec2 r(f.y, -f.x), c(pos.x, pos.z);
    glm::vec2 tip = c + f * 45.0f, left = c - f * 25.0f - r * 25.0f, right = c - f * 25.0f + r * 25.0f;
    draw->AddTriangleFilled(point(tip.x, tip.y), point(left.x, left.y), point(right.x, right.y), IM_COL32(255, 255, 255, 255));
    draw->AddText({origin.x + 8, origin.y + 6}, IM_COL32(210, 225, 230, 255), "N ^   ISLAND");
    draw->AddText({origin.x + 6, origin.y + 224}, IM_COL32(220, 232, 236, 255), "F1: quick travel / island tools");
}

void Game::startCourierRun()
{
    m_deliveryActive = true;
    m_deliveryIndex = 0;
    m_deliveries = 0;
    m_cash = 0;
    m_deliveryHold = 0;
    m_noticeTime = 0;
}

void Game::stopCourierRun()
{
    m_deliveryActive = false;
    m_deliveryHold = 0;
    m_noticeTime = 0;
}

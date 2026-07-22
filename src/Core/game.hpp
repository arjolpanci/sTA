#ifndef GAME_H
#define GAME_H

#include <functional>
#include <memory>
#include <vector>

#include "Core/input.hpp"
#include "Core/debug_ui.hpp"
#include "Rendering/camera.hpp"
#include "Game/world.hpp"
#include "Game/actor.hpp"
#include "Game/player.hpp"
#include "Game/vehicle.hpp"

struct GLFWwindow;
class Renderer;
class Mesh;
class Texture;
class ShadowMap;

// Owns the window and everything in the scene, and runs the main loop:
// fixed-timestep simulation (so gameplay is framerate-independent) with
// rendering once per display frame.
class Game
{
public:
    // defined in game.cpp: the unique_ptr members need the full Renderer/
    // Mesh/Texture types wherever Game is constructed or destroyed
    Game();
    ~Game();

    int run();

private:
    bool init();
    void update(float dt);
    void render();
    void enterOrExitVehicle();

    // world geometry + every other actor, from self's point of view -
    // built once per actor per frame so Player/Vehicle never need to know
    // about World or each other
    std::function<bool(const AABB&)> collisionPredicateFor(const Actor* self) const;
    Vehicle* drivenVehicle() const; // non-null only while m_controlled is a vehicle

    GLFWwindow* m_window = nullptr;
    Input m_input;
    DebugUI m_debugUI;
    Camera m_camera;
    World m_world;

    std::vector<std::unique_ptr<Actor>> m_actors; // owns every actor
    Player* m_player = nullptr;                   // non-owning, points into m_actors
    std::vector<Vehicle*> m_vehicles;             // non-owning, point into m_actors
    Actor* m_controlled = nullptr;                // whichever actor currently receives input

    bool m_showColliders = false;
    bool m_showImGuiDemo = false;
    bool m_debugUIReady = false; // guards DebugUI::shutdown() against a partial init() failure

    bool m_shadowsEnabled = true;
    bool m_showShadowMapPreview = false;
    glm::vec3 m_sunDirection = glm::normalize(glm::vec3(0.4f, 1.0f, 0.3f)); // normalized, points toward the light

    // GL resources live behind pointers: they can only be created in init(),
    // once the OpenGL context exists
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::unique_ptr<Mesh> m_groundMesh;
    std::unique_ptr<Mesh> m_rampMesh;
    std::unique_ptr<Texture> m_groundTexture;
    std::unique_ptr<ShadowMap> m_shadowMap;
};

#endif

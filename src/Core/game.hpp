#ifndef GAME_H
#define GAME_H

#include <memory>
#include <vector>

#include "Core/input.hpp"
#include "Core/debug_ui.hpp"
#include "Rendering/camera.hpp"
#include "Game/world.hpp"
#include "Game/player.hpp"
#include "Game/vehicle.hpp"

struct GLFWwindow;
class Renderer;
class Mesh;
class Texture;

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

    GLFWwindow* m_window = nullptr;
    Input m_input;
    DebugUI m_debugUI;
    Camera m_camera;
    World m_world;
    Player m_player;
    std::vector<Vehicle> m_vehicles;
    int m_drivingIndex = -1; // -1 = on foot; otherwise index into m_vehicles being driven
    bool m_showColliders = false;
    bool m_showImGuiDemo = false;
    bool m_debugUIReady = false; // guards DebugUI::shutdown() against a partial init() failure

    // GL resources live behind pointers: they can only be created in init(),
    // once the OpenGL context exists
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::unique_ptr<Mesh> m_groundMesh;
    std::unique_ptr<Texture> m_groundTexture;
};

#endif

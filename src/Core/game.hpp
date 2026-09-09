#ifndef GAME_H
#define GAME_H

#include <functional>
#include <memory>
#include <vector>

#include "Core/input.hpp"
#include "Core/performance_history.hpp"
#include "Core/debug_ui.hpp"
#include "Rendering/camera.hpp"
#include "Rendering/sky.hpp"
#include "Game/world.hpp"
#include "Game/actor.hpp"
#include "Game/player.hpp"
#include "Game/vehicle.hpp"

struct GLFWwindow;
class Renderer;
class Mesh;
class Texture;
class ShadowMap;
class IslandRenderer;
class SceneRenderer;

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

    int run(bool smokeTest = false, bool benchmark = false);

private:
    bool init();
    void update(float dt);
    void render();
    void streamActors();
    void renderHUD();
    void startCourierRun();
    void stopCourierRun();
    void enterOrExitVehicle();

    // world geometry + every other actor, from self's point of view -
    // built once per actor per frame so Player/Vehicle never need to know
    // about World or each other
    std::function<bool(const CollisionBox&)> collisionPredicateFor(const Actor* self) const;
    Vehicle* drivenVehicle() const; // non-null only while m_controlled is a vehicle

    // Drops a new car on the ground ahead of whatever the player is currently
    // controlling, facing the way the camera looks. Debug-only, so it does not
    // check whether the spot is clear - it lands where you are looking.
    Vehicle& spawnVehicleAhead(VehicleType type);

    const char* m_capturePath = nullptr;
    bool m_smokeTest = false;
    GLFWwindow* m_window = nullptr;
    Input m_input;
    PerformanceHistory m_performance;
    DebugUI m_debugUI;
    Camera m_camera;
    World m_world;

    std::vector<std::unique_ptr<Actor>> m_actors; // owns every actor
    std::vector<Actor*> m_streamedVehicles, m_streamedPedestrians;
    std::vector<std::shared_ptr<const ModelAsset>> m_actorAssets; // shared catalog, never duplicated per placement
    Player* m_player = nullptr;                   // non-owning, points into m_actors
    std::vector<Vehicle*> m_vehicles;             // non-owning, point into m_actors
    Actor* m_controlled = nullptr;                // whichever actor currently receives input

    std::vector<glm::vec3> m_deliveryStops;
    float m_worldTime = 0;
    float m_waveStrength = 1.0f;
    float m_waterRecoveryNotice = 0;
    bool m_deliveryActive = false;
    bool m_showHUD = true;
    int m_debugVehicleIndex = 0;
    size_t m_deliveryIndex = 0;
    int m_deliveries = 0;
    int m_cash = 0;
    float m_deliveryHold = 0.0f;
    float m_noticeTime = 0.0f;

    bool m_showColliders = false;
    bool m_showImGuiDemo = false;
    bool m_debugUIReady = false; // guards DebugUI::shutdown() against a partial init() failure

    bool m_shadowsEnabled = true;
    bool m_showShadowMapPreview = false;
    // The sun is derived from the clock rather than fixed: m_timeOfDay drives
    // its direction, the light and haze colours, and the sky itself.
    float m_timeOfDay = 9.5f;      // hours, [0,24)
    float m_minutesPerSecond = 1.0f; // a full day in 24 real minutes
    bool m_dayRunning = true;
    Lighting m_lighting = Lighting::atTime(9.5f);
    Sky::Clouds m_clouds;

    // GL resources live behind pointers: they can only be created in init(),
    // once the OpenGL context exists
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::unique_ptr<IslandRenderer> m_islandRenderer;
    std::unique_ptr<SceneRenderer> m_sceneRenderer;
    std::unique_ptr<Mesh> m_rampMesh;
    std::unique_ptr<Texture> m_mapTexture;
    std::unique_ptr<ShadowMap> m_shadowMap;
    std::unique_ptr<Sky> m_sky;
};

#endif

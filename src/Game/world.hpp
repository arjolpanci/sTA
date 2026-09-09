#ifndef WORLD_H
#define WORLD_H

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "terrain.hpp"
#include <glm/glm.hpp>

#include "collision_box.hpp"

// A saved box: buildings, sidewalks, bridge decks and scenery.
struct StaticBox
{
    glm::vec3 center;
    glm::vec3 size;
    glm::vec3 color;
    bool facade = false;
    float yaw = 0;
    bool modelProxy = false;
};

// Walkable wedge. Its high end and sides block entry below the surface.
struct Ramp
{
    glm::vec3 footprintCenter;  // world XZ center (y unused)
    glm::vec2 footprintSize;    // x = width (across the slope), y = length (along the slope)
    bool alongX = false;        // false: rises toward +Z: true: rises toward +X
    float lowHeight = 0.0f;
    float highHeight = 3.0f;
    glm::vec3 color{ 0.55f, 0.52f, 0.50f };

    // nullopt if (x, z) falls outside the footprint
    std::optional<float> heightAt(float x, float z) const;
};

struct TreeSpawn { glm::vec3 feet; float height, yaw; int model; };

struct Landmark { std::string kind, name; glm::vec3 position; };

struct Road { float width; std::vector<glm::vec3> route; };
// Painted road marking: a rectangle on the ground plan, laid over whatever
// shape the terrain turned out to be rather than standing on it as a slab.
struct Marking { glm::vec2 center, size; glm::vec3 color; };
// Kerbside scenery. -1 as the model means the manhole cover, which is placed on
// the road surface rather than beside it.
struct PropSpawn { glm::vec3 feet; float yaw, height; int model; };
struct VehicleSpawn { int type; float yaw, speed; std::vector<glm::vec3> route; };
struct PedestrianSpawn { glm::vec3 color; float speed; std::vector<glm::vec3> route; };

// Loads the baked scene and indexes static geometry for collision/support queries.
class World
{
public:
    World();
    const std::vector<Landmark>& landmarks() const { return m_landmarks; }
    const std::vector<TreeSpawn>& trees() const { return m_trees; }
    const std::vector<Road>& roads() const { return m_roads; }
    const std::vector<PropSpawn>& props() const { return m_props; }
    const std::vector<Marking>& markings() const { return m_markings; }
    const Terrain& terrain() const { return m_terrain; }
    const std::vector<VehicleSpawn>& vehicleSpawns() const { return m_vehicleSpawns; }
    const std::vector<PedestrianSpawn>& pedestrianSpawns() const { return m_pedestrianSpawns; }
    static std::vector<glm::vec3> trafficLoop(float x, float z);

    const std::vector<StaticBox>& boxes() const { return m_boxes; }
    const std::vector<StaticBox>& decorations() const { return m_decorations; }
    const std::vector<Ramp>& ramps() const { return m_ramps; }
    glm::vec2 groundSize() const { return glm::vec2(m_terrain.extent()); }

    // Upright oriented actors against static solids and ramp walls.
    bool collides(const CollisionBox& box) const;

    // Terrain plus reachable rooftops, bridge decks and ramps. maxHeight
    // prevents actors below bridges from snapping onto their decks.
    float groundHeightAt(float x, float z, float maxHeight = 10000.0f) const;
    glm::vec3 surfaceNormal(const glm::vec3& feet) const;
    float supportHeight(const CollisionBox& actor, float maxHeight) const;

private:
    // Derived from the baked roads rather than baked itself: the scene file
    // stays untouched, and moving a road moves its street furniture with it.
    void placeProps();
    std::vector<size_t> candidates(float x, float z, float radius = 0) const;
    static int64_t cellKey(int x, int z);
    Terrain m_terrain;
    std::vector<Landmark> m_landmarks;
    std::vector<Road> m_roads;
    std::vector<TreeSpawn> m_trees;
    std::vector<PropSpawn> m_props;
    std::vector<Marking> m_markings;
    std::unordered_map<int64_t, std::vector<size_t>> m_cells;
    std::vector<VehicleSpawn> m_vehicleSpawns;
    std::vector<PedestrianSpawn> m_pedestrianSpawns;
    std::vector<StaticBox> m_boxes;
    std::vector<StaticBox> m_decorations;
    std::vector<CollisionBox> m_colliders;
    std::vector<Ramp> m_ramps;
};

#endif

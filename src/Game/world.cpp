#include "world.hpp"
#include "model_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "vertical_motion.hpp"

std::optional<float> Ramp::heightAt(float x, float z) const
{
    float dx = x - footprintCenter.x;
    float dz = z - footprintCenter.z;
    float along = alongX ? dx : dz;    // position along the slope direction
    float across = alongX ? dz : dx;   // position across the slope (width)

    float halfWidth = footprintSize.x * 0.5f;
    float halfLength = footprintSize.y * 0.5f;
    if (std::abs(across) > halfWidth || std::abs(along) > halfLength)
        return std::nullopt;

    float t = (along + halfLength) / (2.0f * halfLength); // 0 at low end, 1 at high end
    return lowHeight + t * (highHeight - lowHeight);
}

World::World()
{
    std::ifstream file("resources/maps/island.scene");
    std::string magic; int version;
    if (!(file >> magic >> version) || magic != "STA_SCENE" || (version != 1 && version != 2))
        throw std::runtime_error("Missing or unsupported baked island scene");
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::istringstream row(line);
        char type; row >> type;
        auto vec = [&row]() { glm::vec3 v; row >> v.x >> v.y >> v.z; return v; };
        auto route = [&row, &vec]() {
            int count = 0; row >> count;
            if (count < 1 || count > 256) throw std::runtime_error("Invalid baked actor route");
            std::vector<glm::vec3> result;
            for (int i=0; i<count; ++i) result.push_back(vec());
            return result;
        };
        if (type == 'B' || type == 'D')
        {
            StaticBox box; box.center=vec(); box.size=vec(); box.color=vec();
            row >> box.facade >> box.yaw;
            if (box.size.x <= 0 || box.size.y <= 0 || box.size.z <= 0) throw std::runtime_error("Invalid baked box size");
            if (type == 'D') m_decorations.push_back(box);
            else {
                m_boxes.push_back(box);
                m_colliders.push_back(CollisionBox::fromCenterHalf(box.center, box.size*.5f, box.yaw));
            }
        }
        else if (type == 'T')
        {
            TreeSpawn tree; tree.feet=vec(); row >> tree.height >> tree.yaw >> tree.model;
            if(tree.height<=0 || tree.model<0 || tree.model>=int(TreeModels.size())) throw std::runtime_error("Invalid baked tree");
            m_trees.push_back(tree);
            float trunkHeight=tree.height*3.1f/7.0f;
            StaticBox trunk{tree.feet+glm::vec3(0,trunkHeight*.5f,0),{.65f,trunkHeight,.65f},{.32f,.23f,.16f},false,0,true};
            m_boxes.push_back(trunk);
            m_colliders.push_back(CollisionBox::fromCenterHalf(trunk.center,trunk.size*.5f));
        }
        else if (type == 'R')
        {
            Ramp r;
            row >> r.footprintCenter.x >> r.footprintCenter.z >> r.footprintSize.x >> r.footprintSize.y
                >> r.alongX >> r.lowHeight >> r.highHeight;
            r.color=vec();
            if (r.footprintSize.x<=0 || r.footprintSize.y<=0 || r.highHeight<=r.lowHeight)
                throw std::runtime_error("Invalid baked ramp");
            m_ramps.push_back(r);
        }
        else if (type == 'L')
        {
            Road road; row >> road.width; road.route=route(); m_roads.push_back(road);
        }
        else if (type == 'V')
        {
            VehicleSpawn spawn; row >> spawn.type >> spawn.yaw >> spawn.speed;
            spawn.route=route();
            if (spawn.type<0 || spawn.type>=int(VehicleModels.size()) || spawn.speed<0) throw std::runtime_error("Invalid baked vehicle");
            m_vehicleSpawns.push_back(spawn);
        }
        else if (type == 'P')
        {
            PedestrianSpawn spawn; spawn.color=vec(); row >> spawn.speed;
            spawn.route=route(); m_pedestrianSpawns.push_back(spawn);
        }
        else throw std::runtime_error("Unknown baked scene record");
        if (!row) throw std::runtime_error("Malformed baked scene record");
        std::string extra;
        if (row >> extra) throw std::runtime_error("Unexpected data in baked scene record");
    }
    // Spatial lookup is an acceleration structure for the loaded geometry.
    for (size_t i=0; i<m_boxes.size(); ++i) {
        const auto& box=m_boxes[i];
        float r=(box.size.x+box.size.z)*.5f;
        for (int z=int(std::floor((box.center.z-r)/64)); z<=int(std::floor((box.center.z+r)/64)); ++z)
            for (int x=int(std::floor((box.center.x-r)/64)); x<=int(std::floor((box.center.x+r)/64)); ++x)
                m_cells[cellKey(x,z)].push_back(i);
    }
    placeProps();
}

namespace {
// Stable per-position noise, so the same road always dresses the same way and
// nothing about the scenery has to be baked into the scene file.
uint32_t propNoise(float x, float z, uint32_t salt)
{
    uint32_t seed = uint32_t(int32_t(std::lround(x*4))) * 73856093u ^ uint32_t(int32_t(std::lround(z*4))) * 19349663u ^ salt*83492791u;
    seed ^= seed >> 16; seed *= 0x45d9f3bu; seed ^= seed >> 16;
    return seed;
}
}

// Distance from a point to the nearest point on a polyline.
static float polylineDistance(const std::vector<glm::vec3>& route, float x, float z)
{
    if (route.empty()) return 1e9f;
    // A parked car is a route of one point, and still needs its space.
    float best = glm::length(glm::vec2(route.front().x - x, route.front().z - z));
    for (size_t i=0; i+1 < route.size(); ++i)
    {
        glm::vec2 a(route[i].x, route[i].z), b(route[i+1].x, route[i+1].z);
        glm::vec2 along = b - a;
        float lengthSquared = glm::dot(along, along);
        float t = lengthSquared > 0 ? glm::clamp(glm::dot(glm::vec2(x,z) - a, along) / lengthSquared, 0.0f, 1.0f) : 0.0f;
        best = std::min(best, glm::length(glm::vec2(x,z) - (a + along*t)));
    }
    return best;
}

// Distance from a point to the nearest road surface edge, negative inside it.
static float roadClearance(const std::vector<Road>& roads, float x, float z)
{
    float clearance = 1e9f;
    for (const Road& road : roads)
        clearance = std::min(clearance, polylineDistance(road.route, x, z) - road.width*0.5f);
    return clearance;
}

void World::placeProps()
{
    // Props are solid, so they join the same box/collider/cell arrays the baked
    // geometry uses - and are tested against what is already there, which is
    // why this runs after the spatial index is built rather than during load.
    auto addSolid = [this](const CollisionBox& box, const glm::vec3& center, const glm::vec3& size, float yaw) {
        size_t index = m_boxes.size();
        m_boxes.push_back({center, size, {0.4f, 0.4f, 0.4f}, false, yaw, true});
        m_colliders.push_back(box);
        float r = (size.x + size.z) * 0.5f;
        for (int z=int(std::floor((center.z-r)/64)); z<=int(std::floor((center.z+r)/64)); ++z)
            for (int x=int(std::floor((center.x-r)/64)); x<=int(std::floor((center.x+r)/64)); ++x)
                m_cells[cellKey(x,z)].push_back(index);
    };

    // One kerbside placement attempt, shared by the lamp cadence and the random
    // clutter: level ground, clear of every carriageway, room to stand.
    // Walk outward from the carriageway until the ground steps up: that step is
    // the kerb, and street furniture belongs on the pavement behind it, not on
    // the strip of dirt between the two.
    auto kerbOffset = [&](const glm::vec3& middle, const glm::vec3& outward, float roadY, float radius) {
        for (float step = 0.6f; step <= 7.0f; step += 0.3f)
        {
            const glm::vec3 probe = middle + outward*step;
            if (groundHeightAt(probe.x, probe.z) > roadY + 0.08f)
                return step + radius + 0.4f; // clear of the kerb edge, whole footprint on the pavement
        }
        return -1.0f; // no kerb within reach: a junction mouth, or a rural verge
    };

    auto place = [&](const glm::vec3& middle, const glm::vec3& outward, float roadY, int model, float yaw) {
        const PropModel& prop = PropModels[size_t(model)];
        float offset = kerbOffset(middle, outward, roadY, prop.radius);
        if (offset < 0)
        {
            // Across the mouth of a junction the search only ever finds more
            // carriageway, and a lamp in the middle of a crossing is worse than
            // no lamp. On an open verge there is nothing to step onto and the
            // prop simply stands back from the asphalt.
            if (roadClearance(m_roads, middle.x + outward.x*4.0f, middle.z + outward.z*4.0f) < prop.radius + 5.0f) return;
            offset = prop.radius + 1.2f;
        }
        const glm::vec3 feet = middle + outward*offset;
        const float ground = groundHeightAt(feet.x, feet.z);
        // Skip anything the road does not run level with: cliffs, water, and
        // the ground under a bridge.
        if (std::abs(ground - roadY) > 1.0f) return;
        // Clear of *every* road, not just this one: at a junction a kerb
        // belonging to one street lies in another's carriageway.
        if (roadClearance(m_roads, feet.x, feet.z) < prop.radius + 0.9f) return;
        // Baked actors are placed before this runs and are not part of the
        // collision index, so their routes have to be kept clear by hand. It is
        // the whole route, not just the spawn point: patrols run along legs the
        // road network does not cover, and a prop there stops the traffic dead.
        for (const auto& spawn : m_vehicleSpawns)
            // Wide of the route, not just clear of it: the traffic AI steers
            // proportionally and cuts corners by a couple of metres.
            if (polylineDistance(spawn.route, feet.x, feet.z) < prop.radius + 6.0f) return;
        for (const auto& spawn : m_pedestrianSpawns)
            if (polylineDistance(spawn.route, feet.x, feet.z) < prop.radius + 1.2f) return;

        const glm::vec3 center(feet.x, ground + prop.height*0.5f, feet.z);
        const glm::vec3 size(prop.radius*2, prop.height, prop.radius*2);
        if (prop.solid)
        {
            auto box = CollisionBox::fromCenterHalf(center, size*0.5f, yaw);
            if (collides(box)) return;
            addSolid(box, center, size, yaw);
        }
        m_props.push_back({{feet.x, ground, feet.z}, yaw, prop.height, model});
    };

    for (const Road& road : m_roads)
    {
        for (size_t i=0; i+1 < road.route.size(); ++i)
        {
            const glm::vec3 from = road.route[i], to = road.route[i+1];
            const glm::vec3 along = to - from;
            const float length = glm::length(glm::vec2(along.x, along.z));
            if (length < 14.0f) continue;
            const glm::vec3 direction = along / length;
            const glm::vec3 side(-direction.z, 0, direction.x);
            const float heading = glm::degrees(std::atan2(direction.x, direction.z));

            // Lamps first, on a regular cadence and alternating sides.
            for (float travelled = 8.0f; travelled < length - 8.0f; travelled += LampSpacing)
            {
                const float hand = int(travelled/LampSpacing) % 2 ? 1.0f : -1.0f;
                const glm::vec3 point = from + direction*travelled;
                place(point + side*hand*(road.width*0.5f), side*hand, point.y, 0, heading + 90.0f*hand);
            }

            for (float travelled = 6.0f; travelled < length - 6.0f; travelled += 11.0f)
            {
                const glm::vec3 point = from + direction*travelled;
                if (propNoise(point.x, point.z, 7) % 5 == 0)
                {
                    // A manhole every so often, on the road surface itself.
                    float ground = groundHeightAt(point.x, point.z);
                    if (std::abs(ground - point.y) < 1.0f)
                        m_props.push_back({{point.x, ground, point.z}, heading, ManholeHeight, -1});
                }

                for (float hand : {-1.0f, 1.0f})
                {
                    const glm::vec3 middle = point + side*hand*(road.width*0.5f);
                    uint32_t seed = propNoise(middle.x, middle.z, 1);
                    int roll = int(seed % 100u), model = -1;
                    for (size_t p=0; p<PropModels.size() && model<0; ++p)
                        if (PropModels[p].weight > 0 && (roll -= PropModels[p].weight) < 0) model = int(p);
                    if (model < 0) continue; // most kerbside slots stay empty

                    // Facing the road, give or take, so a row never looks stamped.
                    float yaw = (PropModels[size_t(model)].alongRoad ? heading : heading + 90.0f*hand)
                              + float(int(seed>>8) % 13) - 6.0f;
                    place(middle, side*hand, point.y, model, yaw);
                }
            }
        }
    }
}

int64_t World::cellKey(int x, int z)
{
    return static_cast<int64_t>((uint64_t(uint32_t(x)) << 32) | uint32_t(z));
}

std::vector<size_t> World::candidates(float x, float z, float radius) const
{
    std::vector<size_t> result;
    for (int iz=int(std::floor((z-radius)/64)); iz<=int(std::floor((z+radius)/64)); ++iz)
        for (int ix=int(std::floor((x-radius)/64)); ix<=int(std::floor((x+radius)/64)); ++ix)
            if (auto it=m_cells.find(cellKey(ix,iz)); it!=m_cells.end())
                result.insert(result.end(),it->second.begin(),it->second.end());
    return result;
}

bool World::collides(const CollisionBox& box) const
{
    if (box.center.y - box.half.y < m_terrain.heightAt(box.center.x, box.center.z) - 0.05f) return true;
    for (size_t index : candidates(box.center.x, box.center.z, box.half.x + box.half.z))
        if (box.intersects(m_colliders[index]))
            return true;
    for (const Ramp& ramp : m_ramps)
    {
        glm::vec3 size(ramp.footprintSize.x, ramp.highHeight - ramp.lowHeight, ramp.footprintSize.y);
        CollisionBox volume = CollisionBox::fromCenterHalf(
            {ramp.footprintCenter.x, (ramp.lowHeight + ramp.highHeight) * 0.5f, ramp.footprintCenter.z},
            size * 0.5f, ramp.alongX ? 90.0f : 0.0f);
        if (!box.intersects(volume)) continue;
        float halfX = ramp.alongX ? ramp.footprintSize.y * 0.5f : ramp.footprintSize.x * 0.5f;
        float halfZ = ramp.alongX ? ramp.footprintSize.x * 0.5f : ramp.footprintSize.y * 0.5f;
        float x = std::clamp(box.center.x, ramp.footprintCenter.x - halfX, ramp.footprintCenter.x + halfX);
        float z = std::clamp(box.center.z, ramp.footprintCenter.z - halfZ, ramp.footprintCenter.z + halfZ);
        auto surface = ramp.heightAt(x, z);
        if (surface && *surface > box.center.y - box.half.y + MAX_STEP_UP)
            return true; // block the tall end and sides; allow gradual uphill steps
    }
    return false;
}

float World::groundHeightAt(float x, float z, float maxHeight) const
{
    float height = m_terrain.heightAt(x, z);

    for (size_t index : candidates(x, z))
    {
        const StaticBox& box = m_boxes[index];
        float halfX = box.size.x * 0.5f;
        float halfZ = box.size.z * 0.5f;
        glm::vec2 delta(x-box.center.x,z-box.center.z);
        auto axes=m_colliders[index].axes();
        bool withinFootprint = std::abs(glm::dot(delta,axes[0]))<=halfX && std::abs(glm::dot(delta,axes[1]))<=halfZ;
        if (withinFootprint && box.center.y + box.size.y * 0.5f <= maxHeight)
            height = std::max(height, box.center.y + box.size.y * 0.5f);
    }

    for (const Ramp& ramp : m_ramps)
    {
        if (std::optional<float> h = ramp.heightAt(x, z))
            if (*h <= maxHeight) height = std::max(height, *h);
    }

    return height;
}

float World::supportHeight(const CollisionBox& actor, float maxHeight) const
{
    float height = groundHeightAt(actor.center.x, actor.center.z, maxHeight);
    for (size_t index : candidates(actor.center.x, actor.center.z, actor.half.x + actor.half.z))
    {
        const StaticBox& box = m_boxes[index];
        float top = box.center.y + box.size.y * 0.5f;
        if (top > maxHeight || top <= height) continue;
        CollisionBox footprint = actor;
        footprint.center.y = box.center.y;
        footprint.half.y = box.size.y;
        if (footprint.intersects(CollisionBox::fromCenterHalf(box.center, box.size * 0.5f, box.yaw)))
            height = top;
    }
    return height;
}

glm::vec3 World::surfaceNormal(const glm::vec3& feet) const
{
    for (const Ramp& ramp : m_ramps)
        if (auto h = ramp.heightAt(feet.x, feet.z))
            if (std::abs(*h - feet.y) < 0.1f)
            {
                float slope = (ramp.highHeight - ramp.lowHeight) / ramp.footprintSize.y;
                return glm::normalize(ramp.alongX ? glm::vec3(-slope,1,0) : glm::vec3(0,1,-slope));
            }
    if (std::abs(feet.y - m_terrain.heightAt(feet.x, feet.z)) < 0.2f)
        return m_terrain.normalAt(feet.x, feet.z);
    return {0,1,0};
}

std::vector<glm::vec3> World::trafficLoop(float x, float z)
{
    return {{x+12,8,z+3.5f}, {x+56.5f,8,z+3.5f}, {x+56.5f,8,z+56.5f},
            {x+3.5f,8,z+56.5f}, {x+3.5f,8,z+3.5f}};
}

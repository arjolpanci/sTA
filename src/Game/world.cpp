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

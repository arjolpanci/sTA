#ifndef WAYPOINT_PATH_H
#define WAYPOINT_PATH_H

#include <utility>
#include <vector>
#include <glm/glm.hpp>

// A looping sequence of ground points to walk or drive toward one at a time.
// Shared by Pedestrian and Vehicle's traffic AI - both just need "where do I
// go next", not two separate implementations of the same idea.
class WaypointPath
{
public:
    explicit WaypointPath(std::vector<glm::vec3> points) : m_points(std::move(points)) {}

    void reset() { m_index = 0; }

    const glm::vec3& current() const { return m_points[m_index]; }

    // call after moving toward current(): advances to the next point (or
    // back to the first, past the last - so two points make a back-and-forth
    // patrol and three or more make a loop) once within reachRadius of it
    void advanceIfReached(const glm::vec3& position, float reachRadius)
    {
        glm::vec3 toTarget = current() - position;
        toTarget.y = 0.0f;
        if (glm::length(toTarget) <= reachRadius)
            m_index = (m_index + 1) % m_points.size();
    }

private:
    std::vector<glm::vec3> m_points;
    size_t m_index = 0;
};

#endif

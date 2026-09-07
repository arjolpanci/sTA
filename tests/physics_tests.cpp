#include <cmath>
#include <cstdlib>
#include <iostream>
#include "Game/world.hpp"
#include "Game/vertical_motion.hpp"
#include "Rendering/mesh.hpp"

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool close(float a, float b) { return std::abs(a - b) < 0.002f; }
struct Context {
    std::function<bool(const CollisionBox&)> collides;
    std::function<float(float, float)> groundHeightAt;
};
}
int main() {
    auto car = CollisionBox::fromCenterHalf({0, 1, 0}, {1, 1, 2.2f}, 45);
    auto emptyCorner = CollisionBox::fromCenterHalf({2, 1, -2}, {0.1f, 0.5f, 0.1f});
    check(!car.intersects(emptyCorner), "rotated car does not occupy enclosing AABB corners");
    check(car.intersects(CollisionBox::fromCenterHalf({1, 1, 1}, {0.2f, 0.5f, 0.2f})), "rotated nose collides");
    check(!car.intersects(CollisionBox::fromCenterHalf({0, 3, 0}, {1, 1, 1})), "touching vertical faces do not collide");
    for (int yaw = 0; yaw < 360; yaw += 5) {
        car.yaw = float(yaw);
        auto other = CollisionBox::fromCenterHalf({2, 1, 1}, {0.7f, 1, 1.6f}, float(yaw + 31));
        check(car.intersects(other) == other.intersects(car), "SAT is symmetric at every heading");
    }
    auto wedge = Mesh::rampVertices();
    for (size_t i = 0; i < wedge.size(); i += 24) {
        glm::vec3 a(wedge[i], wedge[i+1], wedge[i+2]);
        glm::vec3 b(wedge[i+8], wedge[i+9], wedge[i+10]);
        glm::vec3 c(wedge[i+16], wedge[i+17], wedge[i+18]);
        glm::vec3 normal(wedge[i+3], wedge[i+4], wedge[i+5]);
        check(glm::dot(glm::cross(b-a, c-a), normal) > 0, "every ramp triangle winds outward");
    }
    World world;
    check(world.groundSize().x == 360, "expanded city bounds");
    check(!world.collides(CollisionBox::fromCenterHalf({0, 0.9f, 0}, {0.3f, 0.9f, 0.3f})), "player spawn clear");
    check(close(world.groundHeightAt(21, -30, 0.45f), 0.16f), "inaccessible roofs are excluded");
    for (const Ramp& ramp : world.ramps()) {
        glm::vec3 along = ramp.alongX ? glm::vec3(1,0,0) : glm::vec3(0,0,1);
        auto low = ramp.footprintCenter - along * ramp.footprintSize.y * 0.5f;
        auto high = ramp.footprintCenter + along * ramp.footprintSize.y * 0.5f;
        check(close(*ramp.heightAt(low.x, low.z), ramp.lowHeight), "ramp low endpoint matches render");
        check(close(*ramp.heightAt(high.x, high.z), ramp.highHeight), "ramp high endpoint matches render");
        glm::vec3 pos = low - along;
        pos.y = ramp.lowHeight;
        VerticalMotion motion;
        auto bounds = [&]() { return CollisionBox::fromCenterHalf(pos + glm::vec3(0,0.9f,0), {0.3f,0.9f,0.3f}); };
        Context ctx{[&](const CollisionBox& box) {return world.collides(box);}, [&](float x, float z) {
            auto box = bounds(); box.center.x = x; box.center.z = z;
            return world.supportHeight(box, pos.y + MAX_STEP_UP);
        }};
        for (int i = 0; i < 148; ++i) {
            moveHorizontal(pos, along * 0.1f, motion.grounded, bounds, ctx);
            resolveVerticalMotion(motion, pos.y, 1.0f/60, ctx.groundHeightAt(pos.x,pos.z), [&]() {return ctx.collides(bounds());});
        }
        check(pos.y > ramp.highHeight - 0.1f, "walk up entire ramp without sticking");
        for (int i = 0; i < 145; ++i) {
            moveHorizontal(pos, -along * 0.1f, motion.grounded, bounds, ctx);
            resolveVerticalMotion(motion, pos.y, 1.0f/60, ctx.groundHeightAt(pos.x,pos.z), [&]() {return ctx.collides(bounds());});
        }
        check(close(pos.y, ramp.lowHeight), "walk down ramp smoothly");
        pos = high + along * 0.1f; pos.y = ramp.lowHeight;
        check(world.collides(bounds()), "tall ramp end is solid, not a teleport");
        pos = high + along * 0.5f; pos.y = ramp.highHeight;
        motion.land();
        resolveVerticalMotion(motion, pos.y, 1.0f/60, ramp.lowHeight, [](){return false;});
        check(!motion.grounded && pos.y > ramp.highHeight - 0.1f, "leaving ramp edge starts a fall");
    }
    VerticalMotion jump;
    float y = 0;
    jump.jump(9);
    for (int i = 0; i < 120; ++i) resolveVerticalMotion(jump, y, 1.0f/60, 0, [](){return false;});
    check(jump.grounded && close(y, 0), "jump returns to floor");
    y = 0; jump.land();
    resolveVerticalMotion(jump, y, 1.0f/60, 12, [](){return false;});
    check(y < 0.1f, "ground following cannot snap onto a roof");
    glm::vec3 pos(0,0,0);
    auto bounds = [&]() {return CollisionBox::fromCenterHalf(pos + glm::vec3(0,0.9f,0), {0.3f,0.9f,0.3f});};
    auto wall = CollisionBox::fromCenterHalf({3,1,0}, {0.1f,1,5});
    Context ctx{[&](const CollisionBox& b){return wall.intersects(b);}, [](float,float){return 0.0f;}};
    moveHorizontal(pos, {12,0,2}, true, bounds, ctx);
    check(pos.x < 2.7f && pos.z > 1.9f, "fast motion cannot tunnel through thin walls and slides along them");
    pos = {7.5f,0,20};
    Context curb{[&](const CollisionBox& b){return world.collides(b);}, [&](float x,float z){
        auto b = bounds(); b.center.x=x; b.center.z=z; return world.supportHeight(b,pos.y+MAX_STEP_UP);
    }};
    moveHorizontal(pos, {1,0,0}, true, bounds, curb);
    check(pos.x > 8.4f && close(pos.y,0.16f), "walk onto sidewalk with leading edge of footprint");
    std::cout << checks << " physics checks passed\n";
}

#ifndef DEBUG_UI_H
#define DEBUG_UI_H

#include <functional>
#include <string>
#include <vector>

struct GLFWwindow;

// Thin wrapper around Dear ImGui. Owns its GL/GLFW lifecycle and lets other
// systems register their own panels via addPanel(), so new debug tools don't
// require touching this class or Game - just call addPanel() wherever the
// system is set up.
//
// Toggled with F1. While open, the cursor is freed and gameplay (camera look,
// player movement) is suppressed so dragging a slider doesn't also spin the
// camera or walk the player.
class DebugUI
{
public:
    void init(GLFWwindow* window);
    void shutdown();

    void toggle();
    bool visible() const { return m_visible; }

    // registers a panel; drawFn is called every visible frame, between
    // ImGui::Begin(title) and ImGui::End()
    void addPanel(const std::string& title, std::function<void()> drawFn);

    void beginFrame(); // builds this frame's UI (no-op while hidden)
    void render();      // draws it on top of the 3D scene (no-op while hidden)

private:
    struct Panel
    {
        std::string title;
        std::function<void()> draw;
    };

    GLFWwindow* m_window = nullptr;
    bool m_visible = false;
    std::vector<Panel> m_panels;
};

#endif

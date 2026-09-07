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
    void setVisible(bool visible);
    void selectPanel(const std::string& title);
    bool visible() const { return m_visible; }

    // registers a panel; drawFn is called every visible frame, between
    // the content child of the selected sidebar page
    void addPanel(const std::string& title, std::function<void()> drawFn);

    void beginFrame(); // starts the HUD frame and optional debug window
    void render();      // draws the UI on top of the 3D scene

private:
    struct Panel
    {
        std::string title;
        std::function<void()> draw;
    };

    GLFWwindow* m_window = nullptr;
    bool m_visible = false;
    size_t m_selectedPanel = 0;
    std::vector<Panel> m_panels;
};

#endif

#ifndef INPUT_H
#define INPUT_H

#include <unordered_map>

struct GLFWwindow;

// Wraps GLFW input: key polling plus per-frame mouse/scroll deltas
// accumulated from callbacks. Call endFrame() once per frame after the
// deltas have been consumed.
class Input
{
public:
    void attach(GLFWwindow* window);

    bool keyDown(int key) const;
    bool keyPressed(int key);      // true only on the frame the key goes down; good for toggles
    float mouseDX() const { return m_mouseDX; }
    float mouseDY() const { return m_mouseDY; }   // > 0 = mouse moved up
    float scrollDY() const { return m_scrollDY; }

    void endFrame();

private:
    static void cursorCallback(GLFWwindow* window, double x, double y);
    static void scrollCallback(GLFWwindow* window, double dx, double dy);

    GLFWwindow* m_window = nullptr;
    double m_lastX = 0.0, m_lastY = 0.0;
    bool m_firstMouse = true;
    float m_mouseDX = 0.0f, m_mouseDY = 0.0f, m_scrollDY = 0.0f;
    std::unordered_map<int, bool> m_prevKeyDown; // per-key state for keyPressed()
};

#endif

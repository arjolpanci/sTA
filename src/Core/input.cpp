#include "input.hpp"

#include <GLFW/glfw3.h>

void Input::attach(GLFWwindow* window)
{
    m_window = window;
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

bool Input::keyDown(int key) const
{
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool Input::keyPressed(int key)
{
    bool down = keyDown(key);
    bool& wasDown = m_prevKeyDown[key];
    bool pressed = down && !wasDown;
    wasDown = down;
    return pressed;
}

void Input::endFrame()
{
    m_mouseDX = 0.0f;
    m_mouseDY = 0.0f;
    m_scrollDY = 0.0f;
}

void Input::cursorCallback(GLFWwindow* window, double x, double y)
{
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input)
        return;

    if (input->m_firstMouse)
    {
        input->m_lastX = x;
        input->m_lastY = y;
        input->m_firstMouse = false;
        return;
    }

    input->m_mouseDX += static_cast<float>(x - input->m_lastX);
    input->m_mouseDY += static_cast<float>(input->m_lastY - y); // reversed: up is positive
    input->m_lastX = x;
    input->m_lastY = y;
}

void Input::scrollCallback(GLFWwindow* window, double /*dx*/, double dy)
{
    Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input)
        return;
    input->m_scrollDY += static_cast<float>(dy);
}

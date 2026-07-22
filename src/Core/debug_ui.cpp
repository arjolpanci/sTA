#include "debug_ui.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

void DebugUI::init(GLFWwindow* window)
{
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // install_callbacks=true: ImGui chains onto whatever GLFW callbacks are
    // already set (Input::attach must run before this) rather than replacing
    // them, so our own mouse/scroll handling keeps working
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void DebugUI::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void DebugUI::toggle()
{
    m_visible = !m_visible;
    glfwSetInputMode(m_window, GLFW_CURSOR, m_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void DebugUI::addPanel(const std::string& title, std::function<void()> drawFn)
{
    m_panels.push_back({ title, std::move(drawFn) });
}

void DebugUI::beginFrame()
{
    if (!m_visible)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    for (Panel& panel : m_panels)
    {
        if (ImGui::Begin(panel.title.c_str()))
            panel.draw();
        ImGui::End();
    }
}

void DebugUI::render()
{
    if (!m_visible)
        return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

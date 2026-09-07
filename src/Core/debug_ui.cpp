#include "debug_ui.hpp"

#include <imgui.h>
#include <algorithm>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

void DebugUI::init(GLFWwindow* window)
{
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8;
    style.FrameRounding = 4;
    style.FramePadding = {8, 6};
    style.ItemSpacing = {10, 9};


    // install_callbacks=true: ImGui chains onto whatever GLFW callbacks are
    // already set (Input::attach must run before this) rather than replacing
    // them, so our own mouse/scroll handling keeps working
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
#ifdef STA_DEBUG_BUILD
    setVisible(true);
#else
    setVisible(false);
#endif
}

void DebugUI::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void DebugUI::toggle()
{
    setVisible(!m_visible);
}

void DebugUI::setVisible(bool visible)
{
    m_visible = visible;
    glfwSetInputMode(m_window, GLFW_CURSOR, m_visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

void DebugUI::addPanel(const std::string& title, std::function<void()> drawFn)
{
    m_panels.push_back({ title, std::move(drawFn) });
}

void DebugUI::selectPanel(const std::string& title)
{
    for (size_t i = 0; i < m_panels.size(); ++i)
        if (m_panels[i].title == title) { m_selectedPanel = i; setVisible(true); return; }
}

void DebugUI::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!m_visible) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({24, 24}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({std::min(820.0f, display.x - 48), std::min(560.0f, display.y - 48)}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({480, 320}, {std::max(480.0f, display.x), std::max(320.0f, display.y)});
    bool open = true;
    if (ImGui::Begin("sTA Debug Tools", &open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextColored({0.4f, 0.9f, 0.75f, 1}, "SIMULATION PAUSED");
        ImGui::SameLine();
        if (ImGui::Button("Resume game (F1)")) setVisible(false);
        ImGui::Separator();
        ImGui::BeginChild("Navigation", {145, 0}, true);
        for (size_t i = 0; i < m_panels.size(); ++i)
            if (ImGui::Selectable(m_panels[i].title.c_str(), m_selectedPanel == i))
                m_selectedPanel = i;
        ImGui::Spacing();
        ImGui::TextWrapped("Tab / arrows: navigate\nEnter: activate\nF1: close / reopen");
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Tools", {0, 0});
        if (m_selectedPanel < m_panels.size())
        {
            ImGui::TextUnformatted(m_panels[m_selectedPanel].title.c_str());
            ImGui::Separator();
            ImGui::PushItemWidth(-170);
            m_panels[m_selectedPanel].draw();
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();
    }
    ImGui::End();
    if (!open) setVisible(false);
}

void DebugUI::render()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

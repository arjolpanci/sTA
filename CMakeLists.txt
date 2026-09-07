cmake_minimum_required(VERSION 3.16)
project(sTA LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sTA
	src/main.cpp
	src/Core/game.cpp
	src/Core/input.cpp
	src/Core/debug_ui.cpp
	src/Rendering/shader.cpp
	src/Rendering/camera.cpp
	src/Rendering/mesh.cpp
	src/Rendering/texture.cpp
	src/Rendering/renderer.cpp
	src/Rendering/island_renderer.cpp
	src/Rendering/scene_renderer.cpp
	src/Rendering/shadow_map.cpp
	src/Game/world.cpp src/Game/terrain.cpp
	src/Game/player.cpp
	src/Game/vehicle.cpp
	src/Game/pedestrian.cpp
	lib/imgui/imgui.cpp
	lib/imgui/imgui_draw.cpp
	lib/imgui/imgui_tables.cpp
	lib/imgui/imgui_widgets.cpp
	lib/imgui/imgui_demo.cpp
	lib/imgui/backends/imgui_impl_glfw.cpp
	lib/imgui/backends/imgui_impl_opengl3.cpp
)

# Copy saved assets even when only the map/shaders changed. This never runs the generator.
set(STA_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources")
if(CMAKE_CONFIGURATION_TYPES)
  set(STA_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/resources")
endif()
add_custom_target(sta_resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" "${STA_RESOURCE_DIR}"
)
add_dependencies(sTA sta_resources)

# --- Setting up some directories -----------------------------
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(INCLUDE_DIR "${PROJ_ROOT}/include")
set(GLFW_LIB_FILE "${PROJ_ROOT}/lib/glfw3.lib")
set(GLAD_C_FILE "${PROJ_ROOT}/lib/glad.c")
# ------------------------------------------------------------

# Include path
target_include_directories(sTA PRIVATE "${INCLUDE_DIR}" "${INCLUDE_DIR}/imgui" "${INCLUDE_DIR}/imgui/backends" "${PROJ_ROOT}/src")

# Dear ImGui's OpenGL3 backend bundles its own GL function loader by default;
# IMGUI_IMPL_OPENGL_LOADER_CUSTOM tells it to skip that and use whatever GL
# header is already visible instead - so we force-include glad (already
# vendored and loaded by Game::init) into just that one source file
target_compile_definitions(sTA PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM $<$<CONFIG:Debug>:STA_DEBUG_BUILD>)
if(MSVC)
  set_source_files_properties(lib/imgui/backends/imgui_impl_opengl3.cpp PROPERTIES COMPILE_OPTIONS "/FI${INCLUDE_DIR}/glad/glad.h")
else()
  set_source_files_properties(lib/imgui/backends/imgui_impl_opengl3.cpp PROPERTIES COMPILE_OPTIONS "-include${INCLUDE_DIR}/glad/glad.h")
endif()

# Compiling glad.c
if(EXISTS "${GLAD_C_FILE}")
  target_sources(sTA PRIVATE "${GLAD_C_FILE}")
else()
  message(FATAL_ERROR "glad.c not found at ${GLAD_C_FILE}")
endif()

# Linking GLFW and OpenGL
if(WIN32)
  target_compile_definitions(sTA PRIVATE GLFW_STATIC)
  if (EXISTS "${GLFW_LIB_FILE}")
    target_link_libraries(sTA PRIVATE "${GLFW_LIB_FILE}" opengl32)
  else()
    message(FATAL_ERROR "glfw3.lib not found at ${GLFW_LIB_FILE}")
  endif()
else()
  find_package(glfw3 REQUIRED)
  find_package(OpenGL REQUIRED)
  target_link_libraries(sTA PRIVATE glfw OpenGL::GL)
endif()
include(CTest)
if(BUILD_TESTING)
  add_executable(sta_physics_tests tests/physics_tests.cpp src/Game/world.cpp src/Game/terrain.cpp src/Rendering/mesh.cpp lib/glad.c)
  target_include_directories(sta_physics_tests PRIVATE "${INCLUDE_DIR}" "${PROJ_ROOT}/src")
  add_test(NAME physics_regressions COMMAND sta_physics_tests)
endif()

if(BUILD_TESTING)
  add_executable(sta_traffic_tests tests/traffic_tests.cpp src/Game/world.cpp src/Game/terrain.cpp src/Game/vehicle.cpp
    src/Game/pedestrian.cpp src/Core/input.cpp src/Rendering/camera.cpp src/Rendering/mesh.cpp
    src/Rendering/renderer.cpp src/Rendering/shader.cpp src/Rendering/texture.cpp src/Rendering/shadow_map.cpp lib/glad.c)
  target_include_directories(sta_traffic_tests PRIVATE "${INCLUDE_DIR}" "${PROJ_ROOT}/src")
  if(WIN32)
    target_compile_definitions(sta_traffic_tests PRIVATE GLFW_STATIC)
    target_link_libraries(sta_traffic_tests PRIVATE "${GLFW_LIB_FILE}" opengl32)
  else()
    target_link_libraries(sta_traffic_tests PRIVATE glfw OpenGL::GL)
  endif()
  add_test(NAME traffic_patrols COMMAND sta_traffic_tests)
endif()

if(BUILD_TESTING)
  add_executable(sta_terrain_tests tests/terrain_tests.cpp src/Game/terrain.cpp)
  target_include_directories(sta_terrain_tests PRIVATE "${INCLUDE_DIR}" "${PROJ_ROOT}/src")
  add_test(NAME baked_terrain COMMAND sta_terrain_tests)
  set_tests_properties(physics_regressions traffic_patrols baked_terrain PROPERTIES WORKING_DIRECTORY "${PROJ_ROOT}")
endif()

if(BUILD_TESTING)
  add_executable(sta_island_tests tests/island_tests.cpp src/Game/world.cpp src/Game/terrain.cpp)
  target_include_directories(sta_island_tests PRIVATE "${INCLUDE_DIR}" "${PROJ_ROOT}/src")
  add_test(NAME island_roads COMMAND sta_island_tests)
  set_tests_properties(island_roads PROPERTIES WORKING_DIRECTORY "${PROJ_ROOT}")
endif()

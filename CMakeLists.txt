cmake_minimum_required(VERSION 3.16)
project(sTA LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sTA 
	src/main.cpp
	src/Rendering/shader.cpp
)

add_custom_command(TARGET sTA POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    $<TARGET_FILE_DIR:sTA>/resources
)

# --- Setting up some directories -----------------------------
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(INCLUDE_DIR "${PROJ_ROOT}/include")
set(GLFW_LIB_FILE "${PROJ_ROOT}/lib/glfw3.lib")
set(GLAD_C_FILE "${PROJ_ROOT}/lib/glad.c")
# ------------------------------------------------------------

# Include path
 target_include_directories(sTA PRIVATE "${INCLUDE_DIR}" "${PROJ_ROOT}/src/Rendering")

target_compile_definitions(sTA PRIVATE GLFW_STATIC)

# Compiling glad.c
if(EXISTS "${GLAD_C_FILE}")
  target_sources(sTA PRIVATE "${GLAD_C_FILE}")
else()
  message(FATAL_ERROR "glad.c not found at ${GLAD_C_FILE}")
endif()

# Linking GLFW
if (EXISTS "${GLFW_LIB_FILE}")
  target_link_libraries(sTA PRIVATE "${GLFW_LIB_FILE}" opengl32)
else()
  message(FATAL_ERROR "glfw3.lib not found at ${GLFW_LIB_FILE}")
endif()
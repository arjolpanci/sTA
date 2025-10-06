#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include "Rendering/shader.hpp"
#include "Rendering/basicrenderable.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float triangle_vertices[] = {
     0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,  // top
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f,  // bottom left
     0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f   // bottom right
};

unsigned int indices[] = {
    0, 1, 2,
};

static float offset_X, offset_Y = 0.0f;

int main()
{
	// Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "sTA", NULL, NULL);

    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
		return -1;
    }
	glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
	}

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1); // Enable vsync

	Shader shaderProgram("resources/shaders/simple.vert", "resources/shaders/simple.frag");

	BasicRenderable triangle1(triangle_vertices, sizeof(triangle_vertices), indices, sizeof(indices) / sizeof(indices[0]), &shaderProgram);
    triangle1.addVertexAttribPointer(0, 3, static_cast<GLsizei>(6 * sizeof(float)), (void*)0);
    triangle1.addVertexAttribPointer(1, 3, static_cast<GLsizei>(6 * sizeof(float)), (void*)(3 * sizeof(float)));

	// Render loop
    while(!glfwWindowShouldClose(window))
    {
        processInput(window);

        float timeValue = glfwGetTime();
        float greenValue = (sin(timeValue) / 2.0f) + 0.5f;

		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glClearColor(0.2f, 0.3f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        triangle1.getShader()->setFloat("offset_X", offset_X);
        triangle1.getShader()->setFloat("offset_Y", offset_Y);
        triangle1.draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
	}

	// Clean up and exit
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        offset_X -= 0.01f;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        offset_X += 0.01f;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        offset_Y += 0.01f;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        offset_Y -= 0.01f;
    }
}
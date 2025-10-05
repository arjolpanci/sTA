#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include "Rendering/shader.hpp"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float triangle1_vertices[] = {
    -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f,  // top
     0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // right
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f   // bottom
};

float triangle2_vertices[] = {
     0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f,  // top
     0.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // left
     0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f   // bottom
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
	glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
	}

	glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    unsigned int VBOs[2], VAOs[2], EBO;
	glGenVertexArrays(2, VAOs);
	glGenBuffers(2, VBOs);
    glGenBuffers(1, &EBO);

	// First triangle setup
	glBindVertexArray(VAOs[0]);
	glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle1_vertices), triangle1_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

	// Second triangle setup
	glBindVertexArray(VAOs[1]);
	glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle2_vertices), triangle2_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

	Shader shaderProgram("resources/shaders/simple.vert", "resources/shaders/simple.frag");

	// Render loop
    while(!glfwWindowShouldClose(window))
    {
        processInput(window);

        float timeValue = glfwGetTime();
        float greenValue = (sin(timeValue) / 2.0f) + 0.5f;

		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        shaderProgram.use();
        shaderProgram.setFloat("offset_X", offset_X);
        shaderProgram.setFloat("offset_Y", offset_Y);
		//shaderProgram.setFloat4("ourColor", 0.0f, greenValue, 0.0f, 1.0f);
		glBindVertexArray(VAOs[0]);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
		glBindVertexArray(VAOs[1]);
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
	}

	// Clean up and exit
	glDeleteVertexArrays(2, VAOs);
	glDeleteBuffers(2, VBOs);
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
#include <iostream>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include "Rendering/shader.hpp"
#include "Rendering/basicrenderable.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float vertices[] = {
    // positions          // colors           // texture coords
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
     0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
    -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left 
};

unsigned int indices[] = {
    0, 1, 3,
    1, 2, 3
};

static float offset_X, offset_Y = 0.0f;

int main()
{
	// Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "small Theft Auto", NULL, NULL);

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
    Shader shaderProgram2("resources/shaders/simple.vert", "resources/shaders/simple.frag");

	BasicRenderable square(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(indices[0]), &shaderProgram);
    square.addTexture(0, "resources/textures/awesomeface.png", 512, 512, 3);
    square.addTexture(1, "resources/textures/asphalt.jpg", 960, 640, 3);
    square.addVertexAttribPointer(0, 3, 8 * sizeof(float), (void*)0);
    square.addVertexAttribPointer(1, 3, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    square.addVertexAttribPointer(2, 2, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	BasicRenderable square2(vertices, sizeof(vertices), indices, sizeof(indices) / sizeof(indices[0]), &shaderProgram2);
    square2.addTexture(0, "resources/textures/awesomeface.png", 512, 512, 3);
    square2.addTexture(1, "resources/textures/asphalt.jpg", 960, 640, 3);
    square2.addVertexAttribPointer(0, 3, 8 * sizeof(float), (void*)0);
    square2.addVertexAttribPointer(1, 3, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    square2.addVertexAttribPointer(2, 2, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	// Render loop
    while(!glfwWindowShouldClose(window))
    {
        processInput(window);

        float timeValue = glfwGetTime();
        float greenValue = (sin(timeValue) / 2.0f) + 0.5f;

		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glClearColor(0.2f, 0.3f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        square.getShader()->setFloat("offset_X", offset_X);
        square.getShader()->setFloat("offset_Y", offset_Y);
        glm::mat4 trans = glm::mat4(1.0f);
        trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
        trans = glm::rotate(trans, timeValue, glm::vec3(0.0f, 0.0f, 1.0f));
        square.getShader()->setMat4("transform", trans);
        square.draw();

        glm::mat4 trans2 = glm::mat4(1.0f);
        trans2 = glm::translate(trans2, glm::vec3(-0.5f, 0.5f, 0.0f));
		trans2 = glm::scale(trans2, glm::vec3(abs(sin(timeValue/2)), abs(sin(timeValue/2)), 1.0f));
        square2.getShader()->setMat4("transform", trans2);
        square2.draw();

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
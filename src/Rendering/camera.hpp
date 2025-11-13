#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Directions for keyboard input
enum class Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera
{
public:
    // ctor with sensible defaults
    Camera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
        const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = -90.0f,
        float pitch = 0.0f);

    // return view matrix for shaders
    glm::mat4 GetViewMatrix() const;

    // movement handlers
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

    // getters
    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetFront() const { return m_front; }
    float GetZoom() const { return m_zoom; }

private:
    // camera attributes
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    // euler angles
    float m_yaw;
    float m_pitch;

    // camera options
    float m_movementSpeed;
    float m_mouseSensitivity;
    float m_zoom; // fov

    // update front/right/up vectors from current yaw/pitch
    void updateCameraVectors();
};

#endif
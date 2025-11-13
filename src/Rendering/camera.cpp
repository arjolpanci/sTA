#include "camera.hpp"
#include <cmath>

// defaults
static constexpr float DEFAULT_SPEED = 10.0f;
static constexpr float DEFAULT_SENSITIVITY = 0.1f;
static constexpr float DEFAULT_ZOOM = 45.0f;

Camera::Camera(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch)
    : m_position(position),
    m_front(glm::vec3(0.0f, 0.0f, -1.0f)),
    m_worldUp(up),
    m_yaw(yaw),
    m_pitch(pitch),
    m_movementSpeed(DEFAULT_SPEED),
    m_mouseSensitivity(DEFAULT_SENSITIVITY),
    m_zoom(DEFAULT_ZOOM)
{
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime)
{
    float velocity = m_movementSpeed * deltaTime;
    if (direction == Camera_Movement::FORWARD)
        m_position += m_front * velocity;
    if (direction == Camera_Movement::BACKWARD)
        m_position -= m_front * velocity;
    if (direction == Camera_Movement::LEFT)
        m_position -= m_right * velocity;
    if (direction == Camera_Movement::RIGHT)
        m_position += m_right * velocity;
    if (direction == Camera_Movement::UP)
        m_position += m_worldUp * velocity;
    if (direction == Camera_Movement::DOWN)
        m_position -= m_worldUp * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    if (constrainPitch) {
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset)
{
    m_zoom -= yoffset;
    if (m_zoom < 1.0f)  m_zoom = 1.0f;
    if (m_zoom > 45.0f) m_zoom = 45.0f;
}

void Camera::updateCameraVectors()
{
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    // also re-calculate the Right and Up vector
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

#include "Camera.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace VulkanRender
{
namespace
{

constexpr float radians(float degrees)
{
    return degrees * 0.017453292519943295f;
}

float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalize(Vec3 v)
{
    const float length = std::sqrt(dot(v, v));
    return {v.x / length, v.y / length, v.z / length};
}

}

Camera::Camera()
{
    updateVectors();
}

void Camera::update(GLFWwindow* window, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED))
    {
        m_firstMouse = true;
        return;
    }

    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_firstMouse = true;
    }

    processMouse(window);
    processKeyboard(window, deltaTime);
}

const Vec3& Camera::getPosition() const
{
    return m_position;
}

void Camera::updateVectors()
{
    const float yaw = radians(m_yaw);
    const float pitch = radians(m_pitch);
    m_forward = normalize({std::cos(yaw) * std::cos(pitch),
        std::sin(pitch), std::sin(yaw) * std::cos(pitch)});
    m_right = normalize(cross(m_forward, m_worldUp));
    m_up = normalize(cross(m_right, m_forward));
}

void Camera::processKeyboard(GLFWwindow* window, float deltaTime)
{
    const float distance = m_speed * std::max(deltaTime, 0.0f);
    const auto move = [this, distance](Vec3 direction, float sign)
    {
        m_position.x += direction.x * distance * sign;
        m_position.y += direction.y * distance * sign;
        m_position.z += direction.z * distance * sign;
    };

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move(m_forward, 1.0f);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move(m_forward, -1.0f);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move(m_right, -1.0f);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move(m_right, 1.0f);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) move(m_worldUp, -1.0f);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) move(m_worldUp, 1.0f);
}

void Camera::processMouse(GLFWwindow* window)
{
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    if (m_firstMouse)
    {
        m_lastMouseX = x;
        m_lastMouseY = y;
        m_firstMouse = false;
        return;
    }

    m_yaw += static_cast<float>(x - m_lastMouseX) * m_mouseSensitivity;
    m_pitch += static_cast<float>(m_lastMouseY - y) * m_mouseSensitivity;
    m_lastMouseX = x;
    m_lastMouseY = y;
    m_yaw = std::remainder(m_yaw, 360.0f);
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
    updateVectors();
}

Mat4 Camera::getViewMatrix() const
{
    return {{{m_right.x, m_up.x, -m_forward.x, 0.0f,
        m_right.y, m_up.y, -m_forward.y, 0.0f,
        m_right.z, m_up.z, -m_forward.z, 0.0f,
        -dot(m_right, m_position), -dot(m_up, m_position),
        dot(m_forward, m_position), 1.0f}}};
}

Mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.0f)
    {
        throw std::invalid_argument("Camera aspect ratio must be finite and positive");
    }

    const float scale = 1.0f / std::tan(radians(m_fov) * 0.5f);
    Mat4 projection{};
    projection.elements[0] = scale / aspectRatio;
    projection.elements[5] = -scale;
    projection.elements[10] = m_farPlane / (m_nearPlane - m_farPlane);
    projection.elements[11] = -1.0f;
    projection.elements[14] = (m_farPlane * m_nearPlane) / (m_nearPlane - m_farPlane);
    return projection;
}

}

#pragma once

#include <array>

struct GLFWwindow;

namespace VulkanRender
{

struct Vec3
{
    float x;
    float y;
    float z;
};

// Column-major storage: elements[column * 4 + row], multiplied by column vectors.
struct Mat4
{
    std::array<float, 16> elements{};
};

class Camera
{
public:
    Camera();

    void update(GLFWwindow* window, float deltaTime);
    const Vec3& getPosition() const;
    Mat4 getViewMatrix() const;
    // Right-handed, negative-Z forward, depth [0, 1], with Y inverted for Vulkan.
    Mat4 getProjectionMatrix(float aspectRatio) const;

private:
    void updateVectors();
    void processKeyboard(GLFWwindow* window, float deltaTime);
    void processMouse(GLFWwindow* window);

    Vec3 m_position{0.0f, 0.0f, 3.0f};
    Vec3 m_forward{};
    Vec3 m_right{};
    Vec3 m_up{};
    Vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_speed = 3.0f;
    float m_mouseSensitivity = 0.1f;
    float m_fov = 60.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_firstMouse = true;
};

}

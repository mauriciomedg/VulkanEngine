#include "Window.h"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace VulkanRender
{

Window::Window(int width, int height, const char* title)
{
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
}

Window::~Window()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

std::pair<int, int> Window::getFramebufferSize() const
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    return { width, height };
}

GLFWwindow* Window::getNativeHandle() const
{
    return m_window;
}

}

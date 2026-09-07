#pragma once

#include <string>

struct GLFWwindow;

namespace VulkanRender
{

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void pollEvents() const;
    GLFWwindow* getNativeHandle() const;

private:
    GLFWwindow* m_window = nullptr;
};

}

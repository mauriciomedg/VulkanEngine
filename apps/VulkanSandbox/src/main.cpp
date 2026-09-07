#include <VulkanRender/Renderer.h>

#include <exception>
#include <iostream>

int main()
{
    try
    {
        VulkanRender::Renderer renderer;
        renderer.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

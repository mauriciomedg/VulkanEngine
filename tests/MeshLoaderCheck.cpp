#include "MeshLoader.h"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    try
    {
        const auto mesh = VulkanRender::MeshLoader::load(argv[1]);
        std::cout << mesh.vertices.size() << ' ' << mesh.indices.size() << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

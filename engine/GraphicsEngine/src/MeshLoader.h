#pragma once

#include <MeshFormat.h>
#include <filesystem>

namespace VulkanRender
{
class MeshLoader
{
public:
    static MeshData load(const std::filesystem::path& path);
};
}

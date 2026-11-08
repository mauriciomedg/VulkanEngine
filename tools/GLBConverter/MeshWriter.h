#pragma once
#include <MeshFormat.h>
#include <filesystem>

void writeMesh(const std::filesystem::path& path, const VulkanRender::MeshData& mesh);

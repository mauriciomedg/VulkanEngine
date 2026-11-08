#include "MeshWriter.h"
#include <fstream>

void writeMesh(const std::filesystem::path& path, const VulkanRender::MeshData& mesh)
{
    using namespace VulkanRender::MeshFormat;
    validate(mesh);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Failed to open output mesh: " + path.string());
    writeUint32(stream, Magic);
    writeUint32(stream, Version);
    writeUint32(stream, static_cast<uint32_t>(mesh.vertices.size()));
    writeUint32(stream, static_cast<uint32_t>(mesh.indices.size()));
    for (const auto& vertex : mesh.vertices)
    {
        for (float value : vertex.position) writeFloat(stream, value);
        for (float value : vertex.color) writeFloat(stream, value);
    }
    for (uint32_t index : mesh.indices) writeUint32(stream, index);
    stream.close();
    if (!stream) throw std::runtime_error("Failed to finish writing output mesh: " + path.string());
}

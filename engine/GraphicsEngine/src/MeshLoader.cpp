#include "MeshLoader.h"
#include <fstream>
#include <string>

namespace VulkanRender
{
MeshData MeshLoader::load(const std::filesystem::path& path)
{
    using namespace MeshFormat;
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Failed to open mesh: " + path.string());
    const auto length = stream.tellg();
    if (length < 0) throw std::runtime_error("Failed to determine mesh file size");
    stream.seekg(0);
    if (readUint32(stream) != Magic) throw std::runtime_error("Invalid mesh magic");
    const uint32_t version = readUint32(stream);
    if (version != Version) throw std::runtime_error("Unsupported mesh version: "
        + std::to_string(version) + ", expected " + std::to_string(Version));
    const uint32_t vertexCount = readUint32(stream);
    const uint32_t indexCount = readUint32(stream);
    validateCounts(vertexCount, indexCount);
    const uint64_t expected = HeaderBytes + uint64_t(vertexCount) * VertexBytes
        + uint64_t(indexCount) * sizeof(uint32_t);
    if (static_cast<uint64_t>(length) < expected) throw std::runtime_error("Unexpected end of mesh file");
    if (static_cast<uint64_t>(length) != expected) throw std::runtime_error("Unexpected trailing mesh data");

    MeshData mesh;
    mesh.vertices.resize(vertexCount);
    mesh.indices.resize(indexCount);
    for (auto& vertex : mesh.vertices)
    {
        for (float& value : vertex.position) value = readFloat(stream);
        for (float& value : vertex.normal) value = readFloat(stream);
        for (float& value : vertex.color) value = readFloat(stream);
    }
    for (auto& index : mesh.indices) index = readUint32(stream);
    validate(mesh);
    return mesh;
}
}

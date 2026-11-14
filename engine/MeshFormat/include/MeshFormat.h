#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace VulkanRender
{

struct Vertex
{
    float position[3];
    float normal[3];
    float color[3];
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

namespace MeshFormat
{
// Disk order: 4 little-endian uint32 header fields, nine IEEE-754 floats per
// vertex (XYZ position, XYZ normal, RGB), then little-endian uint32 indices. No native struct dumps.
inline constexpr uint32_t Magic = 0x4853454d; // bytes "MESH"
inline constexpr uint32_t Version = 2;
inline constexpr uint64_t HeaderBytes = 16;
inline constexpr uint64_t VertexBytes = 36;
inline constexpr uint32_t MaxVertices = 4'000'000;
inline constexpr uint32_t MaxIndices = 12'000'000;

struct MeshFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t vertexCount;
    uint32_t indexCount;
};

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(Vertex) == VertexBytes);
static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, normal) == 12);
static_assert(offsetof(Vertex, color) == 24);
static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>);
static_assert(sizeof(MeshFileHeader) == HeaderBytes);

inline void validateCounts(uint64_t vertices, uint64_t indices)
{
    if (vertices == 0 || vertices > MaxVertices || indices == 0 || indices > MaxIndices
        || indices % 3 != 0)
    {
        throw std::runtime_error("Invalid vertex/index count (limits: 4M vertices, 12M triangle indices)");
    }
}

inline void validate(const MeshData& mesh)
{
    validateCounts(mesh.vertices.size(), mesh.indices.size());
    for (const auto& vertex : mesh.vertices)
    {
        for (float value : vertex.position)
            if (!std::isfinite(value)) throw std::runtime_error("Non-finite mesh position");
        for (float value : vertex.normal)
            if (!std::isfinite(value)) throw std::runtime_error("Non-finite mesh normal");
        for (float value : vertex.color)
            if (!std::isfinite(value)) throw std::runtime_error("Non-finite mesh color");
    }
    for (uint32_t index : mesh.indices)
        if (index >= mesh.vertices.size()) throw std::runtime_error("Mesh index out of range");
}

inline void writeUint32(std::ostream& stream, uint32_t value)
{
    const unsigned char bytes[] = {static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24)};
    stream.write(reinterpret_cast<const char*>(bytes), 4);
    if (!stream) throw std::runtime_error("Failed to write output mesh file");
}

inline uint32_t readUint32(std::istream& stream)
{
    unsigned char bytes[4]{};
    if (!stream.read(reinterpret_cast<char*>(bytes), 4))
        throw std::runtime_error("Unexpected end of mesh file");
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8)
        | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

inline void writeFloat(std::ostream& stream, float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUint32(stream, bits);
}

inline float readFloat(std::istream& stream)
{
    const uint32_t bits = readUint32(stream);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}
}

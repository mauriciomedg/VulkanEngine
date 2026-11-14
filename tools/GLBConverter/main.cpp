#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"
#include "MeshWriter.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{
const cgltf_accessor* requireNormals(const cgltf_primitive& primitive)
{
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    for (size_t i = 0; i < primitive.attributes_count; ++i)
    {
        const auto& attribute = primitive.attributes[i];
        if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
        if (attribute.type == cgltf_attribute_type_normal) normals = attribute.data;
    }
    if (!normals) throw std::runtime_error("Mesh has no NORMAL attribute");
    if (normals->type != cgltf_type_vec3 || normals->component_type != cgltf_component_type_r_32f
        || normals->is_sparse || !normals->buffer_view || normals->normalized)
        throw std::runtime_error("Unsupported NORMAL accessor format; non-sparse float VEC3 required");
    if (positions && normals->count != positions->count)
        throw std::runtime_error("POSITION/NORMAL accessor counts must match");
    return normals;
}

void requireSuccess(cgltf_result result, const char* operation)
{
    if (result != cgltf_result_success)
        throw std::runtime_error(std::string(operation) + " (cgltf result "
            + std::to_string(static_cast<int>(result)) + ")");
}

VulkanRender::MeshData convert(const char* input)
{
    using namespace VulkanRender;
    cgltf_options options{};
    options.type = cgltf_file_type_glb;
    cgltf_data* parsed = nullptr;
    requireSuccess(cgltf_parse_file(&options, input, &parsed), "Failed to open/parse GLB");
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(parsed, cgltf_free);
    if (data->file_type != cgltf_file_type_glb) throw std::runtime_error("GLB input required");
    if (!data->meshes_count) throw std::runtime_error("No mesh found");
    if (data->extensions_required_count) throw std::runtime_error("Required GLB extensions are unsupported");
    if (data->animations_count || data->skins_count)
        throw std::runtime_error("Animation and skinning are unsupported");
    for (size_t i = 0; i < data->buffers_count; ++i)
        if (data->buffers[i].uri) throw std::runtime_error("Only embedded GLB buffers are supported");
    for (size_t i = 0; i < data->buffer_views_count; ++i)
        if (data->buffer_views[i].has_meshopt_compression)
            throw std::runtime_error("Meshopt compression is unsupported");
    bool warnedTransforms = false;
    bool warnedSceneObjects = false;
    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        const auto& node = data->nodes[i];
        if (node.has_mesh_gpu_instancing) throw std::runtime_error("GPU instancing is unsupported");
        if (!warnedTransforms && (node.has_matrix || node.has_translation || node.has_rotation
            || node.has_scale || node.parent || node.children_count))
        {
            std::cerr << "Warning: node transforms/hierarchy are ignored; apply transforms in Blender.\n";
            warnedTransforms = true;
        }
        if (!warnedSceneObjects && (node.camera || node.light))
        {
            std::cerr << "Warning: glTF cameras/lights are ignored.\n";
            warnedSceneObjects = true;
        }
    }
    if (data->nodes_count > 1)
        std::cerr << "Warning: scene nodes/instances are ignored; each mesh definition is converted once.\n";
    if (data->materials_count || data->textures_count || data->images_count)
        std::cerr << "Warning: materials and textures are ignored; only vertex/default RGB colors are used.\n";

    requireSuccess(cgltf_load_buffers(&options, data.get(), input), "Failed to load GLB buffers");
    // Diagnose NORMAL metadata before generic validation; read only after validation.
    for (size_t m = 0; m < data->meshes_count; ++m)
        for (size_t p = 0; p < data->meshes[m].primitives_count; ++p)
            requireNormals(data->meshes[m].primitives[p]);
    requireSuccess(cgltf_validate(data.get()), "Invalid GLB data");
    MeshData mesh;
    for (size_t meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
    {
        const auto& source = data->meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < source.primitives_count; ++primitiveIndex)
        {
            const auto& primitive = source.primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles)
                throw std::runtime_error("Unsupported primitive topology; indexed triangles required");
            if (primitive.has_draco_mesh_compression || primitive.targets_count)
                throw std::runtime_error("Draco compression and morph targets are unsupported");
            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = requireNormals(primitive);
            const cgltf_accessor* colors = nullptr;
            for (size_t i = 0; i < primitive.attributes_count; ++i)
            {
                const auto& attribute = primitive.attributes[i];
                if (attribute.type == cgltf_attribute_type_position) positions = attribute.data;
                else if (attribute.type == cgltf_attribute_type_normal) continue;
                else if (attribute.type == cgltf_attribute_type_color && attribute.index == 0) colors = attribute.data;
                else std::cerr << "Warning: ignoring attribute " << attribute.name << ".\n";
            }
            if (!positions) throw std::runtime_error("Mesh has no POSITION attribute");
            if (positions->type != cgltf_type_vec3 || positions->component_type != cgltf_component_type_r_32f
                || positions->is_sparse || !positions->buffer_view || positions->normalized)
                throw std::runtime_error("Unsupported POSITION accessor format; non-sparse float VEC3 required");
            if (colors && ((colors->type != cgltf_type_vec3 && colors->type != cgltf_type_vec4)
                || colors->count != positions->count || colors->is_sparse || !colors->buffer_view
                || !((colors->component_type == cgltf_component_type_r_32f && !colors->normalized)
                    || ((colors->component_type == cgltf_component_type_r_8u
                        || colors->component_type == cgltf_component_type_r_16u) && colors->normalized))))
                throw std::runtime_error("Unsupported COLOR_0 accessor format");

            const auto* indices = primitive.indices;
            if (!indices) throw std::runtime_error("Indexed geometry required");
            if (indices->type != cgltf_type_scalar || indices->is_sparse || !indices->buffer_view
                || indices->normalized || (indices->component_type != cgltf_component_type_r_8u
                    && indices->component_type != cgltf_component_type_r_16u
                    && indices->component_type != cgltf_component_type_r_32u))
                throw std::runtime_error("Unsupported index accessor format; uint8/uint16/uint32 required");
            MeshFormat::validateCounts(positions->count, indices->count);
            MeshFormat::validateCounts(mesh.vertices.size() + positions->count, mesh.indices.size() + indices->count);
            const auto baseVertex = static_cast<uint32_t>(mesh.vertices.size());
            for (size_t i = 0; i < positions->count; ++i)
            {
                Vertex vertex{{}, {}, {0.8f, 0.8f, 0.8f}};
                if (!cgltf_accessor_read_float(positions, i, vertex.position, 3))
                    throw std::runtime_error("Failed to read POSITION accessor");
                if (!cgltf_accessor_read_float(normals, i, vertex.normal, 3))
                    throw std::runtime_error("Failed to read NORMAL accessor");
                // Double intermediates avoid overflow for finite float components.
                double lengthSquared = 0.0;
                for (float value : vertex.normal)
                {
                    if (!std::isfinite(value)) throw std::runtime_error("Non-finite NORMAL component");
                    lengthSquared += double(value) * value;
                }
                const double length = std::sqrt(lengthSquared);
                if (length <= 1e-8) throw std::runtime_error("NORMAL length is too small");
                for (float& value : vertex.normal) value = static_cast<float>(value / length);
                if (colors)
                {
                    float rgba[4]{};
                    if (!cgltf_accessor_read_float(colors, i, rgba, 4))
                        throw std::runtime_error("Failed to read COLOR_0 accessor");
                    for (int component = 0; component < 3; ++component) vertex.color[component] = rgba[component];
                }
                mesh.vertices.push_back(vertex);
            }
            for (size_t i = 0; i < indices->count; ++i)
            {
                const size_t index = cgltf_accessor_read_index(indices, i);
                if (index >= positions->count) throw std::runtime_error("GLB index out of range");
                mesh.indices.push_back(baseVertex + static_cast<uint32_t>(index));
            }
        }
    }
    MeshFormat::validate(mesh);
    return mesh;
}
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: GLBConverter <input.glb> <output.mesh>\n";
        return 1;
    }
    try
    {
        std::error_code pathError;
        if (std::filesystem::absolute(argv[1]).lexically_normal()
                == std::filesystem::absolute(argv[2]).lexically_normal()
            || std::filesystem::equivalent(argv[1], argv[2], pathError))
            throw std::runtime_error("Input and output must be different files");
        const auto mesh = convert(argv[1]);
        writeMesh(argv[2], mesh);
        std::cout << "Converted:\n  input: " << argv[1] << "\n  output: " << argv[2]
            << "\n  vertices: " << mesh.vertices.size() << "\n  indices: " << mesh.indices.size()
            << "\n  triangles: " << mesh.indices.size() / 3 << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "GLBConverter: " << error.what() << '\n';
        return 1;
    }
    return 0;
}

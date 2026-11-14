#pragma once

#include "Camera.h"
#include <cstddef>
#include <type_traits>

namespace VulkanRender
{

// GLSL column-major mat4 layout, with 16-byte column strides.
struct CameraPushConstants
{
    Mat4 model;
    Mat4 view;
    Mat4 projection;
};

struct CameraViewProjectionPushConstants
{
    Mat4 view;
    Mat4 projection;
};

static_assert(sizeof(Mat4) == 64);
static_assert(std::is_standard_layout_v<CameraPushConstants>);
static_assert(std::is_trivially_copyable_v<CameraPushConstants>);
static_assert(sizeof(CameraPushConstants) == 192);
static_assert(offsetof(CameraPushConstants, view) == 64);
static_assert(offsetof(CameraPushConstants, projection) == 128);
static_assert(sizeof(CameraViewProjectionPushConstants) == 128);
static_assert(offsetof(CameraViewProjectionPushConstants, projection) == 64);

}

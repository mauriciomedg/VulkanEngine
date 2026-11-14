#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;

layout(push_constant, std430, column_major) uniform CameraData
{
#ifndef CAMERA_IDENTITY_MODEL
    mat4 model;
#endif
    mat4 view;
    mat4 projection;
} camera;

void main()
{
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1.0);
    // Model is identity: imported normals and the light are in world space.
    // TODO: When per-object model transforms are introduced, use an appropriate
    // normal matrix, especially for non-uniform scaling.
    fragNormal = normalize(inNormal);
    fragColor = inColor;
}

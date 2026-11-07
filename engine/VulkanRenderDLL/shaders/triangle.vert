#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

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
#ifdef CAMERA_IDENTITY_MODEL
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1.0);
#else
    gl_Position = camera.projection * camera.view * camera.model * vec4(inPosition, 1.0);
#endif
    fragColor = inColor;
}

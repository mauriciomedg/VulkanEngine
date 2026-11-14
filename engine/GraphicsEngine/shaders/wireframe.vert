#version 450

layout(location = 0) in vec3 inPosition;

// Preserve OpaquePass camera offsets on both 192-byte and 128-byte devices.
// The existing model slot is unused: the model remains identity.
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
}

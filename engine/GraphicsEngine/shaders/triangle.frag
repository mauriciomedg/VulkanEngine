#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 N = normalize(fragNormal);
    // World-space direction the light rays travel; negate to point toward light.
    vec3 lightDirection = normalize(vec3(-0.5, -1.0, -0.3));
    float diffuse = max(dot(N, -lightDirection), 0.0);
    float ambient = 0.15;
    vec3 finalColor = fragColor * (ambient + diffuse);
    outColor = vec4(finalColor, 1.0);
}

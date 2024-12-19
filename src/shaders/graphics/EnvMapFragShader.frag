#version 450

layout(set = 1, binding = 0) uniform samplerCube skyboxSampler;

layout (location = 0) in vec3 fragTexCoords;
layout (location = 0) out vec4 outColor;

void main() {
    outColor = texture(skyboxSampler, fragTexCoords);
}
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model_matrix;
    mat4 view_matrix;
    mat4 projection_matrix;
} ubo;

void main() {
    gl_Position = ubo.projection_matrix * ubo.view_matrix * ubo.model_matrix * vec4(inPosition, 1.0);
    fragColor = vec3(1.0, 0.0, 0.0);
}
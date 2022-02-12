#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform TransformationMatrices{
	mat4 transformationMatrix;
} transformationMatrices;

void main() {
    gl_Position = transformationMatrices.transformationMatrix * vec4(inPosition, 1.0);
    fragColor = vec3(1.0, 0.0, 0.0);
}
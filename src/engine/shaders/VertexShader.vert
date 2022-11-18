#version 450

// Input attributes coming from the vertex buffer, stored somewhere in VRAM.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUv;

layout(location = 0) out vec3 fragColor;

// Variables coming from descriptor sets, bound at graphics pipeline creation time.
layout(set = 0, binding = 0) uniform TransformationMatrices {
	mat4 transformationMatrix;
} transformationMatrices;

void main() 
{
    gl_Position = transformationMatrices.transformationMatrix * vec4(inPosition, 1.0); // This multiplication also applies perspective division, meaning that each component of inPosition will be divided by the second number in vec4's constructor
    // gl_Position = vec4(inPosition, 1.0);
    fragColor = vec3(1.0, 0.0, 0.0);
}
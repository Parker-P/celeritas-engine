#version 450

// Input attributes coming from a vertex buffer.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUv;

layout(location = 0) out vec3 fragColor;

// Variables coming from descriptor sets.
layout(set = 0, binding = 0) uniform TransformationMatrices {
	mat4 viewAndProjection;
	mat4 localToWorld;
} transformationMatrices;

void main() 
{
    gl_Position = transformationMatrices.viewAndProjection * transformationMatrices.localToWorld * vec4(inPosition, 1.0); // This multiplication also applies perspective division, meaning that each component of inPosition will be divided by the second number in vec4's constructor.
    vec3 lightDirection = vec3(0.0, -1.0, 0.0);
	float colorMultiplier = (dot(-lightDirection, inNormal) + 1.0) / 2.0;
	fragColor = colorMultiplier * vec3(1.0, 1.0, 1.0);
}
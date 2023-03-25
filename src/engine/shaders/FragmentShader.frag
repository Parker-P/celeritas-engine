#version 450

// Input variables.
layout(location = 0) in vec3 inVertexColor;
layout (location = 1) in vec2 inUVCoord;

// Output variables.
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform LightData {
	vec3 position;
	vec4 colorIntensity; // X, Y, Z for color, W for intensity.
} lightData;

// Variables coming from descriptor sets.
layout(set = 3, binding = 0) uniform sampler2D tex1;

void main() {
    vec3 color = texture(tex1, vec2(inUVCoord.x, inUVCoord.y)).xyz;
	color = vec3(inVertexColor.x * color.x, inVertexColor.y * color.y, inVertexColor.z * color.z);
	outColor = vec4(color.xyz, 1.0f);
}
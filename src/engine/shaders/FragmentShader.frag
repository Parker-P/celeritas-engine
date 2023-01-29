#version 450

// Input variables.
layout(location = 0) in vec3 vertexColor;
layout (location = 1) in vec2 texCoord;

// Output variables.
layout(location = 0) out vec4 outColor;

// Variables coming from descriptor sets.
layout(set = 2, binding = 0) uniform sampler2D tex1;

void main() {
    vec3 color = texture(tex1,texCoord).xyz;
	color = vec3(vertexColor.x * color.x, vertexColor.y * color.y, vertexColor.z * color.z);
	outColor = vec4(color.xyz, 1.0f);
}
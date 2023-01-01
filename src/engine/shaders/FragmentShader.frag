#version 450

// Input variables.
layout(location = 0) in vec3 fragColor;
layout (location = 1) in vec2 texCoord;

// Output variables.
layout(location = 0) out vec4 outColor;

// Variables coming from descriptor sets.
layout(set = 1, binding = 1) uniform sampler2D tex1;

void main() {
    vec3 color = texture(tex1,texCoord).xyz;
	outColor = vec4(color,1.0f);
}
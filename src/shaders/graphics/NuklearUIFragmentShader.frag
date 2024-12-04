#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D overlay;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput inputColor;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the input attachment (base color)
    vec4 baseColor = subpassLoad(inputColor);
    // Sample the overlay image
    vec4 overlayColor = texture(overlay, inUV);

    // Blend the overlay on top of the base color using alpha blending
    outColor = overlayColor * overlayColor.a + baseColor * (1.0 - overlayColor.a);
//    outColor = overlayColor;
}

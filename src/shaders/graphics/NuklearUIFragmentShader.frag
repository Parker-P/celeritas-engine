#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D overlay;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput sceneColorImage;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

// Declare a push constant for gamma correction
layout(push_constant) uniform PushConstants {
    float gamma; // Gamma value
} pushConstants;

void main() {
    // Sample the input attachment (base color)
    vec4 baseColor = subpassLoad(sceneColorImage);
    // Sample the overlay image
    vec4 overlayColor = texture(overlay, inUV);

    // Blend the overlay on top of the base color using alpha blending
    vec4 blendedColor = overlayColor * overlayColor.a + baseColor * (1.0 - overlayColor.a);

    // Apply gamma correction (adjust gamma as needed, e.g., 2.2)
    blendedColor.rgb = pow(blendedColor.rgb, vec3(1.0 / pushConstants.gamma));

    outColor = blendedColor;
}

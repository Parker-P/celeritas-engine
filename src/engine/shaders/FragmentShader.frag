#version 450

// Input variables. All these values will be interpolated for each pixel this instance
// of the shader runs on.
layout (location = 0) in vec2 inUVCoord;
layout (location = 1) in vec4 inWorldSpaceNormal;
layout (location = 2) in vec3 inDirectionToLight;

// Output variables.
layout(location = 0) out vec4 outColor;

// Variables coming from descriptor sets.
layout(set = 2, binding = 0) uniform LightData {
	vec3 position;
	vec4 colorIntensity; // X, Y, Z for color, W for intensity.
} lightData;

layout(set = 3, binding = 0) uniform sampler2D baseColorTexture;

void main() 
{
	// We need to figure out, for each pixel, how much light and of what color is incoming from the
	// environment map.
	// The first step, then, is to calculate the hemisphere sub-portion of the environment sphere
	// that we are going to use for lighting, and use some math to figure out the 2D coordinate range
	// of that hemisphere as if it was unwrapped onto the image.
	// For that, we can use the fragment's normal (inWorldSpaceNormal) and use some math to figure out
	// which pixels on the environment map we will need to consider for light contribution.

	float attenuation = 1.0 / dot(inDirectionToLight, inDirectionToLight);
	vec3 lightColor = lightData.colorIntensity.xyz * lightData.colorIntensity.w * attenuation;
	vec3 diffuseLight = lightColor * max(dot(inWorldSpaceNormal.xyz, normalize(inDirectionToLight)), 0);
    vec3 textureColor = texture(baseColorTexture, vec2(inUVCoord.x, inUVCoord.y)).xyz;
	textureColor = vec3(diffuseLight.x * textureColor.x, diffuseLight.y * textureColor.y, diffuseLight.z * textureColor.z);
	outColor = vec4(textureColor.xyz, 1.0f);
}
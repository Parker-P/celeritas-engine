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

layout(set = 4, binding = 0) uniform sampler2D environmentTexture;

layout(set = 4, binding = 1) uniform textureBuffer environmentPositions;

void main() 
{
	// We need to figure out, for each pixel, how much light and of what color is incoming from the
	// environment map.
	// The first step, then, is to calculate the hemisphere sub-portion of the environment sphere
	// that we are going to use for lighting, and use some math to figure out the 2D coordinate range
	// of that hemisphere as if it was unwrapped onto the image.
	// For that, we can use the fragment's normal (inWorldSpaceNormal) and use some math to figure out
	// which pixels on the environment map we will need to consider for light contribution.

	vec3 x = texelFetch(environmentPositions, 0).xyz;
	vec3 y = texelFetch(environmentPositions, 1).xyz;
	vec3 z = texelFetch(environmentPositions, 2).xyz;

	vec3 environmentMapColor = texture(environmentTexture, vec2(0.5f,0.5f)).xyz;

	float attenuation = 1.0 / dot(inDirectionToLight, inDirectionToLight);

	vec3 lightColor = lightData.colorIntensity.xyz * lightData.colorIntensity.w * attenuation;

	vec3 diffuseLight = lightColor * max(dot(inWorldSpaceNormal.xyz, normalize(inDirectionToLight)), 0);

    vec3 textureColor = texture(baseColorTexture, vec2(inUVCoord.x, inUVCoord.y)).xyz;

	textureColor = vec3(diffuseLight.x * textureColor.x, diffuseLight.y * textureColor.y, diffuseLight.z * textureColor.z);

	vec3 finalColor = vec3((textureColor.x + environmentMapColor.x) / 2.0f, (textureColor.y + environmentMapColor.y) / 2.0f, (textureColor.z + environmentMapColor.z) / 2.0f);
	// vec3 finalColor = textureColor;

	outColor = vec4(finalColor.xyz, 1.0f);
}
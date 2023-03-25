#version 450

// Input variables. All these values will be interpolated for each pixel this instance
// of the shader runs on.
layout (location = 0) in vec2 inUVCoord;
layout (location = 1) in vec4 inWorldSpaceNormal;
layout (location = 2) in vec3 inDirectionToLight;

// Output variables.
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform LightData {
	vec3 position;
	vec4 colorIntensity; // X, Y, Z for color, W for intensity.
} lightData;

// Variables coming from descriptor sets.
layout(set = 3, binding = 0) uniform sampler2D tex1;

void main() 
{
	float attenuation = 1.0 / dot(inDirectionToLight, inDirectionToLight);
	vec3 lightColor = lightData.colorIntensity.xyz * lightData.colorIntensity.w * attenuation;
	vec3 diffuseLight = lightColor * max(dot(inWorldSpaceNormal.xyz, normalize(inDirectionToLight)), 0);
    vec3 textureColor = texture(tex1, vec2(inUVCoord.x, inUVCoord.y)).xyz;
	textureColor = vec3(diffuseLight.x * textureColor.x, diffuseLight.y * textureColor.y, diffuseLight.z * textureColor.z);
	outColor = vec4(textureColor.xyz, 1.0f);
}
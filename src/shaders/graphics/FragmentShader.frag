#version 450

// Input variables. All these values will be interpolated for each pixel this instance
// of the shader runs on, with the base and target interpolation values coming from the
// attributes of the 2 closest vertices.
layout (location = 0) in vec2 inUVCoord;
layout (location = 1) in vec3 inWorldSpaceNormal;
layout (location = 2) in vec3 inDirectionToLight;
layout (location = 3) in vec3 inDirectionToCamera;

// Output variables.
layout(location = 0) out vec4 outColor;

// Variables coming from descriptor sets. See ShaderResources.cpp for more info on descriptor sets.
layout(set = 2, binding = 0) uniform LightData {
	vec3 position;
	vec4 colorIntensity; // X, Y, Z for color, W for intensity.
} lightData;

// Base color texture.
layout(set = 3, binding = 0) uniform sampler2D albedoMap;
layout(set = 3, binding = 1) uniform sampler2D roughnessMap;
layout(set = 3, binding = 2) uniform sampler2D metalnessMap;

// Environment map.
layout(set = 4, binding = 0) uniform samplerCube environmentMap;

void main() 
{
	// Only do the calculations if the pixel is actually visible.
	if (dot(inDirectionToCamera, inWorldSpaceNormal) > -0.1f) {

        // Sample the textures that will be used in our Cook-Torrance material model.
		vec4 albedoMapColor = texture(albedoMap, inUVCoord);
		vec4 roughnessMapColor = texture(roughnessMap, inUVCoord);
		vec4 metalnessMapColor = texture(metalnessMap, inUVCoord);

        // Calculate the vector resulting from an imaginary ray shooting out of the camera and bouncing off
		// the pixel on the surface we want to render.
		vec4 reflected = reflect(vec4(-inDirectionToCamera.xyz, 0.0f), vec4(inWorldSpaceNormal.xyz, 0.0f));

		// Now we need to get the roughness value as a grayscale value. We calculate the average of all three color channels in case the image is not already grayscale.
        float roughness = (roughnessMapColor.x + roughnessMapColor.y + roughnessMapColor.z) / 3.0f;
        float metalness = (metalnessMapColor.x + metalnessMapColor.y + metalnessMapColor.z) / 3.0f;

        vec4 environmentMapColor = textureLod(environmentMap, reflected.xyz, 3.0f);
        outColor = normalize(environmentMapColor + albedoMapColor);
//        outColor = albedoMapColor;
	}
	else {
		outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}
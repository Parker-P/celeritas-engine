#version 450
#define M_PI 3.1415926535f

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

// Cook-Torrance BRDF calculation function.
vec4 CookTorrance(vec3 normalDir, vec3 directionToCamera, vec3 directionToLight, vec4 albedoColor, vec4 environmentMapColor, float roughness, float metalness) 
{
    // Calculate some basic variables used throughout the function.
    vec3 halfVec = normalize(directionToLight + directionToCamera);
    float NdotH = max(dot(normalDir, halfVec), 0.0);
    float NdotL = max(dot(normalDir, directionToLight), 0.0);
    float NdotV = max(dot(normalDir, directionToCamera), 0.0);
    float roughness2 = roughness * roughness;
    float NdotH2 = NdotH * NdotH;

    // Calculate the Fresnel-Schlick approximation for the specular reflection. This calculation is used as a scalar factor to increase
    // or decrease the shinyness/intensity of the final color based on the angle of incidence of the incoming light, to simulate
    // the amount of photons bouncing off the surface and reaching the camera directly from the light. This factor is
    // strongest if the direction from the pixel to the camera, from the pixel to the light, and the normal vector are the same.
    vec4 F0 = mix(vec4(0.04), albedoColor, metalness);
    vec4 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // Geometric term (Smith's method). The geometric factor is another scalar that is used to increase or decrease the intensity of the final
    // color based on the roughness of the material, in order to simulate the shading that happens on each microfacet due to it being
    // angled in a direction that deflects photons away from the camera or towards the camera from the light source.
    // The rougher the material, the steeper (on average) the direction on the microfacet will be, which could increase or decrease
    // the amount of photons reaching the camera from the light source, thus the intensity of the final color from the perspective of the camera.
    float k = (roughness2 + 1.0) / 8.0;
    float k2 = k * k;
    float G1 = NdotV / (NdotV * (1.0 - k) + k);
    float G2 = NdotL / (NdotL * (1.0 - k) + k);
    float geometric = G1 * G2;

    // Distribution term (Cook-Torrance GGX).
    float distribution = NdotH2 * (roughness2 - 1.0) + 1.0;
    distribution = (roughness2 + 1.0) * NdotH2 * NdotH2 / (M_PI * distribution * distribution);

    // Cook-Torrance BRDF.
    vec4 specular = (F * geometric * distribution) / (4.0 * NdotL * NdotV + 0.001);

    // Lambertian diffuse reflection.
    vec4 scaledEnvironmentMapColor = environmentMapColor * roughness;
    vec4 diffuse = normalize(albedoColor / M_PI + scaledEnvironmentMapColor);

    // Final color = diffuse + specular.
    return diffuse + specular;
}

// Generates a pesudo-random float number.
float RandomFloat(vec2 seed){
    return fract(sin(dot(seed, vec2(12.9898f, 78.233f))) * 43758.5453f);
}

void main() 
{
	// Only do the calculations if the pixel is actually visible.
	if (dot(inDirectionToCamera, inWorldSpaceNormal) > 0.0f) {

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
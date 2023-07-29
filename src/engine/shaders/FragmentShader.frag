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
layout(set = 3, binding = 1) uniform sampler2D metallicMap;
layout(set = 3, binding = 2) uniform sampler2D roughnessMap;

// Environment map.
layout(set = 4, binding = 0) uniform samplerCube environmentMap;

vec3 RotateVector(vec3 vectorToRotate, vec3 axis, float angleDegrees) 
{
    float angleRadians = radians(angleDegrees);
	float cosine = cos(angleRadians / 2.0f);
	float sine = sin(angleRadians / 2.0f);
	return (vec4(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * vec4(vectorToRotate, 1.0f)).xyz;
}

// Cook-Torrance BRDF calculation function.
vec4 CookTorrance(vec3 normalDir, vec3 directionToCamera, vec3 directionToLight, vec4 albedoColor, float roughness, float metalness) 
{
    vec3 halfVec = normalize(directionToLight + directionToCamera);
    float NdotH = max(dot(normalDir, halfVec), 0.0);
    float NdotL = max(dot(normalDir, directionToLight), 0.0);
    float NdotV = max(dot(normalDir, directionToCamera), 0.0);

    float roughness2 = roughness * roughness;
    float NdotH2 = NdotH * NdotH;

    // Fresnel Schlick approximation for the specular reflection.
    vec4 F0 = mix(vec4(0.04), albedoColor, metalness);
    vec4 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // Geometric term (Smith's method).
    float k = (roughness2 + 1.0) / 8.0;
    float k2 = k * k;
    float G1 = NdotV / (NdotV * (1.0 - k) + k);
    float G2 = NdotL / (NdotL * (1.0 - k) + k);
    float geometric = G1 * G2;

    // Distribution term (Cook-Torrance GGX).
    float distribution = NdotH2 * (roughness2 - 1.0) + 1.0;
    distribution = (roughness2 + 1.0) * NdotH2 * NdotH2 / (M_PI * distribution * distribution);

    // Cook-Torrance BRDF
    vec4 specular = (F * geometric * distribution) / (4.0 * NdotL * NdotV + 0.001);

    // Lambertian diffuse reflection
    vec4 diffuse = albedoColor / M_PI;

    // Final color = diffuse + specular
    return diffuse + specular;
}

void main() 
{
		// Only do the calculations if the pixel is actually visible.
		if (dot(inDirectionToCamera, inWorldSpaceNormal) > 0.0f) {

		// Calculate the vector resulting from an imaginary ray shooting out of the camera and bouncing off
		// the pixel on the surface we want to render.
		vec4 reflected = reflect(vec4(-inDirectionToCamera.xyz, 0.0f), vec4(inWorldSpaceNormal.xyz, 0.0f));

		// Sample the environment map using the calculated reflected vector. Here we are using an overload of the texture function
		// that takes a samplerCube and a 3D coordinate and spits out the color of the texture at the intersection
		// of a cube's face with that 3D vector.
		vec4 environmentMapColor = texture(environmentMap, reflected.xyz);

        // Sample the textures that will be used in our Cook-Torrance material model.
		vec4 albedoMapColor = texture(albedoMap, inUVCoord);
		vec4 roughnessMapColor = texture(roughnessMap, inUVCoord);
		vec4 metallicMapColor = texture(metallicMap, inUVCoord);

        // Average between all three color channels to get a single value.
        float roughness = (metallicMapColor.x + metallicMapColor.y + metallicMapColor.z) / 3.0f;
        float metalness = (metallicMapColor.x + metallicMapColor.y + metallicMapColor.z) / 3.0f;

        outColor = CookTorrance(inWorldSpaceNormal, inDirectionToCamera, inDirectionToLight, albedoMapColor, roughness, metalness);

//		outColor = normalize(environmentMapColor + albedoMapColor);
	}
	else {
		outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}
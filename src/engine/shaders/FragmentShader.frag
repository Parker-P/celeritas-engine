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

// Environment map data.
layout(set = 4, binding = 0) uniform textureBuffer environmentColors;
layout(set = 4, binding = 1) uniform textureBuffer environmentPositions;
layout(set = 4, binding = 2) uniform Size {
	int environmentDataEntryCount;
} envSize;

vec3 RotateVector(vec3 vectorToRotate, vec3 axis, float angleDegrees) {
    float angleRadians = radians(angleDegrees);
	float cosine = cos(angleRadians / 2.0f);
	float sine = sin(angleRadians / 2.0f);
	return (vec4(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * vec4(vectorToRotate, 1.0f)).xyz;
}

void main() 
{
	float sampleStep = 0.05f;
	vec3 samplePosition = cross(inWorldSpaceNormal.xyz, vec3(0.0f, 0.0f, 1.0f));
	vec3 colorFromTexture = texture(baseColorTexture, vec2(inUVCoord.x, inUVCoord.y)).xyz;
	vec3 colorAfterEnvironmentMapLighting = colorFromTexture;

	// The idea behind using an environment map is the following:
	// On the CPU side, take the environment map and sample it in an array, where for each cell, 
	// have the world space position of the sampled pixel mapped onto a sphere, and the colour
	// of the pixel. You will be passing this into the shader.
	// 
	// In the fragment shader, use the cross product of the normal vector and a random vector. This
	// will give you a vector that represents a random direction at the base of the hemisphere above
	// the pixel to render, in world space.
	// 
	// You then rotate this vector around the zenith and azimuth angles of the entire hemisphere, calculate
	// the spherical coordinates on the hemisphere (in world space) and get the corresponding colour from
	// the matrix of precomputed samples we passed in.
	// 
	// As you loop through all directions, you add up the light contributions to a variable.

	// Start sampling the environment map.
	for (float sampleZenith = 0.0f; sampleZenith < 90.0f; sampleZenith += sampleStep) {
        for (float sampleAzimuth = 0.0f; sampleAzimuth < 360.0f; sampleAzimuth += sampleStep) {

			// Build a quaternion to rotate the samplePosition vector around inWorldSpaceNormal.
		    vec3 desiredEnvironmentMapPixelPosition = RotateVector(samplePosition, inWorldSpaceNormal.xyz, sampleStep);
			
			// Find the color based on the closest available position vector (to desiredEnvironmentMapPixelPosition) in the environment map's pixel positions.
			float minimumDelta = 1.0f;
			vec3 environmentMapPixelPosition = texelFetch(environmentPositions, 0).xyz;
			vec3 closestEnvironmentMapPixelPosition = environmentMapPixelPosition;

			int indexOfClosestPosition = 0; // Index in the array of environment map's pixel positions of the closest available position vector to the position vector we want to sample from.

			for (int i = 0; i < envSize.environmentDataEntryCount; ++i) {
				float delta = 1.0f - dot(desiredEnvironmentMapPixelPosition, environmentMapPixelPosition);

				if (delta < minimumDelta) {
					indexOfClosestPosition = i;
				}

				environmentMapPixelPosition = texelFetch(environmentPositions, i).xyz;
			}

			vec3 sampledColor = texelFetch(environmentColors, indexOfClosestPosition).xyz;
			vec3 sampledPosition = texelFetch(environmentPositions, indexOfClosestPosition).xyz;

			// Now we calculate the average between the 2 colors (the color from the texture and the color from the sampled environment map's texture).
			// The light contribution from the sampled color from the environment map needs to be weighted based on the angle of incidence with the current surface pixel's normal.
			// F.E.: if the color of the current surface pixel is red, and the color from the enviroment map's pixel is white and is coming from the side 
			// (perpendicular to the current surface pixel), then the contribution from the environment map's pixel color will be zero, therefore the surface pixel remains red, 
			// without it being affected by the light coming from the environment map at all. On the other hand, assume that the angle is 0, so the light from the environment pixel
			// is parallel to the surface pixel's normal (therefore the light is striking the surface head on), then the contribution from the environment map's light will be at 
			// its maximum, so the resulting color is a lighter red (because of the averaged value between red (the surface's color) and white (the light).
			float weightOfSampledColorFromEnvMap = dot(inWorldSpaceNormal.xyz, sampledPosition);
			vec3 weightedTextureColor = vec3(colorAfterEnvironmentMapLighting.r * (1.0f - weightOfSampledColorFromEnvMap) + sampledColor.r * weightOfSampledColorFromEnvMap, 
				colorAfterEnvironmentMapLighting.g * (1.0f - weightOfSampledColorFromEnvMap) + sampledColor.g * weightOfSampledColorFromEnvMap,
				colorAfterEnvironmentMapLighting.b * (1.0f - weightOfSampledColorFromEnvMap) + sampledColor.b * weightOfSampledColorFromEnvMap);
			
			colorAfterEnvironmentMapLighting = vec3((colorAfterEnvironmentMapLighting.r + weightedTextureColor.r) / 2.0f, (colorAfterEnvironmentMapLighting.g + weightedTextureColor.g) / 2.0f, (colorAfterEnvironmentMapLighting.b + weightedTextureColor.b) / 2.0f);
			samplePosition = desiredEnvironmentMapPixelPosition;
		}
	}

//	vec3 x = texelFetch(environmentPositions, 0).xyz;
//	vec3 y = texelFetch(environmentPositions, 1).xyz;
//	vec3 z = texelFetch(environmentPositions, 2).xyz;

//	vec3 environmentMapColor = texture(environmentTexture, vec2(0.5f,0.5f)).xyz;

//	float attenuation = 1.0 / dot(inDirectionToLight, inDirectionToLight);
//	vec3 lightColor = lightData.colorIntensity.xyz * lightData.colorIntensity.w * attenuation;
//	vec3 diffuseLight = lightColor * max(dot(inWorldSpaceNormal.xyz, normalize(inDirectionToLight)), 0);

//    vec3 textureColor = texture(baseColorTexture, vec2(inUVCoord.x, inUVCoord.y)).xyz;
//	textureColor = vec3(diffuseLight.x * textureColor.x, diffuseLight.y * textureColor.y, diffuseLight.z * textureColor.z);

//	vec3 finalColor = vec3((textureColor.x + environmentMapColor.x) / 2.0f, (textureColor.y + environmentMapColor.y) / 2.0f, (textureColor.z + environmentMapColor.z) / 2.0f);
	// vec3 finalColor = textureColor;

	outColor = vec4(colorAfterEnvironmentMapLighting.xyz, 1.0f);
}
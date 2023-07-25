#version 450

// Input variables. All these values will be interpolated for each pixel this instance
// of the shader runs on, with the base and target interpolation values coming from the
// attributes of the 2 closest vertices.
layout (location = 0) in vec2 inUVCoord;
layout (location = 1) in vec4 inWorldSpaceNormal;
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
layout(set = 3, binding = 0) uniform sampler2D baseColorMap;

// Environment map.
layout(set = 4, binding = 0) uniform samplerCube environmentMap;

vec3 RotateVector(vec3 vectorToRotate, vec3 axis, float angleDegrees) {
    float angleRadians = radians(angleDegrees);
	float cosine = cos(angleRadians / 2.0f);
	float sine = sin(angleRadians / 2.0f);
	return (vec4(cosine, axis.x * sine, axis.y * sine, axis.z * sine) * vec4(vectorToRotate, 1.0f)).xyz;
}

void main() 
{
		// Only do the calculations if the pixel is actually visible.
		if (dot(vec4(inDirectionToCamera, 0.0f), inWorldSpaceNormal) > 0.0f) {

		// Calculate the vector resulting from an imaginary ray shooting out of the camera and bouncing off
		// the pixel on the surface we want to render.
		vec4 reflected = reflect(vec4(-inDirectionToCamera.xyz, 0.0f), vec4(inWorldSpaceNormal.xyz, 0.0f));

		// Sample the environment map using the calculated reflected vector. Here we are using an overload of the texture function
		// that takes a samplerCube and a 3D coordinate and spits out the color of the texture at the intersection
		// of a cube's face with that 3D vector.
		vec4 environmentMapColor = texture(environmentMap, reflected.xyz);
		vec4 baseColorMapColor = texture(baseColorMap, inUVCoord);

		outColor = normalize(environmentMapColor + baseColorMapColor);
	}
	else {
		outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}
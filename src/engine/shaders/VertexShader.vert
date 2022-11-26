#version 450

// https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html

// Input attributes coming from a vertex buffer.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUv;

layout(location = 0) out vec3 fragColor;

//Push constants.
layout(push_constant) uniform constants
{
	float width;
	float height;
	float nearClipDistance;
	float farClipDistance;
} activeCameraProperties;

// Variables coming from descriptor sets.
layout(set = 0, binding = 0) uniform TransformationMatrices {
	mat4 worldToCamera;
	mat4 objectToWorld;
} transformationMatrices;

void main() 
{
    // The idea behind the projection transformation is using the camera as if you were standing behind a glass window: whatever you see out the window gets projected onto
	// the glass.
	// Lets say that YOU are the camera. Now do the following:
	// 1) Go to a window and stand about 1 meter behind it, making sure that your head is roughly at the center of the window.
	// 2) Close one eye so you have only one focal point.
	// 3) Now focus on something outside the window (has to be a specific point, like the tip of the roof of a house or the leaf of a tree) 
	// 4) Next, move your head towards the point you are focusing on in a straight line, untill you eventually hit the window with your head
	// That point on the glass, right in front of your pupil, is the intersection between your screen and the point is space outside your house that you were focusing on.
	// This is exactly how the projection transformation was thought about but in reverse; the point in space outside the window moves towards your pupil untill it
	// eventually hits the window. When it hits the window, you just take the distance from the center of the window to the hit point: these will be your 2D screen
	// coordinates.

	vec4 cameraSpacePosition = /*transformationMatrices.worldToCamera **/ transformationMatrices.objectToWorld * vec4(inPosition.x, inPosition.y, inPosition.z, 1.0f);
	float xCoord = (activeCameraProperties.nearClipDistance * cameraSpacePosition.x) / cameraSpacePosition.z;
	float yCoord = (-activeCameraProperties.nearClipDistance * cameraSpacePosition.y) / cameraSpacePosition.z;
	float zCoord = (cameraSpacePosition.z - activeCameraProperties.nearClipDistance) / (activeCameraProperties.farClipDistance - activeCameraProperties.nearClipDistance);

	// Remember that the next stages are going to divide each component of gl_position by its w component, meaning that
	// gl_position = vec4(gl_position.x / gl_position.w, gl_position.y / gl_position.w, gl_position.z / gl_position.w, gl_position.w / gl_position.w)
	gl_Position = vec4(xCoord, yCoord, zCoord, 1.0f);
	
	// Calculate the color of each vertex so the fragment shader can interpolate the pixels rendered between them.
	vec4 temp = vec4(0.0, -1.0, 0.0, 0.0);
	vec3 lightDirection = vec3(temp.x, temp.y, temp.z);
	float colorMultiplier = (dot(-lightDirection, inNormal) + 1.0) / 2.0;
	fragColor = colorMultiplier * vec3(1.0, 1.0, 1.0); // RGB
}
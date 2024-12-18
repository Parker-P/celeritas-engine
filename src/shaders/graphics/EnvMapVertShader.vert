#version 450

// https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html

// Input attributes coming from a vertex buffer.
layout(location = 0) in vec3 inPosition;

// Output variables to send to the next shader stages.
layout (location = 0) out vec2 outUVCoord;
layout (location = 1) out vec3 outWorldSpaceNormal;

// Data used to project the world space coordinates of the vertex into Vulkan's viewable volume.
layout(set = 0, binding = 0) uniform CameraData {
	float tanHalfHorizontalFov;
	float aspectRatio;
	float nearClipDistance;
	float farClipDistance;
	mat4 worldToCamera;
	vec3 worldSpacePosition;
} cameraData;

// Variables coming from descriptor with binding number 0 at descriptor set 1.
layout(set = 1, binding = 0) uniform ObjectData {
	mat4 objectToWorld;
} objectData;

void main() 
{
	vec4 vertexWorldSpacePosition = objectData.objectToWorld * vec4(inPosition.xyz, 1.0f);
	vec4 worldSpaceNormal = objectData.objectToWorld * vec4(inNormal.xyz, 0.0f);
	vec4 cameraSpacePosition = cameraData.worldToCamera * vertexWorldSpacePosition;

	// The idea behind the projection transformation is using the camera as if you were standing behind a glass window: whatever you see out the window gets projected onto
	// the glass.
	// Lets say that YOU are the camera. Now do the following:
	// 1) Go to a window and stand about 1 meter behind it, making sure that your head is roughly at the center of the window.
	// 2) Close one eye so you have only one focal point.
	// 3) Now focus on something outside the window (has to be a specific point, like the tip of the roof of a house or the leaf of a tree) 
	// 4) Next, move your head towards the point you are focusing on in a straight line, untill you eventually hit the window with your head
	// That point on the glass, right in front of your pupil, is what we're looking to calculate.
	// This is exactly how the projection transformation was thought about but in reverse; the point in space outside the window moves towards your pupil until it
	// eventually hits the window. When it hits the window, you just take the distance from the center of the window to the hit point: these will be your 2D screen
	// coordinates.
	// This calculation is based on the fact that Vulkan's coordinate system is X right, Y down, Z forward (follows the right hand rule).
	// In our engine we have X right, Y up, Z forward (we follow the left hand rule).
	float xCoord = cameraSpacePosition.x / (cameraData.tanHalfHorizontalFov * cameraData.aspectRatio);
	float yCoord = -cameraSpacePosition.y / cameraData.tanHalfHorizontalFov;
	float zCoord = (cameraSpacePosition.z - cameraData.nearClipDistance) / (cameraData.farClipDistance - cameraData.nearClipDistance);

	// Remember that the next stages are going to divide each component of gl_position by its w component, meaning that
	// gl_position = vec4(gl_position.x / gl_position.w, gl_position.y / gl_position.w, gl_position.z / gl_position.w, gl_position.w / gl_position.w)
	gl_Position = vec4(xCoord, yCoord, zCoord, cameraSpacePosition.z);
	
	// Forward the uv coordinate of the vertex to the fragment stage.
	outUVCoord = inUv;
	outWorldSpaceNormal = vec3(worldSpaceNormal.xyz);
//	outWorldSpaceNormal = vec4(inNormal.xyz, 1.0f);
}
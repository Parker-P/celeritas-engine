#version 450

// https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html

// Input attributes coming from a vertex buffer.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUv;

layout(location = 0) out vec3 fragColor;

// Variables coming from descriptor sets.
layout(set = 0, binding = 0) uniform UniformBuffer {
	float tanHalfHorizontalFov;
	float aspectRatio;
	float nearClipDistance;
	float farClipDistance;
	mat4 worldToCamera;
	mat4 objectToWorld;
} uniformBuffer;

void main() 
{
    vec4 cameraSpacePosition = uniformBuffer.worldToCamera * uniformBuffer.objectToWorld * vec4(inPosition.x, inPosition.y, inPosition.z, 1.0f);
	
	// The idea behind the projection transformation is using the camera as if you were standing behind a glass window: whatever you see out the window gets projected onto
	// the glass.
	// Lets say that YOU are the camera. Now do the following:
	// 1) Go to a window and stand about 1 meter behind it, making sure that your head is roughly at the center of the window.
	// 2) Close one eye so you have only one focal point.
	// 3) Now focus on something outside the window (has to be a specific point, like the tip of the roof of a house or the leaf of a tree) 
	// 4) Next, move your head towards the point you are focusing on in a straight line, untill you eventually hit the window with your head
	// That point on the glass, right in front of your pupil, is what we're looking to calculate.
	// This is exactly how the projection transformation was thought about but in reverse; the point in space outside the window moves towards your pupil untill it
	// eventually hits the window. When it hits the window, you just take the distance from the center of the window to the hit point: these will be your 2D screen
	// coordinates.
	// This calculation is based on the fact that Vulkan's coordinate system is X right, Y down, Z forward and follows the right hand rule.
	// In our engine we have X right, Y up, Z forward, and we follow the left hand rule.
	float xCoord = (cameraSpacePosition.x) / (cameraSpacePosition.z * uniformBuffer.tanHalfHorizontalFov * uniformBuffer.aspectRatio);
	float yCoord = (-cameraSpacePosition.y) / (cameraSpacePosition.z * uniformBuffer.tanHalfHorizontalFov);
	float zCoord = (cameraSpacePosition.z - uniformBuffer.nearClipDistance) / (uniformBuffer.farClipDistance - uniformBuffer.nearClipDistance);

	// Remember that the next stages are going to divide each component of gl_position by its w component, meaning that
	// gl_position = vec4(gl_position.x / gl_position.w, gl_position.y / gl_position.w, gl_position.z / gl_position.w, gl_position.w / gl_position.w)
	gl_Position = vec4(xCoord, yCoord, zCoord, 1.0f);
	
	// Calculate the color of each vertex so the fragment shader can interpolate the pixels rendered between them.
	vec4 temp = vec4(0.0f, -1.0f, 0.0f, 0.0f);
	vec3 lightDirection = vec3(temp.x, temp.y, temp.z);
	float colorMultiplier = (dot(-lightDirection, inNormal) + 1.0f) / 2.0f;
	fragColor = colorMultiplier * vec3(1.0f, 1.0f, 1.0f); // RGB
}
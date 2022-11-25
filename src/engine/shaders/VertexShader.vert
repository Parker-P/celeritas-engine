#version 450

// https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html

// Input attributes coming from a vertex buffer.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inUv;

layout(location = 0) out vec3 fragColor;

// Variables coming from descriptor sets.
layout(set = 0, binding = 0) uniform TransformationMatrices {
	mat4 engineToVulkan;
	mat4 viewAndProjection;
	mat4 objectToEngineWorld;
} transformationMatrices;

void main() 
{
    // Matrix used to preserve the correct normalized Z value after perspective division, which is done in the succeeding steps of the rendering
	// pipeline.
	mat4 zScale;
	zScale[0][0] = 1.0f; zScale[1][0] = 0.0f; zScale[2][0] = 0.0f;         zScale[3][0] = 0.0f;
	zScale[0][1] = 0.0f; zScale[1][1] = 1.0f; zScale[2][1] = 0.0f;		   zScale[3][1] = 0.0f;
	zScale[0][2] = 0.0f; zScale[1][2] = 0.0f; zScale[2][2] = inPosition.z; zScale[3][2] = 0.0f;
	zScale[0][3] = 0.0f; zScale[1][3] = 0.0f; zScale[2][3] = 0.0f;		   zScale[3][3] = 1.0f;


	// What happens is that you have a cubic viewing volume right in front of you, within which everything will be rendered.
	// Just imagine having a cube right in front of you. The face closer to you is your monitor. The volume inside of the cube is
	// the range within which everything will be rendered.
	// This viewing volume is a cuboid that ranges from [-1,1,0] (from the perspective of your monitor, this is the lower-left-close vertex 
	// of the cube) to [1,-1,1] (upper right far vertex of the cube). Anything you want to render eventually has to fall within this range.
	// This means that gl_position needs to have an output coordinate that falls within this range for it to be
	// within the visible range. The goal of the projection and zScale matrices is to make sure that the final value of gl_Position
	// falls within this range.
	gl_Position = zScale * transformationMatrices.viewAndProjection * transformationMatrices.objectToEngineWorld * vec4(inPosition, 1.0f);
	
	// Calculate the color of each vertex so the fragment shader can interpolate the pixels rendered between them.
	vec4 temp = vec4(0.0, -1.0, 0.0, 0.0);
	vec3 lightDirection = vec3(temp.x, temp.y, temp.z);
	float colorMultiplier = (dot(-lightDirection, inNormal) + 1.0) / 2.0;
	fragColor = colorMultiplier * vec3(1.0, 1.0, 1.0); // RGB
}
#pragma once

namespace Engine::Core::Renderer::VulkanEntities {
	/*
	- DESCRIPTORS AND DESCRIPTOR SETS -

		A descriptor is a special opaque shader variable that shaders use to access buffer and image resources in an indirect fashion. It can be thought of as a "pointer" to a resource. 
		The Vulkan API allows these variables to be changed between draw operations so that the shaders can access different resources for each draw.
		In the sample example, you have only one uniform buffer. But you could create two uniform buffers, each with a different MVP to give different views of the scene. 
		You could then easily change the descriptor to point to either uniform buffer to switch back and forth between the MVP matrices.
		A descriptor set is called a "set" because it can refer to an array of homogenous resources that can be described with the same layout binding. (The "layout binding" will be explained shortly.)
		You are not using textures in this sample, but one possible way to use multiple descriptors is to construct a descriptor set with two descriptors, with each descriptor referencing a separate texture. Both textures are therefore available during a draw. A command in a command buffer could then select the texture to use by specifying the index of the desired texture.
		It is important to note that you are just working on describing the descriptor set here and are not actually allocating or creating the descriptor set itself, which you will do later, in the descriptor_set sample.
		To describe a descriptor set, you use a descriptor set layout.

		Descriptor Set Layouts
		A descriptor set layout is used to describe the content of a list of descriptor sets. You also need one layout binding for each descriptor set, which you use to describe each descriptor set.
		You happen to be making only one descriptor set, so the only choice for the binding member is 0.
		Since this descriptor is referencing a uniform buffer, you set the descriptorType appropriately.
		You have only one descriptor in this descriptor set, which is indicated by the descriptorCount member.
		You indicate that this uniform buffer resource is to be bound to the shader vertex stage.
		With the binding for our one descriptor set defined, you are ready to create the descriptor set layout.

	- PIPELINE LAYOUTS -

		A pipeline layout contains a list of descriptor set layouts. It also can contain a list of push constant ranges, which is an alternate way to pass constants to a shader and will not be covered here.
		As with the descriptor sets, you are just defining the layout. The actual descriptor set is allocated and filled in with the uniform buffer reference later.

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pNext = NULL;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pipelineLayoutCreateInfo.pPushConstantRanges = NULL;
		pipelineLayoutCreateInfo.setLayoutCount = NUM_DESCRIPTOR_SETS;
		pipelineLayoutCreateInfo.pSetLayouts = info.desc_layout.data();

		res = vkCreatePipelineLayout(info.device, &pipelineLayoutCreateInfo, NULL,
									 &info.pipeline_layout);
		You will use the pipeline layout later to create the graphics pipeline.

	- SHADER REFERENCING OF DESCRIPTORS -

		It is worth pointing out that the shader explicitly references these descriptors in the shader language.

		For example, in GLSL:

		 layout (set=M, binding=N) uniform sampler2D variableNameArray[I];

		M refers the the M'th descriptor set layout in the pSetLayouts member of the pipeline layout
		N refers to the N'th descriptor set (binding) in M's pBindings member of the descriptor set layout
		I is the index into the array of descriptors in N's descriptor set
		The layout code for the uniform buffer in the vertex shader that you will use looks like:

		layout (std140, binding = 0) uniform bufferVals {
			mat4 mvp;
		} myBufferVals;
		This maps the uniform buffer contents to the myBufferVals structure. "set=M" was not specified and defaults to 0.
	
	SOURCE: https://vulkan.lunarg.com/doc/view/1.2.154.1/windows/tutorial/html/08-init_pipeline_layout.html*/
	class DescriptorSet {
		VkBuffer uniform_buffer_; //This is the buffer that contains the uniform_buffer_data_ struct
		VkDeviceMemory uniform_buffer_memory_; //Provides memory allocation info to vulkan when creating the uniform buffer
		VkDescriptorSetLayout descriptor_set_layout_; //This is used to describe the layout of this descriptor set so that Vulkan can tell the GPU how to read the data it contains
	};
}
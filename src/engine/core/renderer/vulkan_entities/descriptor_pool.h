#pragma once

namespace Engine::Core::Renderer::VulkanEntities {
	/*

	A descriptor pool maintains a pool of descriptors, from which descriptor sets are allocated.Descriptor pools are externally synchronized, meaning that the application must not allocate and /or free descriptor sets from the same pool in multiple threads simultaneously.
	Descriptor sets are the main way of connecting CPU data to the GPU.

	DEFINITIONS:

	DescriptorPool - A big heap of available UBOs, textures, storage buffers, etc that 
	can be used when instantiating DescriptorSets. 
	This allows you to allocate a big heap of types ahead of time so that later on you don't
	have to ask the gpu to do expensive allocations.
	
	DescriptorSetLayout - Defines the structure of a descriptor set, a template of sorts. 
	Think of a class or struct in C or C++, it says "I am made out of, 3 UBOs, a texture sampler, etc". 
	It's analogous to going:

	struct MyDesc {
		Buffer MyBuffer[3];
		Texture MyTex;
	}

	struct MyOtherDesc {
	  Buffer MyBuffer;
	}

	DescriptorSet - An actual instance of a descriptor, as defined by a DescriptorSetLayout. 
	Using the class/struct analogy, it's like going MyDesc descInstance();

	PipelineLayout - If you treat your entire shader as if it was just one big 
	void shader(arguments) function then a PipelineLayout is like describing all the "arguments" 
	passed into your shader such as void shader(MyDesc desc, MyOtherDesc otherDesc). 
	This generally maps up to statements like 
	layout(std140,set=0, binding = 0) uniform UBufferInfo{Blah MyBlah;} and 
	layout(set=0, binding = 2, rgba32f) uniform image2D MyImage; 
	in your shader code.

	vkCmdBindDescriptorSet - This is the mechanism to actually pass a DescriptorSet into a shader(aka pipeline). 
	So basically passing the "arguments" like shader(DescInstance,OtherDescInstance).

	*/
	class DescriptorPool {
		VkDescriptorPool descriptor_pool_; //The descriptor pool
		VkPipelineLayout pipeline_layout_; //A pipeline layout contains a list of descriptor set layouts
		std::vector<DescriptorSet> descriptor_sets_; //Descriptor sets allocated from this pool
	public:

		//Creates a command pool for commands that will be submitted to the queue family the "queue" parameter belongs to
		void CreateDescriptorPool(LogicalDevice& logical_device);
		VkDescriptorPool GetDescriptorPool();
	};
}
#pragma once

namespace Engine::Core::Renderer::VulkanEntities {

	class DescriptorSet {
		VkBuffer uniform_buffer_; //This is the buffer that contains the uniform_buffer_data_ struct
		VkDeviceMemory uniform_buffer_memory_; //Provides memory allocation info to vulkan when creating the uniform buffer
		VkDescriptorSetLayout descriptor_set_layout_; //This is used to describe the layout of a descriptor set so that Vulkan can tell the GPU how to read the data inside this descriptor set
	public:
		void CreateDescriptorSet(LogicalDevice& logical_device);
	};
}
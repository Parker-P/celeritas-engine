#pragma once

namespace Engine::Core::VulkanEntities {
	//A descriptor pool maintains a pool of descriptors, from which descriptor sets are allocated. Descriptor pools are externally synchronized, meaning that the application must not allocate and /or free descriptor sets from the same pool in multiple threads simultaneously.
	//Descriptor sets are the main way of connecting CPU data to the GPU
	class DescriptorPool {
		VkDescriptorPool descriptor_pool_;
	public:

		//Creates a command pool for commands that will be submitted to the queue family the "queue" parameter belongs to
		void CreateDescriptorPool(LogicalDevice& logical_device);
		VkCommandPool GetDescriptorPool();
	};
}
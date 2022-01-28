#pragma once

namespace Engine::Core::VulkanEntities {
	//A queue in vulkan is an entity used to organize how vulkan commands are executed. Vulkan command buffers are
	//submitted to these queues and then vulkan tells the drivers to execute them in order. 
	//To categorize the queues vulkan uses the concept of queue families. Each command in vulkan has a flag that says
	//which queue families it can be submitted to. If 2 commands share a flag and are put on separate queues, those 2 queues will share the same queue family.
	//Queue families have identifiers. The queue families available in vulkan are:
	//VK_QUEUE_GRAPHICS_BIT = 0x00000001,
	//VK_QUEUE_COMPUTE_BIT = 0x00000002,
	//VK_QUEUE_TRANSFER_BIT = 0x00000004,
	//VK_QUEUE_SPARSE_BINDING_BIT = 0x00000008,
	//VK_QUEUE_PROTECTED_BIT = 0x00000010
	class Queue {
		uint32_t queue_family_; //The family this queue belongs to
		VkQueue queue_; //The actual queue object
	public:

		//Set queue family
		void SetQueueFamily(uint32_t queue_family_index);

		//Set queue
		void SetQueue(VkQueue queue);

		//Returns the queue family this queue belongs to
		uint32_t GetQueueFamily();

		//Returns the actual queue object from vulkan
		VkQueue& GetQueue();
	};
}
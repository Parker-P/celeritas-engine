#pragma once

namespace Engine::Core::VulkanEntities {
	//A queue in vulkan is an entity used to organize how vulkan commands are executed. Vulkan command buffers are
	//submitted to these queues and then vulkan tells the drivers to execute them in order. 
	//To categorize the queues vulkan uses the concept of queue families. Different types of queues could belong to 
	//the same queue family as long as they serve a similar purpose.
	class Queue {
		uint32_t queue_family_; //The family this queue belongs to
		VkQueue queue_; //The actual queue object
	public:

		//Creates the queue
		void CreateQueue();

		//Returns the queue family this queue belongs to
		uint32_t GetQueueFamily();

		//Returns the actual queue object from vulkan
		uint32_t GetQueue();
	};
}
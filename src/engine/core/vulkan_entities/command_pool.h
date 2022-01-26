#pragma once

namespace Engine::Core::VulkanEntities {
	class CommandPool {
		VkCommandPool command_pool_;
	public:

		//Creates a command pool for commands that will be submitted to the queue family the "queue" parameter belongs to
		void CreateCommandPool(LogicalDevice& logical_device, Queue& queue);
		VkCommandPool GetCommandPool();
	};
}



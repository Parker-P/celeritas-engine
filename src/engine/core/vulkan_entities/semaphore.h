#pragma once

namespace Engine::Core::VulkanEntities {
	class Semaphore {
		VkSemaphore semaphore_;
	public:
		void CreateSemaphore(LogicalDevice& logical_device);
		VkSemaphore GetSemaphore();
	};
}
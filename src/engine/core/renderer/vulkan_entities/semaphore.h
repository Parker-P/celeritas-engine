#pragma once

namespace Engine::Core::VulkanEntities {

	//A semaphore is used to synchronize different commands on different queues. This semaphore in particular is used to make sure a command to display the image to the window isn't executed before the image has actually finished rendering
	class Semaphore {
		VkSemaphore semaphore_;
	public:
		void CreateSemaphore(LogicalDevice& logical_device);
		VkSemaphore GetSemaphore();
	};
}
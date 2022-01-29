#include <vulkan/vulkan.h>
#include "queue.h"

namespace Engine::Core::Renderer::VulkanEntities {
	void Queue::SetQueueFamily(uint32_t queue_family_index) {
		queue_family_ = queue_family_index;
	}

	void Queue::SetQueue(VkQueue queue) {
		queue_ = queue;
	}

	uint32_t Queue::GetQueueFamily() {
		return queue_family_;
	}

	VkQueue& Queue::GetQueue() {
		return queue_;
	}
}
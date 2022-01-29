#include <iostream>
#include <vulkan/vulkan.h>

#include "../src/engine/core/app_config.h"
#include "physical_device.h"
#include "logical_device.h"
#include "semaphore.h"

namespace Engine::Core::Renderer::VulkanEntities {
	void Semaphore::CreateSemaphore(LogicalDevice& logical_device)
	{
		VkSemaphoreCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(logical_device.GetLogicalDevice(), &create_info, nullptr, &semaphore_) != VK_SUCCESS) {
			std::cerr << "failed to create semaphores" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created semaphores" << std::endl;
		}
	}

	VkSemaphore Semaphore::GetSemaphore() {
		return semaphore_;
	}
}



#include <iostream>
#include <vulkan/vulkan.h>

#include "queue.h"
#include "physical_device.h"
#include "logical_device.h"
#include "command_pool.h"

namespace Engine::Core::VulkanEntities {

	void CommandPool::CreateCommandPool(LogicalDevice& logical_device, Queue& queue) {
		//Create graphics command pool for the graphics queue family since we want to send commands on that queue
		VkCommandPoolCreateInfo pool_create_info = {};
		pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_create_info.queueFamilyIndex = queue.GetQueueFamily();
		if (vkCreateCommandPool(logical_device.GetLogicalDevice(), &pool_create_info, nullptr, &command_pool_) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	VkCommandPool CommandPool::GetCommandPool() {
		return command_pool_;
	}
}



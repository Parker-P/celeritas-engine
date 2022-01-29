#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "descriptor_set.h"
#include "logical_device.h"
#include "descriptor_pool.h"

namespace Engine::Core::Renderer::VulkanEntities {
	void DescriptorPool::CreateDescriptorPool(LogicalDevice& logical_device) {

		//This describes how many descriptor sets we'll create from this pool for each type
		VkDescriptorPoolSize type_count;
		type_count.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		type_count.descriptorCount = 1;

		//Configure the pool creation
		VkDescriptorPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.poolSizeCount = 1;
		create_info.pPoolSizes = &type_count;
		create_info.maxSets = 1;

		//Create the pool with the specified config
		if (vkCreateDescriptorPool(logical_device.GetLogicalDevice(), &create_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "descriptor pool created" << std::endl;
		}
	}

	VkDescriptorPool VulkanEntities::DescriptorPool::GetDescriptorPool() {
		return descriptor_pool_;
	}
}
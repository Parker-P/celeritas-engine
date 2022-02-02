#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "logical_device.h"
#include "descriptor_set.h"
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

	void DescriptorPool::AllocateDescriptorSet(LogicalDevice& logical_device, VkDescriptorSetLayout descriptor_set_layout) {
		//There needs to be one descriptor set per binding point in the shader
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool_;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &descriptor_set_layout;

		//Create a slot where the descriptor set will be allocated
		descriptor_sets_.push_back(DescriptorSet());
		VkDescriptorSet descriptor_set = descriptor_sets_[descriptor_sets_.size() - 1].GetDescriptorSet();

		//Create the descriptor set
		if (vkAllocateDescriptorSets(logical_device.GetLogicalDevice(), &alloc_info, &descriptor_set) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set" << std::endl;

			//Set the descriptor set
			descriptor_sets_[descriptor_sets_.size() - 1].SetDescriptorSet(descriptor_set);
		}
	}

	VkDescriptorPool VulkanEntities::DescriptorPool::GetDescriptorPool() {
		return descriptor_pool_;
	}
}
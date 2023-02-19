#include <vector>
#include <string>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_vec.hpp>
#include <vulkan/vulkan.h>

#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/structural/Pipelineable.hpp"
#include "engine/vulkan/ShaderResources.hpp"

namespace Engine::Vulkan
{
	Descriptor::Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, Buffer* pBuffer, Image* pImage)
	{
		_bufferInfo = pBuffer == nullptr ? VkDescriptorBufferInfo{} : pBuffer->GenerateDescriptor();
		_imageInfo = pImage == nullptr ? VkDescriptorImageInfo{} : pImage->GenerateDescriptor();
		_type = type;
		_bindingNumber = bindingNumber;
	}

	void DescriptorSet::SendDescriptorData()
	{
		std::vector<VkWriteDescriptorSet> writeInfos;

		bool canUpdate = true;

		for (auto& descriptor : _descriptors) {
			if (descriptor->_bufferInfo.range != 0 || descriptor->_imageInfo.sampler != VK_NULL_HANDLE) {
				VkWriteDescriptorSet writeInfo = {};
				writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeInfo.dstSet = _handle;
				writeInfo.descriptorCount = 1;
				writeInfo.descriptorType = descriptor->_type;
				writeInfo.pBufferInfo = &descriptor->_bufferInfo.range == 0 ? writeInfo.pBufferInfo : &descriptor->_bufferInfo; // Needs stronger check on type.
				writeInfo.pImageInfo = &descriptor->_imageInfo.sampler == VK_NULL_HANDLE ? writeInfo.pImageInfo : &descriptor->_imageInfo; // This one too.
				writeInfo.dstBinding = descriptor->_bindingNumber;
				writeInfos.push_back(writeInfo);
			}
			else {
				canUpdate = false;
			}
		}

		if (canUpdate) {
			vkUpdateDescriptorSets(_logicalDevice, (uint32_t)writeInfos.size(), writeInfos.data(), 0, nullptr);
		}
	}

	DescriptorSet::DescriptorSet(VkDevice& logicalDevice, const VkShaderStageFlagBits& shaderStageFlags, std::vector<Descriptor*> descriptors)
	{
		_logicalDevice = logicalDevice;
		_indexNumber = -1;
		_descriptors = descriptors;
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		// Prep work for the descriptor set layout.
		for (auto& descriptor : descriptors) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = descriptor->_bindingNumber;
			binding.descriptorType = descriptor->_type;
			binding.descriptorCount = 1;
			binding.stageFlags = shaderStageFlags;
			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)bindings.size();
		descriptorSetLayoutCreateInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &_layout) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set layout" << std::endl;
		}
	}

	void DescriptorPool::AllocateDescriptorSets()
	{
		// The amount of sets and descriptor types is defined when creating the descriptor pool.
		std::vector<VkDescriptorSetLayout> layouts;
		for (auto& descriptorSet : _descriptorSets) {
			layouts.push_back(descriptorSet->_layout);
		}

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _handle;
		allocInfo.descriptorSetCount = (uint32_t)_descriptorSets.size();
		allocInfo.pSetLayouts = layouts.data();

		std::vector<VkDescriptorSet> allocatedDescriptorSetHandles(_descriptorSets.size());
		if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, allocatedDescriptorSetHandles.data()) != VK_SUCCESS) {
			std::cerr << "failed to allocate descriptor sets" << std::endl;
			exit(1);
		}
		else {
			std::cout << "allocated descriptor sets" << std::endl;

			for (int i = 0; i < allocatedDescriptorSetHandles.size(); ++i) {
				_descriptorSets[i]->_handle = allocatedDescriptorSetHandles[i];
				_descriptorSets[i]->SendDescriptorData();
			}
		}
	}

	DescriptorPool::DescriptorPool(VkDevice& logicalDevice, std::vector<DescriptorSet*> descriptorSets)
	{
		_descriptorSets = descriptorSets;
		_logicalDevice = logicalDevice;

		std::vector<VkDescriptorPoolSize> poolSizes;
		for (auto& descriptorSet : descriptorSets) {
			if (descriptorSet->_descriptors.size() != 0) {
				if (descriptorSet->_descriptors[0]->_type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
					VkDescriptorPoolSize poolSize{};
					poolSize.type = descriptorSet->_descriptors[0]->_type;
					poolSize.descriptorCount = (uint32_t)descriptorSet->_descriptors.size();
					poolSizes.push_back(poolSize);
				}
			}
		}

		// maxSets is the maximum number of descriptor sets that can be allocated from the pool.
		// poolSizeCount is the number of elements in pPoolSizes.
		// pPoolSizes is a pointer to an array of VkDescriptorPoolSize structures, each containing a descriptor type and number of descriptors of that type to be allocated in the pool.
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = (uint32_t)descriptorSets.size();
		createInfo.poolSizeCount = (uint32_t)poolSizes.size();
		createInfo.pPoolSizes = poolSizes.data();
		if (vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &_handle) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor pool" << std::endl;
		}

		AllocateDescriptorSets();
	}

	void DescriptorPool::UpdateDescriptor(Descriptor& d, Buffer& data)
	{
		for (auto& descriptorSet : _descriptorSets) {
			for (auto& descriptor : descriptorSet->_descriptors) {
				if (descriptor == &d) {
					descriptor->_bufferInfo = data.GenerateDescriptor();
				}
			}
		}
	}

	void DescriptorPool::UpdateDescriptor(Descriptor& d, Image& data)
	{
		for (auto& descriptorSet : _descriptorSets) {
			for (auto& descriptor : descriptorSet->_descriptors) {
				if (descriptor == &d) {
					descriptor->_imageInfo = data.GenerateDescriptor();
					descriptorSet->SendDescriptorData();
				}
			}
		}
	}
}
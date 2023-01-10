// STL.
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Buffer.hpp"

namespace Engine::Vulkan
{
	Buffer::Buffer(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, VkBufferUsageFlagBits usageFlags, VkMemoryPropertyFlagBits properties, void* data, size_t sizeInBytes)
	{
		_logicalDevice = logicalDevice;
		_physicalDevice = physicalDevice;
		_properties = properties;

		// Create the buffer at the logical level.
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = sizeInBytes;
		vertexBufferInfo.usage = usageFlags;
		if (vkCreateBuffer(_logicalDevice, &vertexBufferInfo, nullptr, &_handle) != VK_SUCCESS) {
			std::cout << ("Failed creating buffer.") << std::endl;
			exit(1);
		}

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(_logicalDevice, _handle, &requirements);

		VkMemoryAllocateInfo allocationInfo = {};
		allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocationInfo.allocationSize = requirements.size;

		if (auto index = _physicalDevice.GetMemoryTypeIndex(requirements.memoryTypeBits, properties); index == -1) {
			std::cout << "Could not get memory type index" << std::endl;
			exit(1);
		}
		else {
			_properties = properties;
			allocationInfo.memoryTypeIndex = index;
		}

		if (vkAllocateMemory(_logicalDevice, &allocationInfo, nullptr, &_memory) != VK_SUCCESS) {
			std::cout << "failed to allocate buffer memory" << std::endl;
			exit(1);
		}

		// Creates a reference/connection to the buffer on the GPU side.
		vkBindBufferMemory(_logicalDevice, _handle, _memory, 0);

		// Creates a reference/connection to the buffer on the CPU side.
		if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			vkMapMemory(_logicalDevice, _memory, 0, sizeInBytes, 0, &_dataAddress);
		}

		if (data != nullptr && _dataAddress != nullptr) {
			memcpy(_dataAddress, data, sizeInBytes);
			_size = sizeInBytes;
		}
	}

	VkDescriptorBufferInfo Buffer::GenerateDescriptor()
	{
		return VkDescriptorBufferInfo{ _handle, 0, _size };
	}

	void Buffer::UpdateData(void* data, size_t sizeInBytes)
	{
		if (_dataAddress != nullptr) {
			if (data != nullptr) {
				memcpy(_dataAddress, data, sizeInBytes);
				_size = sizeInBytes;
			}
		}
	}

	void Buffer::Destroy()
	{
		// If the memory was allocated on RAM, we need to break the binding between GPU and RAM by unmapping the memory first.
		if (_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			vkUnmapMemory(_logicalDevice, _memory);
		}

		vkDestroyBuffer(_logicalDevice, _handle, nullptr);
		vkFreeMemory(_logicalDevice, _memory, nullptr);
	}
}
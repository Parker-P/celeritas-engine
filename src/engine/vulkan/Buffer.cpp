// STL.
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Buffer.hpp"

namespace Engine::Vulkan
{
	Buffer::Buffer(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, VkBufferUsageFlagBits usageFlags, VkMemoryPropertyFlagBits propertiesFlags, void* pData, size_t sizeInBytes, const VkFormat& dataFormat)
	{
		_logicalDevice = logicalDevice;
		_physicalDevice = physicalDevice;
		_pData = pData;
		_bufferDataSize = sizeInBytes;
		_usageFlags = usageFlags;
		_propertiesFlags = propertiesFlags;
		_pBufferData = nullptr;

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

		if (auto index = _physicalDevice.GetMemoryTypeIndex(requirements.memoryTypeBits, propertiesFlags); index == -1) {
			std::cout << "Could not get memory type index" << std::endl;
			exit(1);
		}
		else {
			_propertiesFlags = propertiesFlags;
			allocationInfo.memoryTypeIndex = index;
		}

		if (vkAllocateMemory(_logicalDevice, &allocationInfo, nullptr, &_memory) != VK_SUCCESS) {
			std::cout << "failed to allocate buffer memory" << std::endl;
			exit(1);
		}

		// Creates a reference/connection to the buffer on the GPU side.
		if (vkBindBufferMemory(_logicalDevice, _handle, _memory, 0) != VK_SUCCESS) {
			std::cout << "failed binding buffer memory to the GPU" << std::endl;
			exit(1);
		}

		// Creates a reference/connection to the buffer on the CPU side.
		if (propertiesFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			vkMapMemory(_logicalDevice, _memory, 0, sizeInBytes, 0, &_pBufferData);
		}

		if (pData != nullptr && _pBufferData != nullptr) {
			memcpy(_pBufferData, pData, sizeInBytes);
			_bufferDataSize = sizeInBytes;
		}

		// Describes how the shaders should interpret the data in the buffer.
		if (dataFormat != VK_FORMAT_MAX_ENUM) {
			VkBufferViewCreateInfo bufferViewCreateInfo{};
			bufferViewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
			bufferViewCreateInfo.buffer = _handle;
			bufferViewCreateInfo.format = dataFormat;
			bufferViewCreateInfo.range = VK_WHOLE_SIZE;

			if (auto res = vkCreateBufferView(logicalDevice, &bufferViewCreateInfo, nullptr, &_viewHandle); res != VK_SUCCESS) {
				std::cout << "failed creating buffer view" << std::endl;
			}
		}
	}

	VkDescriptorBufferInfo Buffer::GenerateDescriptor()
	{
		return VkDescriptorBufferInfo{ _handle, 0, _bufferDataSize };
	}

	void Buffer::UpdateData(void* data, size_t sizeInBytes)
	{
		if (_pBufferData != nullptr) {
			if (data != nullptr) {
				memcpy(_pBufferData, data, sizeInBytes);
				_bufferDataSize = sizeInBytes;
			}
		}
	}

	void Buffer::SendToGPU(VkCommandPool& commandPool, Queue& queue)
	{
		// If the buffer is not marked as a destination of a transfer-to-gpu operation and as gpu-only buffer, then you cannot
		// copy data to it.
		if (!(_usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT && _propertiesFlags | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
			return;
		}

		// Create a buffer used as a middleman buffer to transfer data from the RAM to the VRAM. This buffer will be created in RAM.
		auto transferBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			_pData,
			_bufferDataSize);

		VkCommandBufferBeginInfo bufferBeginInfo = {};
		bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		vkAllocateCommandBuffers(_logicalDevice, &cmdBufInfo, &copyCommandBuffer);

		vkBeginCommandBuffer(copyCommandBuffer, &bufferBeginInfo);
		VkBufferCopy copyRegion = {};
		copyRegion.size = _bufferDataSize;
		vkCmdCopyBuffer(copyCommandBuffer, transferBuffer._handle, _handle, 1, &copyRegion);
		vkEndCommandBuffer(copyCommandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;

		vkQueueSubmit(queue._handle, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue._handle);

		vkFreeCommandBuffers(_logicalDevice, commandPool, 1, &copyCommandBuffer);

		transferBuffer.Destroy();
	}

	void Buffer::Destroy()
	{
		// If the memory was allocated on RAM, we need to break the binding between GPU and RAM by unmapping the memory first.
		if (_propertiesFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			vkUnmapMemory(_logicalDevice, _memory);
		}

		vkDestroyBuffer(_logicalDevice, _handle, nullptr);
		vkFreeMemory(_logicalDevice, _memory, nullptr);
	}
}
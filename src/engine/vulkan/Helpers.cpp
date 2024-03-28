#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <filesystem>
#include <map>
#include <bitset>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tinygltf/tiny_gltf.h>
#include "LocalIncludes.hpp"
#include "Helpers.hpp"

namespace Engine::Vulkan
{
	void CheckResult(VkResult result)
	{
		if (result != VK_SUCCESS) {
			std::string message = "ERROR: code " + std::to_string(result);
			Utils::Exit(result, message.c_str());
		}
	}

	Image SolidColorImage(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		Image image;

		image._sizeBytes = 4;
		image._pData = malloc(image._sizeBytes);
		unsigned char imageData[4] = { r, g, b, a };
		memcpy(image._pData, imageData, image._sizeBytes);

		auto& imageCreateInfo = image._createInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.extent = { 1, 1, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &image._image);

		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(logicalDevice, image._image, &reqs);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkDeviceMemory mem;
		vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem);
		vkBindImageMemory(logicalDevice, image._image, mem, 0);

		auto& imageViewCreateInfo = image._viewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.image = image._image;
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &image._view);

		auto& samplerCreateInfo = image._samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
		vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &image._sampler);

		return image;
	}

	void CopyBufferToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer buffer, void* pData, size_t sizeBytes)
	{
		// Create a temporary buffer.
		Buffer stagingBuffer{};
		stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBuffer._createInfo.size = sizeBytes;
		stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
		stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, sizeBytes, 0, &stagingBuffer._cpuMemory);
		memcpy(stagingBuffer._cpuMemory, pData, sizeBytes);

		auto copyCommandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		StartRecording(copyCommandBuffer);

		VkBufferCopy copyRegion = {};
		copyRegion.size = sizeBytes;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer._buffer, buffer, 1, &copyRegion);

		StopRecording(copyCommandBuffer);
		ExecuteCommands(copyCommandBuffer, queue);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);
		vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
	}

	void CopyImageToDeviceMemory(VkDevice& logicalDevice, VkPhysicalDevice& physicalDevice, VkCommandPool& commandPool, VkQueue& queue, VkImage& image, int width, int height, int depth, void* pData, size_t sizeBytes)
	{
		// Create a temporary buffer.
		Buffer stagingBuffer{};
		stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBuffer._createInfo.size = sizeBytes;
		stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
		stagingBuffer._gpuMemory = PhysicalDevice::AllocateMemory(physicalDevice, logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, sizeBytes, 0, &stagingBuffer._cpuMemory);
		memcpy(stagingBuffer._cpuMemory, pData, sizeBytes);

		auto commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		StartRecording(commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_NONE;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy copyInfo{};
		copyInfo.bufferImageHeight = height;
		copyInfo.bufferRowLength = width;
		copyInfo.imageExtent = { (uint32_t)width, (uint32_t)height, (uint32_t)depth };
		copyInfo.imageOffset = { 0,0,0 };
		copyInfo.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

		StopRecording(commandBuffer);
		ExecuteCommands(commandBuffer, queue);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
	}

	VkCommandPool CreateCommandPool(VkDevice& logicalDevice, int& queueFamilyIndex)
	{
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = queueFamilyIndex;
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VkCommandPool outPool;
		vkCreateCommandPool(logicalDevice, &poolCreateInfo, nullptr, &outPool);
		return outPool;
	}

	int FindQueueFamilyIndex(VkPhysicalDevice& physicalDevice, VkQueueFlagBits queueFlags)
	{
		auto queueFamilyProperties = PhysicalDevice::GetAllQueueFamilyProperties(physicalDevice);

		for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); queueFamilyIndex++) {
			if (queueFamilyProperties[queueFamilyIndex].queueCount > 0 && queueFamilyProperties[queueFamilyIndex].queueFlags & queueFlags) {
				return queueFamilyIndex;
			}
		}

		return -1;
	}

	void StartRecording(VkCommandBuffer& commandBuffer)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(commandBuffer, 0);
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	void Engine::Vulkan::StopRecording(VkCommandBuffer& commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);
	}

	void ExecuteCommands(VkCommandBuffer& commandBuffer, VkQueue& queue)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(queue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(queue);
	}

	VkCommandBuffer CreateCommandBuffer(VkDevice& logicalDevice, VkCommandPool& commandPool)
	{
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &commandBuffer);
		return commandBuffer;
	}
}
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
	Image SolidColorImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		Image image;

		image._sizeBytes = 4;
		image._pData = malloc(image._sizeBytes);
		unsigned char imageData[4] = { r,g,b,a };
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
		allocInfo.memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

	void CopyBufferToDeviceMemory(VkDevice logicalDevice, PhysicalDevice& physicalDevice, VkCommandPool commandPool, Queue& queue, Buffer& buffer)
	{
		// Create a temporary buffer.
		Buffer stagingBuffer{};
		stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBuffer._createInfo.size = buffer._sizeBytes;
		stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
		stagingBuffer._gpuMemory = physicalDevice.AllocateMemory(logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, buffer._sizeBytes, 0, &stagingBuffer._cpuMemory);
		memcpy(stagingBuffer._cpuMemory, buffer._pData, buffer._sizeBytes);

		auto copyCommandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		StartRecording(copyCommandBuffer);

		VkBufferCopy copyRegion = {};
		copyRegion.size = buffer._sizeBytes;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffer._buffer, buffer._buffer, 1, &copyRegion);

		StopRecording(copyCommandBuffer);
		ExecuteCommands(copyCommandBuffer, queue._handle);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);
		vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
	}

	void CopyImageToDeviceMemory(VkDevice logicalDevice, PhysicalDevice& physicalDevice, VkCommandPool commandPool, Queue& queue, Image& image, int width, int height, int depth)
	{
		// Create a temporary buffer.
		Buffer stagingBuffer{};
		stagingBuffer._createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBuffer._createInfo.size = image._sizeBytes;
		stagingBuffer._createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		vkCreateBuffer(logicalDevice, &stagingBuffer._createInfo, nullptr, &stagingBuffer._buffer);

		// Allocate memory for the buffer.
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffer._buffer, &requirements);
		stagingBuffer._gpuMemory = physicalDevice.AllocateMemory(logicalDevice, requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Map memory to the correct GPU and CPU ranges for the buffer.
		vkBindBufferMemory(logicalDevice, stagingBuffer._buffer, stagingBuffer._gpuMemory, 0);
		vkMapMemory(logicalDevice, stagingBuffer._gpuMemory, 0, image._sizeBytes, 0, &stagingBuffer._cpuMemory);
		memcpy(stagingBuffer._cpuMemory, image._pData, image._sizeBytes);

		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		auto commandBuffer = CreateCommandBuffer(logicalDevice, commandPool);
		StartRecording(commandBuffer);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_NONE;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = image._currentLayout;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = image._image;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		image._currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Copy the buffer to the specific face by defining the subresource range.
		VkBufferImageCopy copyInfo{};
		copyInfo.bufferImageHeight = height;
		copyInfo.bufferRowLength = width;
		copyInfo.imageExtent = { (uint32_t)width, (uint32_t)height, (uint32_t)depth };
		copyInfo.imageOffset = { 0,0,0 };
		copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyInfo.imageSubresource.layerCount = 1;
		copyInfo.imageSubresource.baseArrayLayer = 0;
		copyInfo.imageSubresource.mipLevel = 0;
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer._buffer, image._image, image._currentLayout, 1, &copyInfo);

		StopRecording(commandBuffer);
		ExecuteCommands(commandBuffer, queue._handle);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
		vkDestroyBuffer(logicalDevice, stagingBuffer._buffer, nullptr);
	}

	void StartRecording(VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(commandBuffer, 0);
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	void Engine::Vulkan::StopRecording(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);
	}

	void ExecuteCommands(VkCommandBuffer commandBuffer, VkQueue queue)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(queue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(queue);
	}

	VkCommandBuffer CreateCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool)
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
// STL.
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/Image.hpp"

namespace Engine::Vulkan
{
	size_t Image::GetPixelSizeBytes(const VkFormat& format)
	{
		if (format == VK_FORMAT_R8G8B8A8_SRGB ||
			format == VK_FORMAT_D32_SFLOAT) {
			return 4;
		}
		else if (format == VK_FORMAT_R8G8B8_SRGB) {
			return 3;
		}

		return 0;
	}

	Image::Image(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const VkFormat& imageFormat, const VkExtent2D& sizePixels, void* data, const VkImageUsageFlagBits& usageFlags, const VkImageAspectFlagBits& typeFlags, const VkMemoryPropertyFlagBits& memoryPropertiesFlags)
	{
		_physicalDevice = physicalDevice;
		_logicalDevice = logicalDevice;
		_format = imageFormat;
		_sizePixels = sizePixels;
		_typeFlags = typeFlags;
		_sizeBytes = GetPixelSizeBytes(imageFormat) * sizePixels.width * sizePixels.height;
		_pData = data;

		VkImageCreateInfo imageCreateInfo = { };
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = imageFormat;

		auto imageSize = VkExtent3D{ sizePixels.width, sizePixels.height, 1 };
		imageCreateInfo.extent = imageSize;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		// Tiling is very important. Tiling describes how the data for the texture is arranged in the GPU. 
		// For improved performance, GPUs do not store images as 2d arrays of pixels, but instead use complex
		// custom formats, unique to the GPU brand and even models. VK_IMAGE_TILING_OPTIMAL tells Vulkan 
		// to let the driver decide how the GPU arranges the memory of the image. If you use VK_IMAGE_TILING_OPTIMAL,
		// it won’t be possible to read the data from CPU or to write it without changing its tiling first 
		// (it’s possible to change the tiling of a texture at any point, but this can be a costly operation). 
		// The other tiling we can care about is VK_IMAGE_TILING_LINEAR, which will store the image as a 2d 
		// array of pixels. While LINEAR tiling will be a lot slower, it will allow the cpu to safely write 
		// and read from that memory.
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = usageFlags;

		if (vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &_imageHandle) != VK_SUCCESS) {
			std::cout << "failed creating image" << std::endl;
		}

		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(logicalDevice, _imageHandle, &reqs);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(reqs.memoryTypeBits, memoryPropertiesFlags);

		VkDeviceMemory mem;
		vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &mem);
		vkBindImageMemory(logicalDevice, _imageHandle, mem, 0);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.image = _imageHandle;
		imageViewCreateInfo.format = imageFormat;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.aspectMask = typeFlags;

		if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &_imageViewHandle) != VK_SUCCESS) {
			std::cout << "failed creating image view" << std::endl;
		}
	}

	Image::Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat)
	{
		_imageHandle = image;
		_logicalDevice = logicalDevice;
		_format = imageFormat;
		_sizePixels = VkExtent2D{ 0, 0 };

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = imageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &_imageViewHandle) != VK_SUCCESS) {
			std::cerr << "failed to create image view." << std::endl;
			exit(1);
		}
	}

	void Image::SendToGPU(VkCommandPool& commandPool, Queue& queue)
	{
		// Allocate a temporary buffer for holding image data to upload to VRAM.
		Buffer stagingBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			_pData,
			_sizeBytes);

		// Transfer the data from the buffer into the allocated image using a command buffer.
		// Here we record the commands for copying data from the buffer to the image.
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		if (vkAllocateCommandBuffers(_logicalDevice, &cmdBufInfo, &copyCommandBuffer) != VK_SUCCESS) {
			std::cout << "Failed allocating copy command buffer to copy buffer to texture." << std::endl;
			exit(1);
		}
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);

		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image = _imageHandle;
		imageBarrier_toTransfer.subresourceRange = range;
		imageBarrier_toTransfer.srcAccessMask = 0;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// Barrier the image into the transfer-receive layout.
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = VkExtent3D{ _sizePixels.width, _sizePixels.height, 1 };
		//copyRegion.imageExtent = VkExtent3D{ texture._size.width, texture._size.height, 1 };

		// Copy the buffer into the image.
		vkCmdCopyBufferToImage(copyCommandBuffer, stagingBuffer._handle, _imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Barrier the image into the shader readable layout.
		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;
		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

		vkEndCommandBuffer(copyCommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;
		vkQueueSubmit(queue._handle, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(queue._handle);

		vkFreeCommandBuffers(_logicalDevice, commandPool, 1, &copyCommandBuffer);
		stagingBuffer.Destroy();
	}

	VkDescriptorImageInfo Image::GenerateDescriptor() {
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.pNext = nullptr;
		info.magFilter = VK_FILTER_NEAREST;
		info.minFilter = VK_FILTER_NEAREST;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		vkCreateSampler(_logicalDevice, &info, nullptr, &_sampler);

		return VkDescriptorImageInfo{ _sampler, _imageViewHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	void Image::Destroy()
	{
		vkDestroyImageView(_logicalDevice, _imageViewHandle, nullptr);
		vkDestroyImage(_logicalDevice, _imageHandle, nullptr);
	}
}
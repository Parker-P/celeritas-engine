// STL.
#include <iostream>
#include <vector>
#include <optional>

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

        if (format == VK_FORMAT_R8G8B8_SRGB ||
            format == VK_FORMAT_R8G8B8_UINT) {
            return 3;
        }

        if (format == VK_FORMAT_R32G32B32_SFLOAT) {
            return 12;
        }

        return 0;
    }

    Image Image::SolidColor(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const unsigned char& red, const unsigned char& green, const unsigned char& blue, const unsigned char& alpha)
    {
        unsigned int pixelColor = red << 24 | green << 16 | blue << 8 | alpha;
        return Image(logicalDevice, physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, VkExtent3D{ 1, 1, 1 }, &pixelColor, (VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT), VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    Image::Image(VkDevice& logicalDevice,
        PhysicalDevice& physicalDevice,
        const VkFormat& imageFormat,
        const VkExtent3D& sizePixels,
        void* data,
        const VkImageUsageFlagBits& usageFlags,
        const VkImageAspectFlagBits& typeFlags,
        const VkMemoryPropertyFlagBits& memoryPropertiesFlags,
        const uint32_t& arrayLayerCount,
        const VkImageCreateFlags& creationFlags,
        const VkImageViewType& imageViewType)
    {
        _physicalDevice = physicalDevice;
        _logicalDevice = logicalDevice;
        _format = imageFormat;
        _sizePixels = sizePixels;
        _typeFlags = typeFlags;
        _sizeBytes = GetPixelSizeBytes(imageFormat) * sizePixels.width * sizePixels.height;
        _pData = data;

        // Tiling is very important. Tiling describes how the data for the texture is arranged in the GPU. 
        // For improved performance, GPUs do not store images as 2d arrays of pixels, but instead use complex
        // custom formats, unique to the GPU brand and even models. VK_IMAGE_TILING_OPTIMAL tells Vulkan 
        // to let the driver decide how the GPU arranges the memory of the image. If you use VK_IMAGE_TILING_OPTIMAL,
        // it won’t be possible to read the data from CPU or to write it without changing its tiling first 
        // (it’s possible to change the tiling of a texture at any point, but this can be a costly operation). 
        // The other tiling we can care about is VK_IMAGE_TILING_LINEAR, which will store the image as a 2d 
        // array of pixels. While LINEAR tiling will be a lot slower, it will allow the CPU to safely write 
        // and read from that memory.
        auto imageSize = VkExtent3D{ sizePixels.width, sizePixels.height, 1 };
        VkImageCreateInfo imageCreateInfo = { };
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.extent = imageSize;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = arrayLayerCount;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = usageFlags;
        imageCreateInfo.flags = creationFlags;

        VkImageFormatProperties fp{};
        auto result = vkGetPhysicalDeviceImageFormatProperties(physicalDevice._handle, _format, imageCreateInfo.imageType, VK_IMAGE_TILING_LINEAR, usageFlags, VK_SAMPLE_COUNT_1_BIT, &fp);

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

        _subresourceRange.baseMipLevel = 0;
        _subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        _subresourceRange.baseArrayLayer = 0;
        _subresourceRange.layerCount = arrayLayerCount;
        _subresourceRange.aspectMask = typeFlags;
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType = imageViewType;
        imageViewCreateInfo.image = _imageHandle;
        imageViewCreateInfo.format = imageFormat;
        imageViewCreateInfo.subresourceRange = _subresourceRange;

        if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &_viewHandle) != VK_SUCCESS) {
            std::cout << "failed creating image view" << std::endl;
        }
    }

    Image::Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat)
    {
        _imageHandle = image;
        _logicalDevice = logicalDevice;
        _format = imageFormat;
        _sizePixels = VkExtent3D{ 0, 0, 0 };

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

        if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &_viewHandle) != VK_SUCCESS) {
            std::cerr << "failed to create image view." << std::endl;
            exit(1);
        }
    }

    void Image::SendToGPU(VkCommandPool& commandPool, Queue& queue, const std::vector<VkBufferImageCopy>& bufferCopyRegions)
    {
        // Check if the image can be sent to the GPU first.
        if ((_memoryPropertiesFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == false) {
            std::cout << "this image cannot be sent to the GPU as it doesn't have the VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT in its memory properties" << std::endl;
            return;
        }

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

        // Change the layout of the image so that it's fastest for memory transfers.
        // GPUs use specific memory layouts (the way data is arranged and stored in memory, for example an image could be stored one row of pixels
        // after another row of pixels and so on, or column after column). By using specific layouts, certain operations are significantly faster.
        VkImageMemoryBarrier imageBarrierToTransfer = {};
        imageBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrierToTransfer.image = _imageHandle;
        imageBarrierToTransfer.subresourceRange = _subresourceRange;
        imageBarrierToTransfer.srcAccessMask = 0;
        imageBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierToTransfer);

        // Copy the temporary buffer into the image.
        if (bufferCopyRegions.size() == 1) { // Handles the default case.
            if (bufferCopyRegions[0].imageExtent.width == 0 || bufferCopyRegions[0].imageExtent.height == 0) {
                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0;
                copyRegion.bufferImageHeight = 0;
                copyRegion.imageSubresource.aspectMask = _typeFlags;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageExtent = _sizePixels;
                vkCmdCopyBufferToImage(copyCommandBuffer,
                    stagingBuffer._handle,
                    _imageHandle,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);
            }
        }
        vkCmdCopyBufferToImage(copyCommandBuffer,
            stagingBuffer._handle,
            _imageHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            bufferCopyRegions.size(),
            bufferCopyRegions.data());

        // Change the layout of the image so that it's fastest for sampling in shaders.
        VkImageMemoryBarrier imageBarrierTransferToShaderRead = imageBarrierToTransfer;
        imageBarrierTransferToShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrierTransferToShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrierTransferToShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageBarrierTransferToShaderRead.image = _imageHandle;
        imageBarrierTransferToShaderRead.subresourceRange = _subresourceRange;
        imageBarrierTransferToShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrierTransferToShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrierTransferToShaderRead);

        // Stop recording copy commands and submit them for execution.
        vkEndCommandBuffer(copyCommandBuffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCommandBuffer;
        vkQueueSubmit(queue._handle, 1, &submitInfo, nullptr);
        vkQueueWaitIdle(queue._handle);

        // Destroy temporary resources.
        vkFreeCommandBuffers(_logicalDevice, commandPool, 1, &copyCommandBuffer);
        stagingBuffer.Destroy();
    }

    VkDescriptorImageInfo Image::GenerateDescriptor() {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.pNext = nullptr;
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        vkCreateSampler(_logicalDevice, &info, nullptr, &_sampler);

        return VkDescriptorImageInfo{ _sampler, _viewHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }

    void Image::Destroy()
    {
        vkDestroyImageView(_logicalDevice, _viewHandle, nullptr);
        vkDestroyImage(_logicalDevice, _imageHandle, nullptr);
    }
}
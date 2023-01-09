#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

// STL.
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <filesystem>
#include <map>
#include <bitset>

// Math.
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

// Scene loading.
#include <tinygltf/tiny_gltf.h>

// Project local classes.
#include "utils/Json.h"
#include "structural/IUpdatable.hpp"
#include "structural/IDrawable.hpp"
#include "structural/Singleton.hpp"
#include "settings/GlobalSettings.hpp"
#include "settings/Paths.hpp"
#include "engine/Time.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/Camera.hpp"
#include "utils/Utils.hpp"
#include "engine/math/Transform.hpp"
#include "engine/VulkanApplication.hpp"

namespace Engine::Vulkan
{
	bool windowResized = false;
	bool windowMinimized = false;

#pragma region BufferFunctions
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
#pragma endregion

#pragma region ImageFunctions
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
		_data = data;

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
			_data,
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
#pragma endregion

#pragma region PhysicalDeviceFunctions
	PhysicalDevice::PhysicalDevice(VkInstance& instance)
	{
		// Try to find 1 Vulkan supported device.
		// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions.
		uint32_t deviceCount = 0;
		if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
			std::cerr << "Failed to get number of physical devices." << std::endl;
			exit(1);
		}

		deviceCount = 1;
		VkResult res = vkEnumeratePhysicalDevices(instance, &deviceCount, &_handle);
		if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
			std::cerr << "Enumerating physical devices failed." << std::endl;
			exit(1);
		}

		if (deviceCount == 0) {
			std::cerr << "No physical devices that support vulkan." << std::endl;
			exit(1);
		}

		std::cout << "Physical device with vulkan support found." << std::endl;

		// Check device features
		// Note: will apiVersion >= appInfo.apiVersion? Probably yes, but spec is unclear.
		/*VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(_handle, &deviceProperties);
		vkGetPhysicalDeviceFeatures(_handle, &deviceFeatures);

		uint32_t supportedVersion[] = {
			VK_VERSION_MAJOR(deviceProperties.apiVersion),
			VK_VERSION_MINOR(deviceProperties.apiVersion),
			VK_VERSION_PATCH(deviceProperties.apiVersion)
		};

		std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;*/
	}

	bool PhysicalDevice::SupportsSwapchains()
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(_handle, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "physical device doesn't support any extensions" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(_handle, nullptr, &extensionCount, deviceExtensions.data());

		for (const auto& extension : deviceExtensions) {
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				std::cout << "physical device supports swap chains" << std::endl;
				return true;
			}
		}

		return false;
	}

	bool PhysicalDevice::SupportsSurface(uint32_t& queueFamilyIndex, VkSurfaceKHR& surface)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(_handle, queueFamilyIndex, surface, &presentSupport);
		return presentSupport == 0 ? false : true;
	}

	VkPhysicalDeviceMemoryProperties PhysicalDevice::GetMemoryProperties()
	{
		VkPhysicalDeviceMemoryProperties props;
		vkGetPhysicalDeviceMemoryProperties(_handle, &props);
		return props;
	}

	uint32_t PhysicalDevice::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlagBits properties)
	{
		auto props = GetMemoryProperties();

		for (uint32_t i = 0; i < 32; i++) {
			if ((typeBits & 1) == 1) {
				if ((props.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}
			typeBits >>= 1;
		}

		return -1;
	}

	std::vector<VkQueueFamilyProperties> PhysicalDevice::GetAllQueueFamilyProperties()
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(_handle, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(_handle, &queueFamilyCount, queueFamilies.data());

		return queueFamilies;
	}

	void PhysicalDevice::GetQueueFamilyIndices(const VkQueueFlagBits& queueFlags, bool needsPresentationSupport, const VkSurfaceKHR& surface)
	{
		// Check queue families
		auto queueFamilyProperties = GetAllQueueFamilyProperties();

		std::vector<uint32_t> queueFamilyIndices;

		for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
			VkBool32 presentationSupported = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(_handle, i, surface, &presentationSupported);

			if (queueFamilyProperties[i].queueCount > 0 && queueFamilyProperties[i].queueFlags & queueFlags) {
				if (needsPresentationSupport) {
					if (presentationSupported) {
						queueFamilyIndices.push_back(i);
					}
				}
				else {
					queueFamilyIndices.push_back(i);
				}
			}
		}
	}

	VkSurfaceCapabilitiesKHR PhysicalDevice::GetSurfaceCapabilities(VkSurfaceKHR& windowSurface) {
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_handle, windowSurface, &surfaceCapabilities) != VK_SUCCESS) {
			std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
		}
		return surfaceCapabilities;
	}

	std::vector<VkSurfaceFormatKHR> PhysicalDevice::GetSupportedFormatsForSurface(VkSurfaceKHR& windowSurface) {
		uint32_t formatCount;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(_handle, windowSurface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
			std::cerr << "failed to get number of supported surface formats" << std::endl;
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(_handle, windowSurface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported surface formats" << std::endl;
		}

		return surfaceFormats;
	}

	std::vector<VkPresentModeKHR> PhysicalDevice::GetSupportedPresentModesForSurface(VkSurfaceKHR& windowSurface) {
		uint32_t presentModeCount;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(_handle, windowSurface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
			std::cerr << "failed to get number of supported presentation modes" << std::endl;
			exit(1);
		}

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(_handle, windowSurface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported presentation modes" << std::endl;
			exit(1);
		}

		return presentModes;
	}
#pragma endregion

#pragma region DescriptorFunctions
	Descriptor::Descriptor(const VkDescriptorType& type, Buffer& buffer, const uint32_t& bindingNumber)
	{
		_bufferInfo = buffer.GenerateDescriptor();
		_imageInfo = VkDescriptorImageInfo{};
		_type = type;
		_bindingNumber = bindingNumber;
	}

	Descriptor::Descriptor(const VkDescriptorType& type, Image& image, const uint32_t& bindingNumber)
	{
		_bufferInfo = VkDescriptorBufferInfo{};
		_imageInfo = image.GenerateDescriptor();
		_type = type;
		_bindingNumber = bindingNumber;
	}
#pragma endregion

#pragma region DescriptorSetFunctions
	void DescriptorSet::SendDescriptorData()
	{
		std::vector<VkWriteDescriptorSet> writeInfos;

		for (int i = 0; i < _descriptors.size(); ++i) {
			VkWriteDescriptorSet writeInfo = {};
			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = _handle;
			writeInfo.descriptorCount = (uint32_t)_descriptors.size();
			writeInfo.descriptorType = _descriptors[i]->_type;
			writeInfo.pBufferInfo = _descriptors[i]->_bufferInfo.range == 0 ? nullptr : &_descriptors[i]->_bufferInfo;
			writeInfo.pImageInfo = _descriptors[i]->_imageInfo.sampler == VK_NULL_HANDLE ? nullptr : &_descriptors[i]->_imageInfo;
			writeInfo.dstBinding = _descriptors[i]->_bindingNumber;
			writeInfos.push_back(writeInfo);
		}

		vkUpdateDescriptorSets(_logicalDevice, (uint32_t)writeInfos.size(), writeInfos.data(), 0, nullptr);
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
			binding.descriptorCount = (uint32_t)descriptors.size();
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
#pragma endregion

#pragma region DescriptorPoolFunctions
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
				_descriptorSets[i]->_indexNumber = i;
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
					descriptorSet->SendDescriptorData();
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
#pragma endregion

#pragma region VulkanApplicationFunctions
	void VulkanApplication::Run()
	{
		InitializeWindow();
		_input = Input::KeyboardMouse(_window);
		SetupVulkan();
		MainLoop();
		Cleanup(true);
	}

	VkBool32 VulkanApplication::DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData)
	{
		if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
			std::cerr << "ERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
			std::cerr << "WARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
		}

		return VK_FALSE;
	}

	void VulkanApplication::InitializeWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_window = glfwCreateWindow(_settings._windowWidth, _settings._windowHeight, "Hold The Line!", nullptr, nullptr);
		glfwSetWindowSizeCallback(_window, VulkanApplication::OnWindowResized);
	}

	void VulkanApplication::SetupVulkan()
	{
		CreateInstance();
		CreateDebugCallback();
		CreateWindowSurface();
		_physicalDevice = PhysicalDevice(_instance);
		FindQueueFamilyIndex();
		CreateLogicalDeviceAndQueue();
		CreateSemaphores();
		CreateCommandPool();
		LoadScene();
		CreateVertexAndIndexBuffers();
		CreateSwapchain();
		CreateRenderPass();
		CreateFramebuffers();
		//LoadTexture();
		CreatePipelineLayout();
		CreateGraphicsPipeline();
		AllocateDrawCommandBuffers();
		RecordDrawCommands();
	}

	void VulkanApplication::MainLoop()
	{
		while (!glfwWindowShouldClose(_window)) {
			Update();
			Draw();
			glfwPollEvents();
		}
	}

	void VulkanApplication::Update()
	{
		Time::Instance().Update();
		_input.Update();
		_mainCamera.Update();
		UpdateShaderData();
	}

	void VulkanApplication::OnWindowResized(GLFWwindow* window, int width, int height)
	{
		windowResized = true;

		if (width == 0 && height == 0) {
			windowMinimized = true;
			return;
		}

		windowMinimized = false;
		Settings::GlobalSettings::Instance()._windowWidth = width;
		Settings::GlobalSettings::Instance()._windowHeight = height;
	}

	void VulkanApplication::WindowSizeChanged()
	{
		windowResized = false;

		// Only recreate objects that are affected by framebuffer size changes.
		Cleanup(false);
		CreateSwapchain();
		CreateRenderPass();
		CreateFramebuffers();
		CreateGraphicsPipeline();
		AllocateDrawCommandBuffers();
		RecordDrawCommands();
		vkDestroyPipelineLayout(_logicalDevice, _graphicsPipeline._shaderResources._pipelineLayout, nullptr);
	}

	void VulkanApplication::DestroyRenderPass()
	{
		_renderPass._depthImage.Destroy();
		/*for (auto image : _renderPass.colorImages) {
			image.Destroy();
		}*/

		vkDestroyRenderPass(_logicalDevice, _renderPass._handle, nullptr);
	}

	void VulkanApplication::DestroySwapchain()
	{
		for (size_t i = 0; i < _swapchain._frameBuffers.size(); i++) {
			vkDestroyFramebuffer(_logicalDevice, _swapchain._frameBuffers[i], nullptr);
		}
	}

	void VulkanApplication::Cleanup(bool fullClean)
	{
		vkDeviceWaitIdle(_logicalDevice);
		vkFreeCommandBuffers(_logicalDevice, _commandPool, (uint32_t)_drawCommandBuffers.size(), _drawCommandBuffers.data());
		vkDestroyPipeline(_logicalDevice, _graphicsPipeline._handle, nullptr);
		DestroyRenderPass();
		DestroySwapchain();

		vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._uniformSet._layout, nullptr);
		vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._samplerSet._layout, nullptr);

		if (fullClean) {
			vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(_logicalDevice, _renderingFinishedSemaphore, nullptr);
			DestroyRenderPass();
			vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

			// Clean up uniform buffer related objects.
			vkDestroyDescriptorPool(_logicalDevice, _graphicsPipeline._shaderResources._descriptorPool._handle, nullptr);
			_graphicsPipeline._shaderResources._transformationDataBuffer.Destroy();

			// Buffers must be destroyed after no command buffers are referring to them anymore.
			_graphicsPipeline._vertexBuffer.Destroy();
			_graphicsPipeline._indexBuffer.Destroy();

			// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly).
			DestroySwapchain();

			vkDestroyDevice(_logicalDevice, nullptr);

			vkDestroySurfaceKHR(_instance, _windowSurface, nullptr);

			if (_settings._enableValidationLayers) {
				PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
				DestroyDebugReportCallback(_instance, _callback, nullptr);
			}

			vkDestroyInstance(_instance, nullptr);
		}
	} // Cleanup

	bool VulkanApplication::ValidationLayersSupported(const std::vector<const char*>& validationLayers)
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}
		return true;
	}

	void VulkanApplication::CreateInstance()
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hold The Line!";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Celeritas Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Get instance extensions required by GLFW to draw to the window.
		unsigned int glfwExtensionCount;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions;
		for (size_t i = 0; i < glfwExtensionCount; i++) {
			extensions.push_back(glfwExtensions[i]);
		}

		if (_settings._enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "no extensions supported!" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		std::cout << "supported instance extensions:" << std::endl;

		for (const auto& extension : availableExtensions) {
			std::cout << "\t" << extension.extensionName << std::endl;
		}

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (_settings._enableValidationLayers) {
			if (ValidationLayersSupported(_settings._validationLayers)) {
				createInfo.enabledLayerCount = 1;
				createInfo.ppEnabledLayerNames = _settings._validationLayers.data();
			}
		}

		if (vkCreateInstance(&createInfo, nullptr, &_instance)) {
			std::cerr << "failed to create instance" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan instance" << std::endl;
		}
	}

	void VulkanApplication::CreateWindowSurface()
	{
		if (glfwCreateWindowSurface(_instance, _window, NULL, &_windowSurface) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}

		std::cout << "created window surface" << std::endl;
	}

	void VulkanApplication::FindQueueFamilyIndex()
	{
		auto queueFamilyProperties = _physicalDevice.GetAllQueueFamilyProperties();

		for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
			if (_physicalDevice.SupportsSurface(i, _windowSurface) &&
				queueFamilyProperties[i].queueCount > 0 &&
				queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

				_queue._familyIndex = i;
			}
		}
	}

	void VulkanApplication::CreateLogicalDeviceAndQueue()
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = _queue._familyIndex;
		queueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;

		// Necessary for shader (for some reason)
		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.shaderCullDistance = VK_TRUE;

		const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

		if (_settings._enableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = 1;
			deviceCreateInfo.ppEnabledLayerNames = _settings._validationLayers.data();
		}

		if (vkCreateDevice(_physicalDevice._handle, &deviceCreateInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
			std::cerr << "failed to create logical device" << std::endl;
			exit(1);
		}

		std::cout << "created logical device" << std::endl;

		vkGetDeviceQueue(_logicalDevice, _queue._familyIndex, 0, &_queue._handle);
	}

	void VulkanApplication::CreateDebugCallback()
	{
		if (_settings._enableValidationLayers) {
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

			PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");

			if (CreateDebugReportCallback(_instance, &createInfo, nullptr, &_callback) != VK_SUCCESS) {
				std::cerr << "failed to create debug callback" << std::endl;
				exit(1);
			}
			else {
				std::cout << "created debug callback" << std::endl;
			}
		}
		else {
			std::cout << "skipped creating debug callback" << std::endl;
		}
	}

	void VulkanApplication::CreateSemaphores()
	{
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_renderingFinishedSemaphore) != VK_SUCCESS) {
			std::cerr << "failed to create semaphores" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created semaphores" << std::endl;
		}
	}

	void VulkanApplication::CreateCommandPool()
	{
		// Create graphics command pool
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = _queue._familyIndex;

		if (vkCreateCommandPool(_logicalDevice, &poolCreateInfo, nullptr, &_commandPool) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	void VulkanApplication::LoadScene() {
		tinygltf::Model gltfScene;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		Scenes::Scene scene;

		auto scenePath = std::filesystem::current_path().string() + R"(\models\mp5k.glb)";
		bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, scenePath);
		std::cout << warn << std::endl;
		std::cout << err << std::endl;

		std::map<unsigned int, unsigned int> loadedMaterialCache; // Gltf material index, index of the materials in scene._materials.

		for (int i = 0; i < gltfScene.materials.size(); ++i) {
			Scenes::Material m;
			auto& baseColorTextureIndex = gltfScene.materials[i].pbrMetallicRoughness.baseColorTexture.index;
			m._name = gltfScene.materials[i].name;
			if (baseColorTextureIndex >= 0) {
				auto& baseColorimageIndex = gltfScene.textures[baseColorTextureIndex].source;
				auto& baseColorimageData = gltfScene.images[baseColorimageIndex].image;
				auto size = VkExtent2D{ (uint32_t)gltfScene.images[baseColorimageIndex].width, (uint32_t)gltfScene.images[baseColorimageIndex].height };
				m._baseColor = Scenes::Material::Texture(VK_FORMAT_R8G8B8A8_SRGB, baseColorimageData, size);;
				scene._materials.push_back(m);
				loadedMaterialCache.emplace(i, scene._materials.size() - 1);
			}
		}

		for (auto& gltfMesh : gltfScene.meshes) {
			for (auto& gltfPrimitive : gltfMesh.primitives) {
				auto gameObject = Scenes::GameObject();
				gameObject._scene = &scene;
				gameObject._name = gltfMesh.name;

				auto faceIndicesAccessorIndex = gltfPrimitive.indices;
				auto vertexPositionsAccessorIndex = gltfPrimitive.attributes["POSITION"];
				auto vertexNormalsAccessorIndex = gltfPrimitive.attributes["NORMAL"];
				auto uvCoords0AccessorIndex = gltfPrimitive.attributes["TEXCOORD_0"];
				auto uvCoords1AccessorIndex = gltfPrimitive.attributes["TEXCOORD_1"];
				auto uvCoords2AccessorIndex = gltfPrimitive.attributes["TEXCOORD_2"];

				auto faceIndicesAccessor = gltfScene.accessors[faceIndicesAccessorIndex];
				auto positionsAccessor = gltfScene.accessors[vertexPositionsAccessorIndex];
				auto normalsAccessor = gltfScene.accessors[vertexNormalsAccessorIndex];
				auto uvCoords0Accessor = gltfScene.accessors[uvCoords0AccessorIndex];
				auto uvCoords1Accessor = gltfScene.accessors[uvCoords1AccessorIndex];
				auto uvCoords2Accessor = gltfScene.accessors[uvCoords2AccessorIndex];

				// Load face indices.
				auto faceIndicesCount = faceIndicesAccessor.count;
				auto faceIndicesBufferIndex = gltfScene.bufferViews[faceIndicesAccessor.bufferView].buffer;
				auto faceIndicesBufferOffset = gltfScene.bufferViews[faceIndicesAccessor.bufferView].byteOffset;
				auto faceIndicesBufferStride = faceIndicesAccessor.ByteStride(gltfScene.bufferViews[faceIndicesAccessor.bufferView]);
				auto faceIndicesBufferSizeBytes = faceIndicesCount * faceIndicesBufferStride;
				std::vector<unsigned int> faceIndices(faceIndicesCount);
				if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					for (int i = 0; i < faceIndicesCount; ++i) {
						unsigned short index = 0;
						memcpy(&index, gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset + i * faceIndicesBufferStride, faceIndicesBufferStride);
						faceIndices[i] = index;
					}
				}
				else if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					memcpy(faceIndices.data(), gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset, faceIndicesBufferSizeBytes);
				}

				// Load vertex Positions.
				auto vertexPositionsCount = positionsAccessor.count;
				auto vertexPositionsBufferIndex = gltfScene.bufferViews[positionsAccessor.bufferView].buffer;
				auto vertexPositionsBufferOffset = gltfScene.bufferViews[positionsAccessor.bufferView].byteOffset;
				auto vertexPositionsBufferStride = positionsAccessor.ByteStride(gltfScene.bufferViews[positionsAccessor.bufferView]);
				auto vertexPositionsBufferSizeBytes = vertexPositionsCount * vertexPositionsBufferStride;
				std::vector<glm::vec3> vertexPositions(vertexPositionsCount);
				memcpy(vertexPositions.data(), gltfScene.buffers[vertexPositionsBufferIndex].data.data() + vertexPositionsBufferOffset, vertexPositionsBufferSizeBytes);

				// Load vertex normals.
				auto vertexNormalsCount = normalsAccessor.count;
				auto vertexNormalsBufferIndex = gltfScene.bufferViews[normalsAccessor.bufferView].buffer;
				auto vertexNormalsBufferOffset = gltfScene.bufferViews[normalsAccessor.bufferView].byteOffset;
				auto vertexNormalsBufferStride = normalsAccessor.ByteStride(gltfScene.bufferViews[normalsAccessor.bufferView]);
				auto vertexNormalsBufferSizeBytes = vertexNormalsCount * vertexNormalsBufferStride;
				std::vector<glm::vec3> vertexNormals(vertexNormalsCount);
				memcpy(vertexNormals.data(), gltfScene.buffers[vertexNormalsBufferIndex].data.data() + vertexNormalsBufferOffset, vertexNormalsBufferSizeBytes);

				// Load UV coordinates for UV slot 0.
				auto uvCoords0Count = uvCoords0Accessor.count;
				auto uvCoords0BufferIndex = gltfScene.bufferViews[uvCoords0Accessor.bufferView].buffer;
				auto uvCoords0BufferOffset = gltfScene.bufferViews[uvCoords0Accessor.bufferView].byteOffset;
				auto uvCoords0BufferStride = uvCoords0Accessor.ByteStride(gltfScene.bufferViews[uvCoords0Accessor.bufferView]);
				auto uvCoords0BufferSize = uvCoords0Count * uvCoords0BufferStride;
				std::vector<glm::vec2> uvCoords0(uvCoords0Count);
				memcpy(uvCoords0.data(), gltfScene.buffers[uvCoords0BufferIndex].data.data() + uvCoords0BufferOffset, uvCoords0BufferSize);

				/*auto uvCoords1Count = uvCoords1Accessor.count;
				auto uvCoords1BufferIndex = gltfScene.bufferViews[uvCoords1Accessor.bufferView].buffer;
				auto uvCoords1BufferOffset = gltfScene.bufferViews[uvCoords1Accessor.bufferView].byteOffset;
				auto uvCoords1BufferSize = gltfScene.bufferViews[uvCoords1Accessor.bufferView].byteLength;
				auto uvCoords1BufferStride = gltfScene.bufferViews[uvCoords1Accessor.bufferView].byteStride;

				std::vector<glm::vec2> uvCoords1(uvCoords1Count);
				memcpy(uvCoords1.data(), gltfScene.buffers[uvCoords1BufferIndex].data.data() + uvCoords1BufferOffset, uvCoords1BufferSize);

				auto uvCoords2Count = uvCoords2Accessor.count;
				auto uvCoords2BufferIndex = gltfScene.bufferViews[uvCoords2Accessor.bufferView].buffer;
				auto uvCoords2BufferOffset = gltfScene.bufferViews[uvCoords2Accessor.bufferView].byteOffset;
				auto uvCoords2BufferSize = gltfScene.bufferViews[uvCoords2Accessor.bufferView].byteLength;
				auto uvCoords2BufferStride = gltfScene.bufferViews[uvCoords2Accessor.bufferView].byteStride;

				std::vector<glm::vec2> uvCoords2(uvCoords2Count);
				memcpy(uvCoords2.data(), gltfScene.buffers[uvCoords2BufferIndex].data.data() + uvCoords2BufferOffset, uvCoords2BufferSize);*/

				auto& mesh = gameObject._mesh;

				// Load material.
				if (loadedMaterialCache.count(gltfPrimitive.material) > 0) {
					mesh._materialIndex = loadedMaterialCache[gltfPrimitive.material];
				}

				/*_graphicsPipeline._shaderResources._texture = Image(_logicalDevice,
					_physicalDevice,
					VK_FORMAT_R8G8B8A8_SRGB,
					VkExtent2D{ (uint32_t)baseColorimageWidth, (uint32_t)baseColorimageHeight },
					imageData.data(),
					(VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

				_graphicsPipeline._shaderResources._texture.SendToGPU(_commandPool, _queue);*/

				mesh._vertices.resize(vertexPositions.size());
				for (int i = 0; i < vertexPositions.size(); ++i) {
					Scenes::Mesh::Vertex v;
					v._position = vertexPositions[i];
					v._normal = vertexNormals[i];
					v._uvCoord = uvCoords0[i];
					mesh._vertices[i] = v;
				}

				mesh._faceIndices = faceIndices;
				scene._objects.push_back(gameObject);
				mesh._gameObject = (unsigned int)(scene._objects.size() - 1);
			}
		}

		//std::vector<Scenes::Mesh::Vertex> verts{};
		//std::vector<unsigned int> indices{};

		_model = scene._objects[0];

		//verts.push_back(Scenes::Mesh::Vertex{ glm::vec3{6.0f, 6.0f, 6.0f}, glm::vec3{}, glm::vec2{} }); // Tip.
		//verts.push_back(Scenes::Mesh::Vertex{ glm::vec3{6.0f, 0.0f, 6.0f}, glm::vec3{}, glm::vec2{} }); // Lower right.
		//verts.push_back(Scenes::Mesh::Vertex{ glm::vec3{0.0f, 0.0f, 6.0f}, glm::vec3{}, glm::vec2{} }); // Lower left.

		//indices.push_back(0);
		//indices.push_back(2);
		//indices.push_back(1);

		//_model._mesh._vertices = verts;
		//_model._mesh._faceIndices = indices;
	}

	void VulkanApplication::CreateVertexAndIndexBuffers()
	{
		auto vertexPositionsSize = Utils::GetVectorSizeInBytes(_model._mesh._vertices);
		auto faceIndicesSize = Utils::GetVectorSizeInBytes(_model._mesh._faceIndices);

#pragma region VerticesToVertexBuffer
		// Create a buffer used as a middleman buffer to transfer data from the RAM to the VRAM. This buffer will be created in RAM.
		auto vertexTransferBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			_model._mesh._vertices.data(),
			vertexPositionsSize);

		_graphicsPipeline._vertexBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			nullptr,
			vertexPositionsSize);

#pragma endregion

#pragma region FaceIndicesToIndexBuffer
		auto indexTransferBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			(void*)_model._mesh._faceIndices.data(),
			faceIndicesSize);

		_graphicsPipeline._indexBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			nullptr,
			faceIndicesSize);
#pragma endregion

#pragma region CommandBufferCreationAndExecution
		// Now copy data from host visible buffer to gpu only buffer
		VkCommandBufferBeginInfo bufferBeginInfo = {};
		bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		// Allocate a command buffer for the copy operations to follow
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = _commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		vkAllocateCommandBuffers(_logicalDevice, &cmdBufInfo, &copyCommandBuffer);

		vkBeginCommandBuffer(copyCommandBuffer, &bufferBeginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.size = vertexPositionsSize;
		vkCmdCopyBuffer(copyCommandBuffer, vertexTransferBuffer._handle, _graphicsPipeline._vertexBuffer._handle, 1, &copyRegion);
		copyRegion.size = faceIndicesSize;
		vkCmdCopyBuffer(copyCommandBuffer, indexTransferBuffer._handle, _graphicsPipeline._indexBuffer._handle, 1, &copyRegion);

		vkEndCommandBuffer(copyCommandBuffer);

		// Submit to queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;

		vkQueueSubmit(_queue._handle, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(_queue._handle);

		vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &copyCommandBuffer);
#pragma endregion

		vertexTransferBuffer.Destroy();
		indexTransferBuffer.Destroy();

		std::cout << "set up vertex and index buffers" << std::endl;
	}

	VkPresentModeKHR VulkanApplication::ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes)
	{
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkSurfaceFormatKHR VulkanApplication::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// We can either choose any format
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
		}

		// Or go with the standard format - if available
		for (const auto& availableSurfaceFormat : availableFormats) {
			if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM) {
				return availableSurfaceFormat;
			}
		}

		// Or fall back to the first available one
		return availableFormats[0];
	}

	VkExtent2D VulkanApplication::ChooseFramebufferSize(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
	{
		auto& settings = Settings::GlobalSettings::Instance();

		if (surfaceCapabilities.currentExtent.width == -1) {
			VkExtent2D swapChainExtent = {};

			swapChainExtent.width = std::min(std::max(settings._windowWidth, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
			swapChainExtent.height = std::min(std::max(settings._windowHeight, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);

			return swapChainExtent;
		}
		else {
			return surfaceCapabilities.currentExtent;
		}
	}

	void VulkanApplication::CreateFramebuffers()
	{
		_swapchain._frameBuffers.resize(_renderPass._colorImages.size());

		for (size_t i = 0; i < _renderPass._colorImages.size(); i++) {

			// We will render to the same depth image for each frame. 
			// We can just keep clearing and reusing the same depth image for every frame.
			VkImageView colorAndDepthImages[2] = { _renderPass._colorImages[i]._imageViewHandle, _renderPass._depthImage._imageViewHandle };
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = _renderPass._handle;
			createInfo.attachmentCount = 2;
			createInfo.pAttachments = &colorAndDepthImages[0];
			createInfo.width = _swapchain._framebufferSize.width;
			createInfo.height = _swapchain._framebufferSize.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &_swapchain._frameBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created framebuffers for swap chain image views" << std::endl;
	}

	void VulkanApplication::Draw()
	{
		if (!windowMinimized) {

			// Acquire image.
			uint32_t imageIndex;
			VkResult res = vkAcquireNextImageKHR(_logicalDevice, _swapchain._handle, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			// Unless surface is out of date right now, defer swap chain recreation until end of this frame.
			if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || windowResized) {
				WindowSizeChanged();
				return;
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to acquire image" << std::endl;
				exit(1);
			}

			// Wait for image to be available and draw.
			// This is the stage where the queue should wait on the semaphore.
			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &_imageAvailableSemaphore;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &_renderingFinishedSemaphore;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_drawCommandBuffers[imageIndex];

			if (auto res = vkQueueSubmit(_queue._handle, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
				std::cerr << "failed to submit draw command buffer" << std::endl;
				exit(1);
			}

			// Present drawn image.
			// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue.
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &_renderingFinishedSemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &_swapchain._handle;
			presentInfo.pImageIndices = &imageIndex;

			res = vkQueuePresentKHR(_queue._handle, &presentInfo);

			if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
				WindowSizeChanged();
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to submit present command buffer" << std::endl;
				exit(1);
			}
		}
	}

	void VulkanApplication::CreateRenderPass()
	{
		auto imageFormat = _swapchain._surfaceFormat.format;

		// Store the images used by the swap chain.
		// Note: these are the images that swap chain image indices refer to.
		// Note: actual number of images may differ from requested number, since it's a lower bound.
		uint32_t actualImageCount = 0;
		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}

		_renderPass._colorImages.resize(actualImageCount);

		std::vector<VkImage> images;
		images.resize(actualImageCount);
		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, images.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swapchain images" << std::endl;
			exit(1);
		}

		std::cout << "acquired swap chain images" << std::endl;

		// Create the color attachments.
		for (uint32_t i = 0; i < actualImageCount; ++i) {
			_renderPass._colorImages[i] = Image(_logicalDevice, images[i], VK_FORMAT_R8G8B8A8_UNORM);
		}

		// Create the depth attachment.
		auto& settings = Settings::GlobalSettings::Instance();
		_renderPass._depthImage = Image(_logicalDevice,
			_physicalDevice,
			VK_FORMAT_D32_SFLOAT,
			_swapchain._framebufferSize,
			nullptr,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		std::cout << "created image views for swap chain images" << std::endl;

		// Describes how the render pass is going to use the main color attachment. An attachment is a fancy word for image.
		VkAttachmentDescription colorAttachmentDescription = {};
		colorAttachmentDescription.format = _renderPass._colorImages.size() > 0 ? _renderPass._colorImages[0]._format : VK_FORMAT_UNDEFINED;
		colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Note: hardware will automatically transition attachment to the specified layout
		// Note: index refers to attachment descriptions array.
		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Describes how the render pass is going to use the depth attachment.
		VkAttachmentDescription depthAttachmentDescription = {};
		depthAttachmentDescription.flags = 0;
		depthAttachmentDescription.format = _renderPass._depthImage._format;
		depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference = {};
		depthAttachmentReference.attachment = 1;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Note: this is a description of how the attachments of the render pass will be used in this sub pass
		// e.g. if they will be read in shaders and/or drawn to.
		VkSubpassDescription subPassDescription = {};
		subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subPassDescription.colorAttachmentCount = 1;
		subPassDescription.pColorAttachments = &colorAttachmentReference;
		subPassDescription.pDepthStencilAttachment = &depthAttachmentReference;

		// Now we have to adjust the renderpass synchronization. Previously, it was possible that multiple frames 
		// were rendered simultaneously by the GPU. This is a problem when using depth buffers, because one frame 
		// could overwrite the depth buffer while a previous frame is still rendering to it.
		// We keep the subpass dependency for the color attachment we were already using.
		VkSubpassDependency colorDependency = {};
		colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		colorDependency.dstSubpass = 0;
		colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.srcAccessMask = 0;
		colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before 
		// previous renderpasses have finished using it.
		VkSubpassDependency depthDependency = {};
		depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depthDependency.dstSubpass = 0;
		depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.srcAccessMask = 0;
		depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// Create the render pass. We pass in the main image attachment (color) and the depth image attachment, so the GPU knows how to treat
		// the images.
		VkAttachmentDescription attachmentDescriptions[2] = { colorAttachmentDescription, depthAttachmentDescription };
		VkSubpassDependency subpassDependencies[2] = { colorDependency, depthDependency };
		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 2;
		createInfo.pAttachments = &attachmentDescriptions[0];
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subPassDescription;
		createInfo.dependencyCount = 2;
		createInfo.pDependencies = &subpassDependencies[0];

		if (vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass._handle) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
	}

	void VulkanApplication::CreateSwapchain()
	{
		// Get physical device capabilities for the window surface.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = _physicalDevice.GetSurfaceCapabilities(_windowSurface);
		std::vector<VkSurfaceFormatKHR> surfaceFormats = _physicalDevice.GetSupportedFormatsForSurface(_windowSurface);
		std::vector<VkPresentModeKHR> presentModes = _physicalDevice.GetSupportedPresentModesForSurface(_windowSurface);

		// Determine number of images for swapchain.
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		std::cout << "using " << imageCount << " images for swap chain" << std::endl;

		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
		_swapchain._framebufferSize = ChooseFramebufferSize(surfaceCapabilities);

		// Determine transformation to use (preferring no transform).
		VkSurfaceTransformFlagBitsKHR surfaceTransform;
		if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			surfaceTransform = surfaceCapabilities.currentTransform;
		}

		// Choose presentation mode (preferring MAILBOX ~= triple buffering).
		VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

		// Finally, create the swap chain.
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = _windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = _swapchain._framebufferSize;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = _swapchain._oldSwapchainHandle;

		if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapchain._handle) != VK_SUCCESS) {
			std::cerr << "failed to create swapchain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swapchain" << std::endl;
		}

		if (_swapchain._oldSwapchainHandle != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(_logicalDevice, _swapchain._oldSwapchainHandle, nullptr);
		}

		_swapchain._oldSwapchainHandle = _swapchain._handle;
	}

	VkShaderModule VulkanApplication::CreateShaderModule(const std::filesystem::path& absolutePath)
	{
		std::ifstream file(absolutePath.c_str(), std::ios::ate | std::ios::binary);
		VkShaderModule shaderModule = nullptr;
		if (file.is_open()) {
			std::vector<char> fileBytes(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(fileBytes.data(), fileBytes.size());
			file.close();

			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = fileBytes.size();
			createInfo.pCode = (uint32_t*)fileBytes.data();

			if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				std::wcerr << "failed to create shader module for " << absolutePath.c_str() << std::endl;
				exit(1);
			}

			std::wcout << "created shader module for " << absolutePath.c_str() << std::endl;
		}
		else {
			std::wcout << "failed to open file " << absolutePath.c_str() << std::endl;
			exit(0);
		}

		return shaderModule;
	}

	void VulkanApplication::CreateGraphicsPipeline()
	{
		using Vertex = Scenes::Mesh::Vertex;

		VkShaderModule vertexShaderModule = CreateShaderModule(Settings::Paths::_vertexShaderPath());
		VkShaderModule fragmentShaderModule = CreateShaderModule(Settings::Paths::_fragmentShaderPath());

		// Set up shader stage info.
		VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
		vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexShaderCreateInfo.module = vertexShaderModule;
		vertexShaderCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
		fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentShaderCreateInfo.module = fragmentShaderModule;
		fragmentShaderCreateInfo.pName = "main";
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

		// Vertex attribute binding - gives the vertex shader more info about a particular vertex buffer, denoted by the binding number. See binding for more info.
		_graphicsPipeline._vertexBindingDescription.binding = 0;
		_graphicsPipeline._vertexBindingDescription.stride = sizeof(Vertex);
		_graphicsPipeline._vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
		// Object-space positions.
		_graphicsPipeline._vertexAttributeDescriptions.resize(3);
		_graphicsPipeline._vertexAttributeDescriptions[0].location = 0;
		_graphicsPipeline._vertexAttributeDescriptions[0].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[0].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Position);

		// Normals.
		_graphicsPipeline._vertexAttributeDescriptions[1].location = 1;
		_graphicsPipeline._vertexAttributeDescriptions[1].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[1].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Normal);

		// UV coordinates.
		_graphicsPipeline._vertexAttributeDescriptions[2].location = 2;
		_graphicsPipeline._vertexAttributeDescriptions[2].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[2].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::UV);

		// Describe vertex input.
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &_graphicsPipeline._vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)_graphicsPipeline._vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = _graphicsPipeline._vertexAttributeDescriptions.data();

		// Describe input assembly - this allows Vulkan to know how many indices make up a face for the vkCmdDrawIndexed function.
		// The input assembly is the very first stage of the graphics pipeline, where vertices and indices are loaded from VRAM and assembled,
		// to then be passed to the shaders.
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// Describe viewport and scissor
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = Utils::Converter::Convert<uint32_t, float>(_swapchain._framebufferSize.width);
		viewport.height = Utils::Converter::Convert<uint32_t, float>(_swapchain._framebufferSize.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _swapchain._framebufferSize.width;
		scissor.extent.height = _swapchain._framebufferSize.height;

		// Note: scissor test is always enabled (although dynamic scissor is possible).
		// Number of viewports must match number of scissors.
		VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
		viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCreateInfo.viewportCount = 1;
		viewportCreateInfo.pViewports = &viewport;
		viewportCreateInfo.scissorCount = 1;
		viewportCreateInfo.pScissors = &scissor;

		// Describe rasterization - this tells Vulkan what settings to use when at the fragment shader stage of the pipeline, a.k.a. when
		// rendering pixels.
		// Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features).
		VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
		rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationCreateInfo.depthBiasClamp = 0.0f;
		rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
		rasterizationCreateInfo.lineWidth = 1.0f;

		// Configure depth testing.
		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.pNext = nullptr;
		depthStencilCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilCreateInfo.minDepthBounds = 0.0f;
		depthStencilCreateInfo.maxDepthBounds = 1.0f;

		// Describe multisampling
		// Note: using multisampling also requires turning on device features.
		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.minSampleShading = 1.0f;
		multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

		// Describing color blending.
		// Note: all paramaters except blendEnable and colorWriteMask are irrelevant here.
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
		colorBlendAttachmentState.blendEnable = VK_FALSE;
		colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		// Note: all attachments must have the same values unless a device feature is enabled.
		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
		colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		colorBlendCreateInfo.attachmentCount = 1;
		colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;
		colorBlendCreateInfo.blendConstants[0] = 0.0f;
		colorBlendCreateInfo.blendConstants[1] = 0.0f;
		colorBlendCreateInfo.blendConstants[2] = 0.0f;
		colorBlendCreateInfo.blendConstants[3] = 0.0f;

		// Create the graphics pipeline.
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = 2;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
		pipelineCreateInfo.layout = _graphicsPipeline._shaderResources._pipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass._handle;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline._handle) != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		// No longer necessary as it has all been put into the _graphicsPipeline object.
		vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
	}

	void VulkanApplication::UpdateShaderData()
	{
		auto& shaderResources = _graphicsPipeline._shaderResources;

		shaderResources._transformationData.objectToWorld = _model._transform._matrix;
		shaderResources._transformationData.worldToCamera = _mainCamera._view._matrix;
		shaderResources._transformationData.tanHalfHorizontalFov = tan(glm::radians(_mainCamera._horizontalFov / 2.0f));
		shaderResources._transformationData.aspectRatio = Utils::Converter::Convert<uint32_t, float>(_settings._windowWidth) / Utils::Converter::Convert<uint32_t, float>(_settings._windowHeight);
		shaderResources._transformationData.nearClipDistance = _mainCamera._nearClippingDistance;
		shaderResources._transformationData.farClipDistance = _mainCamera._farClippingDistance;

		shaderResources._transformationDataBuffer.UpdateData(&shaderResources._transformationData, (size_t)sizeof(shaderResources._transformationData));
	}

	void VulkanApplication::CreatePipelineLayout()
	{
		auto& shaderResources = _graphicsPipeline._shaderResources;

		shaderResources._transformationDataBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&shaderResources._transformationData,
			(size_t)sizeof(shaderResources._transformationData));

		shaderResources._transformationsDescriptor = Descriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderResources._transformationDataBuffer, 0);
		shaderResources._textureDescriptor = Descriptor(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderResources._texture, 0);
		shaderResources._uniformSet = DescriptorSet(_logicalDevice, VK_SHADER_STAGE_VERTEX_BIT, { &shaderResources._transformationsDescriptor });
		shaderResources._samplerSet = DescriptorSet(_logicalDevice, VK_SHADER_STAGE_FRAGMENT_BIT, { &shaderResources._textureDescriptor });
		shaderResources._descriptorPool = DescriptorPool(_logicalDevice, { &shaderResources._uniformSet, &shaderResources._samplerSet });

		UpdateShaderData();
		shaderResources._descriptorPool.UpdateDescriptor(shaderResources._transformationsDescriptor, shaderResources._transformationDataBuffer);

		std::vector<VkDescriptorSetLayout> layouts = { shaderResources._uniformSet._layout, shaderResources._samplerSet._layout };
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();

		if (vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &shaderResources._pipelineLayout) != VK_SUCCESS) {
			std::cerr << "failed to create pipeline layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created pipeline layout" << std::endl;
		}
	}

	void VulkanApplication::AllocateDrawCommandBuffers()
	{
		// Allocate graphics command buffers
		_drawCommandBuffers.resize(_renderPass._colorImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)_renderPass._colorImages.size();

		if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _drawCommandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "failed to allocate draw command buffers" << std::endl;
			exit(1);
		}
		else {
			std::cout << "allocated draw command buffers" << std::endl;
		}
	}

	void VulkanApplication::RecordDrawCommands()
	{
		auto& shaderResources = _graphicsPipeline._shaderResources;

		// Prepare data for recording command buffers
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkImageSubresourceRange subResourceRange = {};
		subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResourceRange.baseMipLevel = 0;
		subResourceRange.levelCount = 1;
		subResourceRange.baseArrayLayer = 0;
		subResourceRange.layerCount = 1;

		// Record command buffer for each swap image
		for (size_t i = 0; i < _renderPass._colorImages.size(); i++) {
			vkBeginCommandBuffer(_drawCommandBuffers[i], &beginInfo);

			// If present queue family and graphics queue family are different, then a barrier is necessary
			// The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier presentToDrawBarrier = {};
			presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presentToDrawBarrier.srcAccessMask = 0;
			presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			presentToDrawBarrier.srcQueueFamilyIndex = _queue._familyIndex;
			presentToDrawBarrier.dstQueueFamilyIndex = _queue._familyIndex;
			presentToDrawBarrier.image = _renderPass._colorImages[i]._imageHandle;
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(_drawCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

#pragma region RenderPassCommandRecording
			VkClearValue clearColor = {
			{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A
			};

			VkClearValue depthClear;
			depthClear.depthStencil.depth = 1.0f;
			VkClearValue clearValues[] = { clearColor, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = _renderPass._handle;
			renderPassBeginInfo.framebuffer = _swapchain._frameBuffers[i];
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = _swapchain._framebufferSize;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = &clearValues[0];

			vkCmdBeginRenderPass(_drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			/*VkDescriptorSet sets[2] = { shaderResources._uniformSet._handle, shaderResources._samplerSet._handle };
			vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shaderResources._pipelineLayout, 0, 2, sets, 0, nullptr);
			vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._handle);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(_drawCommandBuffers[i], 0, 1, &_graphicsPipeline._vertexBuffer._handle, &offset);
			vkCmdBindIndexBuffer(_drawCommandBuffers[i], _graphicsPipeline._indexBuffer._handle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(_drawCommandBuffers[i], (uint32_t)_model._mesh._faceIndices.size(), 1, 0, 0, 0);*/
			for(auto& gameObject : _scene._objects)
			vkCmdEndRenderPass(_drawCommandBuffers[i]);

			if (vkEndCommandBuffer(_drawCommandBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
			}
#pragma endregion
		}

		std::cout << "recorded draw commands in draw command buffers." << std::endl;
	}

	VkFormat VulkanApplication::ChooseImageFormat(const std::filesystem::path& absolutePathToImage)
	{
		auto extension = absolutePathToImage.extension();

		if (extension == L".jpg") {
			return VK_FORMAT_R8G8B8_SRGB;
		}
		else if (extension == L".png") {
			return VK_FORMAT_R8G8B8A8_SRGB;
		}

		return VK_FORMAT_UNDEFINED;
	}

	void VulkanApplication::LoadTexture()
	{
		// Read the file and point to its location in memory.
		auto texturePath = R"(C:\Code\celeritas-engine\textures\ItalianFlag.jpg)";
		int width, height;
		void* pixels = stbi_load(texturePath, &width, &height, nullptr, 0);
		auto format = ChooseImageFormat(texturePath);

		// Allocate and create the image.
		_graphicsPipeline._shaderResources._texture = Image(_logicalDevice,
			_physicalDevice,
			format,
			VkExtent2D{ (uint32_t)width,(uint32_t)height },
			pixels,
			(VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		_graphicsPipeline._shaderResources._texture.SendToGPU(_commandPool, _queue);

		std::cout << "texture loaded successfully" << std::endl;
	}
#pragma endregion

}

int main()
{
	Settings::GlobalSettings::Instance().Load(Settings::Paths::_settings());
	Engine::Vulkan::VulkanApplication app;
	app.Run();

	return 0;
}
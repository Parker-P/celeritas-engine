#define GLFW_INCLUDE_VULKAN

#include <windows.h>

// STL
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

// Math
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

// Project local classes
#include "utils/Json.h"
#include "structural/IUpdatable.h"
#include "structural/Singleton.hpp"
#include "settings/GlobalSettings.hpp"
#include "settings/Paths.hpp"
#include "engine/Time.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Camera.hpp"
#include "engine/scenes/Scene.hpp"
#include "utils/Utils.hpp"
#include "engine/scenes/GltfLoader.hpp"
#include "engine/math/Transform.hpp"
#include "engine/VulkanApplication.hpp"

namespace Engine::Vulkan
{
	bool windowResized = false;

	// Buffer
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

		if (data != nullptr) {
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

	// Image
	Image::Image(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const VkFormat& imageFormat, const VkExtent2D& size, const VkImageUsageFlagBits& usageFlags, const VkImageAspectFlagBits& aspectFlags, const VkMemoryPropertyFlagBits& memoryPropertiesFlags)
	{
		_logicalDevice = logicalDevice;
		_physicalDevice = physicalDevice;
		_format = imageFormat;

		VkImageCreateInfo imageCreateInfo = { };
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = imageFormat;

		auto imageSize = VkExtent3D{ size.width, size.height, 1 };
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

		// Create the image.
		if (vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &_imageHandle) != VK_SUCCESS) {
			std::cout << "Failed creating depth image." << std::endl;
		}

		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(logicalDevice, _imageHandle, &reqs);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = _physicalDevice.GetMemoryTypeIndex(reqs.memoryTypeBits, memoryPropertiesFlags);

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
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &_imageViewHandle) != VK_SUCCESS) {
			std::cout << "Failed creating depth image view." << std::endl;
		}
	}

	Image::Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat)
	{
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

	void Image::Destroy()
	{
		vkDestroyImageView(_logicalDevice, _imageViewHandle, nullptr);
		vkDestroyImage(_logicalDevice, _imageHandle, nullptr);
	}

	// Swapchain
	Swapchain::Swapchain(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface, const VkSwapchainKHR& oldSwapchainHandle)
	{
		_logicalDevice = logicalDevice;
		_physicalDevice = physicalDevice;
		_windowSurface = windowSurface;

		// Get physical device capabilities for the window surface.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = _physicalDevice.GetSurfaceCapabilities(windowSurface);
		std::vector<VkSurfaceFormatKHR> surfaceFormats = _physicalDevice.GetSupportedFormatsForSurface(windowSurface);
		std::vector<VkPresentModeKHR> presentModes = _physicalDevice.GetSupportedPresentModesForSurface(windowSurface);

		// Determine number of images for swap chain.
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		std::cout << "using " << imageCount << " images for swap chain" << std::endl;

		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
		_framebufferSize = ChooseFramebufferSize(surfaceCapabilities);

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
		createInfo.surface = windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = _framebufferSize;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapchainHandle;

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &_handle) != VK_SUCCESS) {
			std::cerr << "failed to create swapchain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swapchain" << std::endl;
		}

		if (oldSwapchainHandle != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(logicalDevice, oldSwapchainHandle, nullptr);
		}
		_oldSwapchainHandle = oldSwapchainHandle;

		auto imageFormat = surfaceFormat.format;

		// Store the images used by the swap chain.
		// Note: these are the images that swap chain image indices refer to.
		// Note: actual number of images may differ from requested number, since it's a lower bound.
		uint32_t actualImageCount = 0;
		if (vkGetSwapchainImagesKHR(logicalDevice, _handle, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}

		_colorImages.resize(actualImageCount);

		std::vector<VkImage> images;
		images.resize(actualImageCount);
		if (vkGetSwapchainImagesKHR(logicalDevice, _handle, &actualImageCount, images.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swapchain images" << std::endl;
			exit(1);
		}

		std::cout << "acquired swap chain images" << std::endl;

		// Create the color images.
		for (int i = 0; i < actualImageCount; ++i) {
			_colorImages[i] = Image(logicalDevice, images[i], VK_FORMAT_R8G8B8A8_UNORM);
		}

		// Create the depth image.
		auto& settings = Settings::GlobalSettings::Instance();
		_depthImage = Image(logicalDevice, physicalDevice, VK_FORMAT_D32_SFLOAT, _framebufferSize, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		std::cout << "created image views for swap chain images" << std::endl;
	}

	VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes)
	{
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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

	VkExtent2D Swapchain::ChooseFramebufferSize(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
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

	void Swapchain::CreateFramebuffers(VkDevice& _logicalDevice, VkRenderPass& renderPass)
	{
		_frameBuffers.resize(_colorImages.size());

		for (size_t i = 0; i < _colorImages.size(); i++) {

			// We will render to the same depth image for each frame. 
			// We can just keep clearing and reusing the same depth image for every frame.
			VkImageView colorAndDepthImages[2] = { _colorImages[i]._imageViewHandle, _depthImage._imageViewHandle };
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.attachmentCount = 2;
			createInfo.pAttachments = &colorAndDepthImages[0];
			createInfo.width = _framebufferSize.width;
			createInfo.height = _framebufferSize.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &_frameBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created framebuffers for swap chain image views" << std::endl;
	}

	void Swapchain::Draw(VkSemaphore& imageAvailableSemaphore, VkSemaphore& renderingFinishedSemaphore, std::vector<VkCommandBuffer>& graphicsCommandBuffers, VkQueue& graphicsQueue, VkQueue& presentationQueue)
	{
		// Acquire image.
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(_logicalDevice, _handle, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		// Unless surface is out of date right now, defer swap chain recreation until end of this frame.
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			OnWindowSizeChanged();
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
		submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderingFinishedSemaphore;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsCommandBuffers[imageIndex];

		if (auto res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
			std::cerr << "failed to submit draw command buffer" << std::endl;
			exit(1);
		}

		// Present drawn image.
		// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue.
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderingFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &_handle;
		presentInfo.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(presentationQueue, &presentInfo);

		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
			OnWindowSizeChanged();
		}
		else if (res != VK_SUCCESS) {
			std::cerr << "failed to submit present command buffer" << std::endl;
			exit(1);
		}
	}

	void Swapchain::Destroy()
	{
		_depthImage.Destroy();
		for (auto image : _colorImages) {
			image.Destroy();
		}

		for (size_t i = 0; i < _frameBuffers.size(); i++) {
			vkDestroyFramebuffer(_logicalDevice, _frameBuffers[i], nullptr);
		}
	}

	// Physical device
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

		if (queueFamilyCount == 0) {
			std::cout << "physical device has no queue families!" << std::endl;
			exit(1);
		}

		// Find queue family with graphics support
		// Note: is a transfer queue necessary to copy vertices to the gpu or can a graphics queue handle that?
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(_handle, &queueFamilyCount, queueFamilies.data());

		std::cout << "physical device has " << queueFamilyCount << " queue families." << std::endl;
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

	// VulkanApplication
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

	void VulkanApplication::Run()
	{
		InitializeWindow();
		_input = Input::KeyboardMouse(_window);
		LoadScene();
		SetupVulkan();
		MainLoop();
		Cleanup(true);
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
		FindPhysicalDevice();
		CreateLogicalDevice();
		CreateSemaphores();
		CreateCommandPool();
		CreateVertexAndIndexBuffers();
		CreateSwapchain();
		CreateDepthImage();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateCommandBuffers();
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
		Settings::GlobalSettings::Instance()._windowWidth = width;
		Settings::GlobalSettings::Instance()._windowHeight = height;
	}

	void VulkanApplication::OnWindowSizeChanged()
	{
		windowResized = false;

		// Only recreate objects that are affected by framebuffer size changes
		Cleanup(false);

		CreateSwapchain();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateCommandBuffers();
	}

	void VulkanApplication::Cleanup(bool fullClean)
	{
		vkDeviceWaitIdle(_logicalDevice);
		vkFreeCommandBuffers(_logicalDevice, _commandPool, (uint32_t)_graphicsCommandBuffers.size(), _graphicsCommandBuffers.data());
		vkDestroyPipeline(_logicalDevice, _graphicsPipeline, nullptr);
		vkDestroyRenderPass(_logicalDevice, _renderPass, nullptr);
		_swapchain.Destroy();
		
		vkDestroyDescriptorSetLayout(_logicalDevice, _descriptorSetLayout, nullptr);

		if (fullClean) {
			vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(_logicalDevice, _renderingFinishedSemaphore, nullptr);

			vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

			// Clean up uniform buffer related objects
			vkDestroyDescriptorPool(_logicalDevice, _descriptorPool, nullptr);
			_uniformBuffer.Destroy();

			// Buffers must be destroyed after no command buffers are referring to them anymore
			_vertexBuffer->Destroy();
			_indexBuffer->Destroy();

			// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
			_swapchain.Destroy();

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

		// Get _instance extensions required by GLFW to draw to window
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

		// Check for extensions
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "no extensions supported!" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		std::cout << "supported extensions:" << std::endl;

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

		// Initialize Vulkan _instance
		if (vkCreateInstance(&createInfo, nullptr, &_instance)) {
			std::cerr << "failed to create _instance!" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan _instance" << std::endl;
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

	void VulkanApplication::FindPhysicalDevice()
	{
		_physicalDevice = PhysicalDevice(_instance);
	}

	void VulkanApplication::CreateLogicalDevice()
	{
		// Greate one graphics queue and optionally a separate presentation queue
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfo[2] = {};

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = _graphicsQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = _presentQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		// Create logical device from physical device
		// Note: there are separate _instance and device extensions!
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;

		if (_graphicsQueueFamily == _presentQueueFamily) {
			deviceCreateInfo.queueCreateInfoCount = 1;
		}
		else {
			deviceCreateInfo.queueCreateInfoCount = 2;
		}

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

		// Get graphics and presentation queues (which may be the same)
		vkGetDeviceQueue(_logicalDevice, _graphicsQueueFamily, 0, &_graphicsQueue);
		vkGetDeviceQueue(_logicalDevice, _presentQueueFamily, 0, &_presentQueue);

		std::cout << "acquired graphics and presentation queues" << std::endl;
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
		poolCreateInfo.queueFamilyIndex = _graphicsQueueFamily;

		if (vkCreateCommandPool(_logicalDevice, &poolCreateInfo, nullptr, &_commandPool) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	void VulkanApplication::LoadScene() {
		//_scene = Scenes::GltfLoader::LoadScene(std::filesystem::current_path().string() + R"(\models\2CylinderEngine.glb)");
		//_scene = Scenes::GltfLoader::LoadScene(std::filesystem::current_path().string() + R"(\models\axesTest.glb)");
		//_scene = Scenes::GltfLoader::LoadScene(std::filesystem::current_path().string() + R"(\models\monkey.glb)");
		_scene = Scenes::GltfLoader::LoadScene(std::filesystem::current_path().string() + R"(\models\monster.glb)");
		//_scene = Scenes::GltfLoader::LoadScene(std::filesystem::current_path().string() + R"(\models\test.glb)");
		_model = _scene._objects[0];

		//std::vector<Scenes::Mesh::Vertex> verts{};
		//std::vector<unsigned int> indices{};

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

		_vertexBuffer = new Buffer(_logicalDevice,
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

		_indexBuffer = new Buffer(_logicalDevice,
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
		vkCmdCopyBuffer(copyCommandBuffer, vertexTransferBuffer._handle, _vertexBuffer->_handle, 1, &copyRegion);
		copyRegion.size = faceIndicesSize;
		vkCmdCopyBuffer(copyCommandBuffer, indexTransferBuffer._handle, _indexBuffer->_handle, 1, &copyRegion);

		vkEndCommandBuffer(copyCommandBuffer);

		// Submit to queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;

		vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(_graphicsQueue);

		vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &copyCommandBuffer);
#pragma endregion

		vertexTransferBuffer.Destroy();
		indexTransferBuffer.Destroy();

		std::cout << "set up vertex and index buffers" << std::endl;
	}

	void VulkanApplication::UpdateShaderData()
	{
		_uniformBufferData.objectToWorld = _model._transform._matrix;
		_uniformBufferData.worldToCamera = _mainCamera._view._matrix;
		_uniformBufferData.tanHalfHorizontalFov = tan(glm::radians(_mainCamera._horizontalFov / 2.0f));
		_uniformBufferData.aspectRatio = Utils::Converter::Convert<uint32_t, float>(_settings._windowWidth) / Utils::Converter::Convert<uint32_t, float>(_settings._windowHeight);
		_uniformBufferData.nearClipDistance = _mainCamera._nearClippingDistance;
		_uniformBufferData.farClipDistance = _mainCamera._farClippingDistance;

		_uniformBuffer.UpdateData(&_uniformBufferData, (size_t)sizeof(_uniformBufferData));
	}

	// Todo: wrap physical device into its own class.
	// Todo: wrap image into its own class.

	//void VulkanApplication::CreateDepthImage()
	//{
	//	_depthImage = 

	//	//VkImageCreateInfo imageCreateInfo = { };
	//	//imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	//	//imageCreateInfo.pNext = nullptr;
	//	//imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	//	//imageCreateInfo.format = _depthFormat;

	//	//_depthExtent = VkExtent3D{ _swapchainExtent.width, _swapchainExtent.height, 1 };
	//	//imageCreateInfo.extent = _depthExtent;
	//	//imageCreateInfo.mipLevels = 1;
	//	//imageCreateInfo.arrayLayers = 1;
	//	//imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	//	//// Tiling is very important. Tiling describes how the data for the texture is arranged in the GPU. 
	//	//// For improved performance, GPUs do not store images as 2d arrays of pixels, but instead use complex
	//	//// custom formats, unique to the GPU brand and even models. VK_IMAGE_TILING_OPTIMAL tells Vulkan 
	//	//// to let the driver decide how the GPU arranges the memory of the image. If you use VK_IMAGE_TILING_OPTIMAL,
	//	//// it won’t be possible to read the data from CPU or to write it without changing its tiling first 
	//	//// (it’s possible to change the tiling of a texture at any point, but this can be a costly operation). 
	//	//// The other tiling we can care about is VK_IMAGE_TILING_LINEAR, which will store the image as a 2d 
	//	//// array of pixels. While LINEAR tiling will be a lot slower, it will allow the cpu to safely write 
	//	//// and read from that memory.
	//	//imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//	//imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	//	//// Create the image.
	//	//if (vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &_depthImage) != VK_SUCCESS) {
	//	//	std::cout << "Failed creating depth image." << std::endl;
	//	//}

	//	//VkMemoryRequirements reqs;
	//	//vkGetImageMemoryRequirements(_logicalDevice, _depthImage, &reqs);

	//	//VkMemoryAllocateInfo allocInfo{};
	//	//allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	//	//allocInfo.allocationSize = reqs.size;
	//	//allocInfo.memoryTypeIndex = GetMemoryTypeIndex(_deviceMemoryProperties, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//	//VkDeviceMemory mem;
	//	//vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &mem);
	//	//vkBindImageMemory(_logicalDevice, _depthImage, mem, 0);

	//	//VkImageViewCreateInfo imageViewCreateInfo = {};
	//	//imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	//	//imageViewCreateInfo.pNext = nullptr;
	//	//imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	//	//imageViewCreateInfo.image = _depthImage;
	//	//imageViewCreateInfo.format = _depthFormat;
	//	//imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	//	//imageViewCreateInfo.subresourceRange.levelCount = 1;
	//	//imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	//	//imageViewCreateInfo.subresourceRange.layerCount = 1;
	//	//imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	//	//if (vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &_depthImageView) != VK_SUCCESS) {
	//	//	std::cout << "Failed creating depth image view." << std::endl;
	//	//}
	//}

	void VulkanApplication::CreateSwapchain()
	{
		_swapchain = Swapchain(_logicalDevice, _physicalDevice, _windowSurface, nullptr);
	}

	void VulkanApplication::CreateRenderPass()
	{
		// Describes how the render pass is going to use the main color attachment. An attachment is a fancy word for image.
		VkAttachmentDescription colorAttachmentDescription = {};
		colorAttachmentDescription.format = _swapchain._colorImages.size() > 0 ? _swapchain._colorImages[0]._format : VK_FORMAT_UNDEFINED;
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
		depthAttachmentDescription.format = _swapchain._depthImage._format;
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

		if (vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
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
		_vertexBindingDescription.binding = 0;
		_vertexBindingDescription.stride = sizeof(Vertex);
		_vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
		// Object-space positions.
		_vertexAttributeDescriptions.resize(3);
		_vertexAttributeDescriptions[0].location = 0;
		_vertexAttributeDescriptions[0].binding = 0;
		_vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		_vertexAttributeDescriptions[0].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Position);

		// Normals.
		_vertexAttributeDescriptions[1].location = 1;
		_vertexAttributeDescriptions[1].binding = 0;
		_vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		_vertexAttributeDescriptions[1].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Normal);

		// UV coordinates.
		_vertexAttributeDescriptions[2].location = 2;
		_vertexAttributeDescriptions[2].binding = 0;
		_vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		_vertexAttributeDescriptions[2].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::UV);

		// Describe vertex input.
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &_vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)_vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = _vertexAttributeDescriptions.data();

		// Describe input assembly - this allows Vulkan to know how many indices make up a face for the vkCmdDrawIndexed function.
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
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

		// Note: all attachments must have the same values unless a device feature is enabled
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

		// Describe pipeline layout.
		// This describes the mapping between memory and shader resources (descriptor sets), which contain the information you want to send to the shaders.
		// This is for uniform buffers and samplers.
		int descriptorCount = 1;
		CreateDescriptorSetLayout(descriptorCount);
		CreateDescriptorPool(descriptorCount);

		_uniformBuffer = Buffer(_logicalDevice,
			_physicalDevice,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&_uniformBufferData,
			(size_t)sizeof(_uniformBufferData));

		UpdateShaderData();
		CreateDescriptorSets();
		CreatePipelineLayout();

		// Create the graphics pipeline
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
		pipelineCreateInfo.layout = _pipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		// No longer necessary
		vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
	}

	void VulkanApplication::CreateDescriptorPool(const uint32_t& descriptorCount)
	{
		// This describes how many descriptor sets we'll create from this pool for each type.
		VkDescriptorPoolSize typeCount;
		typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCount.descriptorCount = descriptorCount;

		// maxSets is the maximum number of descriptor sets that can be allocated from the pool.
		// poolSizeCount is the number of elements in pPoolSizes.
		// pPoolSizes is a pointer to an array of VkDescriptorPoolSize structures, each containing a descriptor type and number of descriptors of that type to be allocated in the pool.
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = 1;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = &typeCount;

		if (vkCreateDescriptorPool(_logicalDevice, &createInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor pool" << std::endl;
		}
	}

	void VulkanApplication::CreateDescriptorSets()
	{
		// There needs to be one descriptor set per binding point in the shader
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &_descriptorSetLayout;

		if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &_descriptorSet) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set" << std::endl;
		}

		// This creates the actual descriptor.
		auto transformationMatricesDescriptorBuffer = _uniformBuffer.GenerateDescriptor();

		// pBufferInfo is a pointer to the start of an array of descriptors.
		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = _descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &transformationMatricesDescriptorBuffer;
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(_logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	void VulkanApplication::CreateDescriptorSetLayout(const uint32_t& descriptorCount)
	{
		// This tells Vulkan how many descriptors we want to use.
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = descriptorCount;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
		descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCreateInfo.bindingCount = 1;
		descriptorLayoutCreateInfo.pBindings = &layoutBinding;

		if (vkCreateDescriptorSetLayout(_logicalDevice, &descriptorLayoutCreateInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor layout" << std::endl;
		}
	}

	void VulkanApplication::CreatePipelineLayout()
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &_descriptorSetLayout;

		if (vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
			std::cerr << "failed to create pipeline layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created pipeline layout" << std::endl;
		}
	}

	void VulkanApplication::CreateCommandBuffers()
	{
		// Allocate graphics command buffers
		_graphicsCommandBuffers.resize(_swapchain._colorImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)_swapchain._colorImages.size();

		if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _graphicsCommandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "failed to allocate graphics command buffers" << std::endl;
			exit(1);
		}
		else {
			std::cout << "allocated graphics command buffers" << std::endl;
		}

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
		for (size_t i = 0; i < _swapchain._colorImages.size(); i++) {
			vkBeginCommandBuffer(_graphicsCommandBuffers[i], &beginInfo);

			// If present queue family and graphics queue family are different, then a barrier is necessary
			// The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier presentToDrawBarrier = {};
			presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presentToDrawBarrier.srcAccessMask = 0;
			presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			if (_presentQueueFamily != _graphicsQueueFamily) {
				presentToDrawBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				presentToDrawBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}
			else {
				presentToDrawBarrier.srcQueueFamilyIndex = _presentQueueFamily;
				presentToDrawBarrier.dstQueueFamilyIndex = _graphicsQueueFamily;
			}

			presentToDrawBarrier.image = _swapchain._colorImages[i]._imageHandle;
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(_graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

#pragma region RenderPassCommandRecording
			VkClearValue clearColor = {
			{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A
			};

			VkClearValue depthClear;
			depthClear.depthStencil.depth = 1.0f;
			VkClearValue clearValues[] = { clearColor, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = _renderPass;
			renderPassBeginInfo.framebuffer = _swapchain._frameBuffers[i];
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = _swapchain._framebufferSize;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = &clearValues[0];

			vkCmdBeginRenderPass(_graphicsCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindDescriptorSets(_graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet, 0, nullptr);
			vkCmdBindPipeline(_graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(_graphicsCommandBuffers[i], 0, 1, &_vertexBuffer->_handle, &offset);

			// Get the value type of vertex indices. Typically unsigned int (32-bit) but could also be 16-bit.
			VkIndexType indexType = VK_INDEX_TYPE_NONE_KHR;
			if (std::is_same_v<decltype(_model._mesh._faceIndices)::value_type, unsigned short>) {
				indexType = VK_INDEX_TYPE_UINT16;
			}
			if (std::is_same_v<decltype(_model._mesh._faceIndices)::value_type, unsigned int>) {
				indexType = VK_INDEX_TYPE_UINT32;
			}
			vkCmdBindIndexBuffer(_graphicsCommandBuffers[i], _indexBuffer->_handle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(_graphicsCommandBuffers[i], (uint32_t)_model._mesh._faceIndices.size(), 1, 0, 0, 0);
			vkCmdEndRenderPass(_graphicsCommandBuffers[i]);
#pragma endregion

			// If present and graphics queue families differ, then another barrier is required.
			if (_presentQueueFamily != _graphicsQueueFamily) {
				VkImageMemoryBarrier drawToPresentBarrier = {};
				drawToPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				drawToPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				drawToPresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				drawToPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.srcQueueFamilyIndex = _graphicsQueueFamily;
				drawToPresentBarrier.dstQueueFamilyIndex = _presentQueueFamily;
				drawToPresentBarrier.image = _swapchain._colorImages[i]._imageHandle;
				drawToPresentBarrier.subresourceRange = subResourceRange;

				vkCmdPipelineBarrier(_graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &drawToPresentBarrier);
			}

			if (vkEndCommandBuffer(_graphicsCommandBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
			}
		}

		std::cout << "recorded command buffers" << std::endl;

		// No longer needed
		vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
	}
}

int main()
{
	auto& globalSettings = Settings::GlobalSettings::Instance();
	globalSettings.Load(Settings::Paths::_settings());
	Engine::Vulkan::VulkanApplication app;
	app.Run();

	return 0;
}
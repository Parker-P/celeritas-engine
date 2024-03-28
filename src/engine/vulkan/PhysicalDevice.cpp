// STL.
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

#include "engine/vulkan/PhysicalDevice.hpp"

namespace Engine::Vulkan
{
	VkDeviceMemory PhysicalDevice::AllocateMemory(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlagBits& memoryType)
	{
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = GetMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, memoryType);

		VkDeviceMemory handleToAllocatedMemory;

		if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &handleToAllocatedMemory) != VK_SUCCESS) {
			std::cout << "failed allocating memory of size " << allocInfo.allocationSize << std::endl;
			std::exit(-1);
		}

		return handleToAllocatedMemory;
	}

	bool PhysicalDevice::SupportsSwapchains(VkPhysicalDevice& physicalDevice)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "physical device doesn't support any extensions" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, deviceExtensions.data());

		for (const auto& extension : deviceExtensions) {
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				std::cout << "physical device supports swap chains" << std::endl;
				return true;
			}
		}

		return false;
	}

	bool PhysicalDevice::SupportsSurface(VkPhysicalDevice& physicalDevice, int& queueFamilyIndex, VkSurfaceKHR& surface)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &presentSupport);
		return presentSupport == 0 ? false : true;
	}

	VkPhysicalDeviceMemoryProperties PhysicalDevice::GetMemoryProperties(VkPhysicalDevice& physicalDevice)
	{
		VkPhysicalDeviceMemoryProperties props;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
		return props;
	}

	uint32_t PhysicalDevice::GetMemoryTypeIndex(VkPhysicalDevice& physicalDevice, uint32_t typeBits, VkMemoryPropertyFlagBits properties)
	{
		auto props = GetMemoryProperties(physicalDevice);

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

	std::vector<VkQueueFamilyProperties> PhysicalDevice::GetAllQueueFamilyProperties(VkPhysicalDevice& physicalDevice)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		return queueFamilies;
	}

	void PhysicalDevice::GetQueueFamilyIndices(VkPhysicalDevice& physicalDevice, const VkQueueFlagBits& queueFlags, bool needsPresentationSupport, const VkSurfaceKHR& surface)
	{
		// Check queue families
		auto queueFamilyProperties = GetAllQueueFamilyProperties(physicalDevice);

		std::vector<uint32_t> queueFamilyIndices;

		for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
			VkBool32 presentationSupported = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupported);

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

	VkSurfaceCapabilitiesKHR PhysicalDevice::GetSurfaceCapabilities(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &surfaceCapabilities) != VK_SUCCESS) {
			std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
		}
		return surfaceCapabilities;
	}

	std::vector<VkSurfaceFormatKHR> PhysicalDevice::GetSupportedFormatsForSurface(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
		uint32_t formatCount;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
			std::cerr << "failed to get number of supported surface formats" << std::endl;
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported surface formats" << std::endl;
		}

		return surfaceFormats;
	}

	std::vector<VkPresentModeKHR> PhysicalDevice::GetSupportedPresentModesForSurface(VkPhysicalDevice& physicalDevice, VkSurfaceKHR& windowSurface) {
		uint32_t presentModeCount;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
			std::cerr << "failed to get number of supported presentation modes" << std::endl;
			exit(1);
		}

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported presentation modes" << std::endl;
			exit(1);
		}

		return presentModes;
	}
}
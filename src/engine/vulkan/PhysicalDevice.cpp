// STL.
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

#include "engine/vulkan/PhysicalDevice.hpp"

namespace Engine::Vulkan
{
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
}
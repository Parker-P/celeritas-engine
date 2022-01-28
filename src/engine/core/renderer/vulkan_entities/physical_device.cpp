#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "queue.h"
#include "instance.h"
#include "window_surface.h"
#include "physical_device.h"

//Set device count to 1 to force Vulkan to use the first device we found
namespace Engine::Core::VulkanEntities {

	void PhysicalDevice::FindQueueFamilies(WindowSurface& window_surface) {
		//Once a physical device has been selected, the device needs to be queried for available queues.
		//In Vulkan, all commands need to be executed on queues. Each device makes a set of queues available 
		//which can execute certain operations such as compute, rendering, presenting, and so on.
		//Queues that share certain properties, for instance those used to execute the same type of operation,
		//are grouped together into queue families. In order to use queues, a queue family which supports the desired 
		//operations of the application needs to be selected. For example, if the application needs to render anything, 
		//a queue family which supports rendering operations should be selected.

		//Check queue families
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
		if (queue_family_count == 0) {
			std::cout << "physical device has no queue families!" << std::endl;
			exit(1);
		}

		//Now we need to find the queue family that has graphics support and the queue family used to present images to the screen.
		//To do that we first query vulkan to get the available queue families for the physical device
		std::vector<VkQueueFamilyProperties> available_queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, available_queue_families.data());
		std::cout << "physical device has " << queue_family_count << " queue families" << std::endl;

		//Then we loop through all the queue families and look for the needed queue families
		bool found_graphics_queue_family = false;
		bool found_present_queue_family = false;
		for (int queueIndex = 0; queueIndex < queue_family_count; ++queueIndex) {
			VkBool32 present_support = false;

			//In doing it we also check if the considered queue family supports commands to present to the screen
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, queueIndex, window_surface.GetWindowSurface(), &present_support);
			if (available_queue_families[queueIndex].queueCount > 0 && available_queue_families[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphics_queue_.SetQueueFamily(queueIndex);
				found_graphics_queue_family = true;
				if (present_support) {
					present_queue_.SetQueueFamily(queueIndex);
					found_present_queue_family = true;
					break;
				}
			}
			if (!found_present_queue_family && present_support) {
				present_queue_.SetQueueFamily(queueIndex);
				found_present_queue_family = true;
			}
		}
		if (found_graphics_queue_family) {
			std::cout << "queue family #" << graphics_queue_.GetQueueFamily() << " supports graphics" << std::endl;

			if (found_present_queue_family) {
				std::cout << "queue family #" << present_queue_.GetQueueFamily() << " supports presentation" << std::endl;
			}
			else {
				std::cerr << "could not find a valid queue family with present support" << std::endl;
				exit(1);
			}
		}
		else {
			std::cerr << "could not find a valid queue family with graphics support" << std::endl;
			exit(1);
		}
	}

	void PhysicalDevice::SelectPhysicalDevice(Instance& instance, WindowSurface& surface) {

		//Try to find 1 Vulkan supported device
		uint32_t device_count = 0;
		if (vkEnumeratePhysicalDevices(instance.GetVulkanInstance(), &device_count, nullptr) != VK_SUCCESS || device_count == 0) {
			std::cerr << "failed to get number of physical devices" << std::endl;
			exit(1);
		}

		//Get the list of GPUs installed on the system
		device_count = 1;
		VkResult res = vkEnumeratePhysicalDevices(instance.GetVulkanInstance(), &device_count, &physical_device_);
		if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
			std::cerr << "enumerating physical devices failed!" << std::endl;
			exit(1);
		}
		if (device_count == 0) {
			std::cerr << "no physical devices that support vulkan!" << std::endl;
			exit(1);
		}
		std::cout << "physical device with vulkan support found" << std::endl;

		//Check device features and properties
		vkGetPhysicalDeviceProperties(physical_device_, &device_properties_);
		vkGetPhysicalDeviceFeatures(physical_device_, &device_features_);
		std::cout << "using physical device \"" << device_properties_.deviceName << "\"" << std::endl;
		uint32_t supported_version[] = {
			VK_VERSION_MAJOR(device_properties_.apiVersion),
			VK_VERSION_MINOR(device_properties_.apiVersion),
			VK_VERSION_PATCH(device_properties_.apiVersion)
		};
		std::cout << "physical device supports version " << supported_version[0] << "." << supported_version[1] << "." << supported_version[2] << std::endl;

		//Check which extensions the physical device supports. An extension is simply an optional component that can be used to accomplish something
		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);
		if (extension_count == 0) {
			std::cerr << "physical device doesn't support any extensions" << std::endl;
			exit(1);
		}

		//Check if the physical device supports swap chains
		device_extensions_.resize(extension_count);
		vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, device_extensions_.data());
		for (const auto& extension : device_extensions_) {
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				std::cout << "physical device supports swap chains" << std::endl;
				return;
			}
		}
		std::cerr << "physical device doesn't support swap chains" << std::endl;

		//Find the queue families needed for rendering and store their IDs in member vaiables
		FindQueueFamilies(surface);
	}

	VkPhysicalDevice PhysicalDevice::GetPhysicalDevice() {
		return physical_device_;
	}

	VkPhysicalDeviceProperties PhysicalDevice::GetDeviceProperties() {
		return device_properties_;
	}

	VkPhysicalDeviceMemoryProperties PhysicalDevice::GetDeviceMemoryProperties()
	{
		return device_memory_properties_;
	}

	VkPhysicalDeviceFeatures PhysicalDevice::GetDeviceFeatures() {
		return device_features_;
	}

	std::vector<VkExtensionProperties> PhysicalDevice::GetDeviceExtensions() {
		return device_extensions_;
	}

	void PhysicalDevice::SetGraphicsQueue(Queue graphics_queue) {
		graphics_queue_ = graphics_queue;
	}

	void PhysicalDevice::SetPresentQueue(Queue present_queue) {
		present_queue_ = present_queue;
	}

	Queue PhysicalDevice::GetGraphicsQueue() {
		return graphics_queue_;
	}

	Queue PhysicalDevice::GetPresentQueue() {
		return present_queue_;
	}

	void PhysicalDevice::SetDeviceMemoryProperties(VkPhysicalDeviceMemoryProperties device_memory_properties) {
		device_memory_properties_ = device_memory_properties;
	}
}
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

#include "../src/engine/core/app_config.h"
#include "instance.h"
#include "queue.h"
#include "window_surface.h"
#include "physical_device.h"
#include "logical_device.h"

namespace Engine::Core::VulkanEntities {
	void LogicalDevice::CreateLogicalDevice(PhysicalDevice& physical_device, AppConfig& app_config) {
		//After the physical device and queue family have been selected, they can be used to generate a logical device handle.
		//A logical device is the main interface between the GPU (physical device) and the application, and is required for the 
		//creation of most objects. It can be used to create the queues that will be used to render and present images.
		//A logical device is often referenced when calling the creation function of many objects.

		//Prepare the info for queue creation
		float queue_priority = 1.0f;
		VkDeviceQueueCreateInfo queue_create_info[2] = {};
		queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info[0].queueFamilyIndex = physical_device.GetGraphicsQueue().GetQueueFamily();
		queue_create_info[0].queueCount = 1;
		queue_create_info[0].pQueuePriorities = &queue_priority;
		queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info[1].queueFamilyIndex = physical_device.GetPresentQueue().GetQueueFamily();
		queue_create_info[1].queueCount = 1;
		queue_create_info[1].pQueuePriorities = &queue_priority;

		//Create logical device from physical device using the two queues defined above
		VkDeviceCreateInfo device_create_info = {};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.pQueueCreateInfos = queue_create_info;
		if (physical_device.GetGraphicsQueue().GetQueueFamily() == physical_device.GetPresentQueue().GetQueueFamily()) {
			device_create_info.queueCreateInfoCount = 1;
		}
		else {
			device_create_info.queueCreateInfoCount = 2;
		}

		//Declare which device extensions we want enabled
		VkPhysicalDeviceFeatures enabled_features = {};
		enabled_features.shaderClipDistance = VK_TRUE;
		enabled_features.shaderCullDistance = VK_TRUE;
		const char* device_extensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		device_create_info.enabledExtensionCount = 1;
		device_create_info.ppEnabledExtensionNames = &device_extensions;
		device_create_info.pEnabledFeatures = &enabled_features;
		if (app_config.debugInfo_.enable_debugging) {
			device_create_info.enabledLayerCount = 1;
			device_create_info.ppEnabledLayerNames = &app_config.debugInfo_.debug_layer;
		}

		//Create the device
		if (vkCreateDevice(physical_device.GetPhysicalDevice(), &device_create_info, nullptr, &logical_device_) != VK_SUCCESS) {
			std::cerr << "failed to create logical device" << std::endl;
			exit(1);
		}
		std::cout << "created logical device" << std::endl;

		//Get graphics and presentation queues (which may be the same because they could belong to the same family since 
		//the queue family the graphics queue belongs to isn't necessarily different from the one that the present queue belongs to
		//because they could contain commands that have the same queue family support list)
		VkQueue graphics_queue = physical_device.GetGraphicsQueue().GetQueue();
		VkQueue present_queue = physical_device.GetPresentQueue().GetQueue();
		vkGetDeviceQueue(logical_device_, physical_device.GetGraphicsQueue().GetQueueFamily(), 0, &graphics_queue);
		vkGetDeviceQueue(logical_device_, physical_device.GetPresentQueue().GetQueueFamily(), 0, &present_queue);
		physical_device.GetGraphicsQueue().SetQueue(graphics_queue);
		physical_device.GetPresentQueue().SetQueue(present_queue);
		std::cout << "acquired graphics and presentation queues" << std::endl;

		//Get physical device memory properties (VRAM) so we have the information to do memory management and set it in the physical device
		VkPhysicalDeviceMemoryProperties device_memory_properties;
		vkGetPhysicalDeviceMemoryProperties(physical_device.GetPhysicalDevice(), &device_memory_properties);
		physical_device.SetDeviceMemoryProperties(device_memory_properties);
	}

	VkDevice LogicalDevice::GetLogicalDevice() {
		return logical_device_;
	}
}
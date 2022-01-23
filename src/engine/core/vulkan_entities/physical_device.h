#pragma once

namespace Engine::Core::VulkanEntities {
	class PhysicalDevice {

		VkPhysicalDevice physical_device_;
		VkPhysicalDeviceProperties device_properties_;
		VkPhysicalDeviceFeatures device_features_;
		std::vector<VkExtensionProperties> device_extensions_;
		Queue graphics_queue_;
		Queue present_queue_;

		//Finds the queue families needed for rendering and stores them in the appropriate member variables
		void FindQueueFamilies(WindowSurface& window_surface);

	public:
		//Finds a physical device and links it do the instance, then gets all the device properties and queue families and stores them in member variables
		void SelectPhysicalDevice(Instance& instance, WindowSurface& surface);

		//Returns the physical device
		VkPhysicalDevice GetPhysicalDevice();

		//Returns the device's properties
		VkPhysicalDeviceProperties GetDeviceProperties();

		//Returns the device's features
		VkPhysicalDeviceFeatures GetDeviceFeatures();

		//Returns the device's supported extensions
		std::vector<VkExtensionProperties> GetDeviceExtensions();

		//Returns the graphics queue family
		VkQueue GetGraphicsQueueFamily();

		//Returns the present queue family
		VkQueue GetPresentQueueFamily();
	};
}
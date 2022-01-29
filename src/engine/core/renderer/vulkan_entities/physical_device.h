#pragma once

namespace Engine::Core::Renderer::VulkanEntities {

	//This is the handle to the actual physical graphics card. The physical device handles the queues and GPU-local memory (VRAM)
	class PhysicalDevice {
		VkPhysicalDevice physical_device_;
		VkPhysicalDeviceProperties device_properties_;
		VkPhysicalDeviceMemoryProperties device_memory_properties_;
		VkPhysicalDeviceFeatures device_features_;
		std::vector<VkExtensionProperties> device_extensions_;
		Queue graphics_queue_;
		Queue present_queue_;

		//Finds the queue families needed for rendering and stores them in the appropriate member variables
		void FindQueueFamilies(WindowSurface& window_surface);

	public:
		//Finds a physical device and links it do the instance, then gets all the device properties and queue families and stores them in member variables
		void SelectPhysicalDevice(Instance& instance, WindowSurface& surface);

		VkPhysicalDevice GetPhysicalDevice();
		VkPhysicalDeviceProperties GetDeviceProperties();
		VkPhysicalDeviceMemoryProperties GetDeviceMemoryProperties();
		VkPhysicalDeviceFeatures GetDeviceFeatures();
		std::vector<VkExtensionProperties> GetDeviceExtensions();
		Queue GetGraphicsQueue();
		Queue GetPresentQueue();
		void SetDeviceMemoryProperties(VkPhysicalDeviceMemoryProperties device_memory_properties);
		void SetGraphicsQueue(Queue graphics_queue);
		void SetPresentQueue(Queue present_queue);
	};
}
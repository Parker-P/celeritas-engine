#pragma once

namespace Engine::Core::VulkanEntities {
	class WindowSurface {
		GLFWwindow* window_;
		VkSurfaceKHR window_surface_;
		VkSurfaceCapabilitiesKHR surface_capabilities_;
		VkSurfaceFormatKHR surface_format_;
	public:
		//Creates the window surface and links it to the instance
		void CreateWindowSurface(Instance& instance, PhysicalDevice& physical_device, GLFWwindow* window);

		GLFWwindow* GetWindow();

		//Returns the window surface. Please create a window surface first using CreateWindowSurface
		VkSurfaceKHR GetWindowSurface();

		VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();

		VkSurfaceFormatKHR GetSurfaceFormat();

		void SetSurfaceFormat(VkSurfaceFormatKHR surface_format);
	};
}
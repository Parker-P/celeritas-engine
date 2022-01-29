#pragma once

namespace Engine::Core::Renderer::VulkanEntities {

	//This is the object that acts as an interface between the glfw window (in our case) and the swap chain. A window surface is an extension, meaning that it's an optional object that contains pieces of code that enable you to do something that is not native to Vulkan.
	class WindowSurface {
		GLFWwindow* window_;
		VkSurfaceKHR window_surface_;
		VkSurfaceCapabilitiesKHR surface_capabilities_;
	public:
		//Creates the window surface and links it to the instance
		void CreateWindowSurface(Instance& instance, PhysicalDevice& physical_device, GLFWwindow* window);

		GLFWwindow* GetWindow();

		//Returns the window surface. Please create a window surface first using CreateWindowSurface
		VkSurfaceKHR GetWindowSurface();

		VkSurfaceCapabilitiesKHR GetSurfaceCapabilities();
	};
}
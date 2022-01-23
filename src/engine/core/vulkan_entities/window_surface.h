#pragma once

namespace Engine::Core::VulkanEntities {
	class WindowSurface {
		VkSurfaceKHR window_surface_;
	public:
		//Creates the window surface and links it to the instance
		void CreateWindowSurface(Instance& instance, GLFWwindow* window);

		//Returns the window surface. Please create a window surface first using CreateWindowSurface
		VkSurfaceKHR GetWindowSurface();
	};
}
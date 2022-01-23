#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "instance.h"
#include "window_surface.h"

namespace Engine::Core::VulkanEntities {
	void WindowSurface::CreateWindowSurface(Instance& instance, GLFWwindow* window) {
		if (glfwCreateWindowSurface(instance.GetVulkanInstance(), window, NULL, &window_surface_) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}
		std::cout << "created window surface" << std::endl;
	}

	VkSurfaceKHR WindowSurface::GetWindowSurface() {
		return window_surface_;
	}
}
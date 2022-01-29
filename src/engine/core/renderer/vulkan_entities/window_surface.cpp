#include <iostream>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "physical_device.h"
#include "instance.h"
#include "window_surface.h"

namespace Engine::Core::Renderer::VulkanEntities {
	void WindowSurface::CreateWindowSurface(Instance& instance, PhysicalDevice& physical_device, GLFWwindow* window) {
		if (glfwCreateWindowSurface(instance.GetVulkanInstance(), window, NULL, &window_surface_) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}
		std::cout << "created window surface" << std::endl;

		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.GetPhysicalDevice(), window_surface_, &surface_capabilities_) != VK_SUCCESS) {
			std::cerr << "failed to acquire window surface capabilities" << std::endl;
			exit(1);
		}
	}

	GLFWwindow* WindowSurface::GetWindow() {
		return window_;
	}

	VkSurfaceKHR WindowSurface::GetWindowSurface() {
		return window_surface_;
	}

	VkSurfaceCapabilitiesKHR WindowSurface::GetSurfaceCapabilities() {
		return surface_capabilities_;
	}
}
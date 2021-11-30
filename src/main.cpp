#include <iostream>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr;
VkApplicationInfo g_vkAppInfo{};
VkInstance g_vkInstance;

int main() {
	//Initialize the window with GLFW API
	glfwInit();
	g_window = glfwCreateWindow(1024, 768, "Celeritas engine", nullptr, nullptr);
	glfwMakeContextCurrent(g_window);

	//Initialize Vulkan using its API
	g_vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	g_vkAppInfo.pApplicationName = "Solar System Explorer";
	g_vkAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	g_vkAppInfo.pEngineName = "Celeritas Engine";
	g_vkAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	g_vkAppInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &g_vkAppInfo;
	vkCreateInstance(&createInfo, nullptr, &g_vkInstance);
	
	//Main application loop
	while (!glfwWindowShouldClose(g_window)) {
		glfwPollEvents();
		std::cout << "Hello world" << std::endl;
	}

	//Post window close operations
	glfwDestroyWindow(g_window);
	glfwTerminate();
	return 0;
}
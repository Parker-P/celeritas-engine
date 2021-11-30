#include <iostream>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr;

int main() {
	glfwInit();
	g_window = glfwCreateWindow(1024, 768, "Celeritas engine", nullptr, nullptr);
	glfwMakeContextCurrent(g_window);
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::cout << extensionCount << " extensions supported\n";
	while (!glfwWindowShouldClose(g_window)) {
		glfwPollEvents();
		std::cout << "Hello world" << std::endl;
	}
	glfwDestroyWindow(g_window);
	glfwTerminate();
	return 0;
}
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr; //The main application window
VkInstance g_vkInstance{}; //The vulkan instance used to load the API
VkPhysicalDevice g_physicalDevice{}; //The physical graphics card handle
VkDevice g_logicalDevice{}; //The logical graphics card handle. This is what we use to communicate with the GPU
VkQueue g_graphicsQueue; //The queue used to send graphics commands to the GPU (actual rendering commands)
VkQueue g_presentQueue; //The queue used to send presentation commands to the GPU (commands to interface with the window)
VkSurfaceKHR g_surface; //The window surface used by vulkan to communicate with the glfw window
VkSwapchainKHR g_swapChain; //The swapchain object. A swapchain is a queue of images waiting to be presented to the screen via framebuffer

int main() {
	//Initialize the window with GLFW API
	glfwInit();
	g_window = glfwCreateWindow(1024, 768, "Celeritas engine", nullptr, nullptr);

	//Initialize Vulkan
	VkApplicationInfo vkAppInfo{};
	vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkAppInfo.pApplicationName = "Solar System Explorer";
	vkAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkAppInfo.pEngineName = "Celeritas Engine";
	vkAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	vkAppInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &vkAppInfo;
	vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance);

	//Create a window surface. A window surface is a way that vulkan uses to communicate with
	//the window where it's going to render things
	VkWin32SurfaceCreateInfoKHR windowSurfaceCreateInfo{};
	windowSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	windowSurfaceCreateInfo.hwnd = glfwGetWin32Window(g_window);
	windowSurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	vkCreateWin32SurfaceKHR(g_vkInstance, &windowSurfaceCreateInfo, nullptr, &g_surface);
	glfwCreateWindowSurface(g_vkInstance, g_window, nullptr, &g_surface);

	//Check for graphics card support and if found assign it to g_physicalDevice
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		std::cout << "failed to find GPUs with Vulkan support!" << std::endl;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices.data());
	for (auto device : devices) {
		g_physicalDevice = device;
	}
	if (nullptr != g_physicalDevice) {
		std::cout << "found your graphics card" << std::endl;
	}

	//Find what queue families are supported by the graphics card.
	//Queue families are queues that are used to organize the order of instruction processing for the GPU.
	//They act like channels of communication with the GPU.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, queueFamilies.data());
	int graphicsQueueFamilyIndex = 0;
	int presentQueueFamilyIndex = 0;
	for (int i = 0; i < queueFamilies.size(); ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			std::cout << "found the graphics bit queue family" << std::endl;
			graphicsQueueFamilyIndex = i; break;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(g_physicalDevice, i, g_surface, &presentSupport);
		if (presentSupport) {
			presentQueueFamilyIndex = i;
		}
	}

	//Initialize logical device
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;
	VkDeviceCreateInfo logicalDeviceCreateInfo{};
	logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logicalDeviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	logicalDeviceCreateInfo.queueCreateInfoCount = 1;
	logicalDeviceCreateInfo.enabledExtensionCount = 1;
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	if (vkCreateDevice(g_physicalDevice, &logicalDeviceCreateInfo, nullptr, &g_logicalDevice) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}
	vkGetDeviceQueue(g_logicalDevice, graphicsQueueFamilyIndex, 0, &g_graphicsQueue);
	vkGetDeviceQueue(g_logicalDevice, presentQueueFamilyIndex, 0, &g_presentQueue);

	//Create the swapchain. The swapchain is a queue of images that are waiting
	//to end up in the frame buffer to then be shown on screen
	//Gather info about what swapchain is supported
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physicalDevice, g_surface, &surfaceCapabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, nullptr);
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, nullptr);
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = g_surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	vkCreateSwapchainKHR(g_logicalDevice, &swapChainCreateInfo, nullptr, &g_swapChain);

	//Main loop
	while (!glfwWindowShouldClose(g_window)) {
		std::cout << "Main loop" << std::endl;
		glfwPollEvents();
	}

	//Shutdown operations (cleanup)
	vkDestroyDevice(g_logicalDevice, nullptr);
	vkDestroySurfaceKHR(g_vkInstance, g_surface, nullptr);
	vkDestroyInstance(g_vkInstance, nullptr);
	glfwDestroyWindow(g_window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
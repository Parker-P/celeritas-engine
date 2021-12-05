#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <algorithm>
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
std::vector<VkImageView> g_swapChainImageViews; //Image views for the swap chain images. Image views describe how to access images in the swap chain
std::vector<VkImage> g_swapChainImages; //Container for the images in the swap chain queue

int main() {
	//Initialize the window with GLFW API
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance) != VK_SUCCESS) {
		std::cout << "failed to create vulkan instance" << std::endl;
	}

	//Create a window surface. A window surface is a way that vulkan uses to communicate with
	//the window where it's going to render things
	VkWin32SurfaceCreateInfoKHR windowSurfaceCreateInfo{};
	windowSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	windowSurfaceCreateInfo.hwnd = glfwGetWin32Window(g_window);
	windowSurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	//std::cout << vkCreateWin32SurfaceKHR(g_vkInstance, &windowSurfaceCreateInfo, nullptr, &g_surface) << std::endl;
	if(glfwCreateWindowSurface(g_vkInstance, g_window, nullptr, &g_surface) != VK_SUCCESS) {
		std::cout << "failed to create vulkan window surface" << std::endl;
	};

	//Check for graphics card support and if found assign it to g_physicalDevice
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		std::cout << "failed to find GPUs with vulkan support!" << std::endl;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(g_vkInstance, &deviceCount, devices.data());
	for (auto device : devices) {
		g_physicalDevice = device;
	}

	//Find what queue families are supported by the graphics card.
	//Queue families are queues that are used to organize the order of instruction processing for the GPU.
	//They act like channels of communication with the GPU.
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &queueFamilyCount, queueFamilies.data());
	uint32_t graphicsQueueFamilyIndex = 0;
	uint32_t presentQueueFamilyIndex = 0;
	for (int i = 0; i < queueFamilies.size(); ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
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
	std::vector<VkSurfaceFormatKHR> formats;
	if (formatCount != 0) {
		formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_physicalDevice, g_surface, &formatCount, formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes;
	if (presentModeCount != 0) {
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_physicalDevice, g_surface, &presentModeCount, presentModes.data());
	}
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = g_surface;
	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
		imageCount = surfaceCapabilities.maxImageCount;
	}
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = formats[0].format;
	swapChainCreateInfo.imageColorSpace = formats[0].colorSpace;
	int width, height;
	glfwGetFramebufferSize(g_window, &width, &height);
	VkExtent2D actualExtent;
	actualExtent.width = std::clamp(static_cast<uint32_t>(width), surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(static_cast<uint32_t>(height), surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	swapChainCreateInfo.imageExtent = actualExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
	if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(g_logicalDevice, &swapChainCreateInfo, nullptr, &g_swapChain) != VK_SUCCESS) {
		std::cout << "failed to create swapchain" << std::endl;
	}
	vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &imageCount, nullptr);
	g_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(g_logicalDevice, g_swapChain, &imageCount, g_swapChainImages.data());

	//Create an image view. An image view describes how to access an image and which 
	//part of the image to access in the swap chain. For example if it should be treated 
	//as a 2D texture depth texture without any mipmapping levels.
	for (int i = 0; i < g_swapChainImages.size(); ++i) {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		std::vector<VkImageView> g_swapChainImageViews;
		imageViewCreateInfo.image = g_swapChainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = formats[0].format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		g_swapChainImageViews.resize(g_swapChainImages.size());
		if (vkCreateImageView(g_logicalDevice, &imageViewCreateInfo, nullptr, &g_swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image views!");
		}
	}

	//Main loop
	while (!glfwWindowShouldClose(g_window)) {
		std::cout << "Main loop" << std::endl;
		glfwPollEvents();
	}

	//Shutdown operations (cleanup)
	for (auto imageView : g_swapChainImageViews) {
		vkDestroyImageView(g_logicalDevice, imageView, nullptr);
	}
	vkDestroyDevice(g_logicalDevice, nullptr);
	vkDestroySurfaceKHR(g_vkInstance, g_surface, nullptr);
	vkDestroyInstance(g_vkInstance, nullptr);
	glfwDestroyWindow(g_window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
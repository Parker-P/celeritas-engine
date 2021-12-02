#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

GLFWwindow* g_window = nullptr; //The main application window
VkApplicationInfo g_vkAppInfo{}; //The vulkan app information container
VkInstance g_vkInstance{}; //The vulkan instance used to load the API
VkPhysicalDevice g_physicalDevice{}; //The physical graphics card handle
VkDevice g_logicalDevice{}; //The logical graphics card handle. This is what we use to communicate with the GPU

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
	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &g_vkAppInfo;
	vkCreateInstance(&instanceCreateInfo, nullptr, &g_vkInstance);

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
	//We need to find the index of the family queue that has the VK_QUEUE_GRAPHICS_BIT flag as we only want to perform graphics operations for now
	int graphicsQueueFamilyIndex=0;
	for (int i = 0; i < queueFamilies.size(); ++i) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			std::cout << "found the graphics bit queue family" << std::endl;
			graphicsQueueFamilyIndex = i; break;
		}
	}

	//Create the logical device in order to be able to communicate with the GPU
	VkPhysicalDeviceFeatures deviceFeatures{};
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.enabledLayerCount = 0;
	if (vkCreateDevice(g_physicalDevice, &deviceCreateInfo, nullptr, &g_logicalDevice) != VK_SUCCESS) {
		std::cout << "failed to create logical device" << std::endl;
	}
	else { std::cout << "logical device created successfully" << std::endl; }

	//Main application loop
	while (!glfwWindowShouldClose(g_window)) {
		glfwPollEvents();
		//std::cout << "Hello world" << std::endl;
	}

	//Post window close operations
	vkDestroyInstance(g_vkInstance, nullptr);
	vkDestroyDevice(g_logicalDevice, nullptr);
	glfwDestroyWindow(g_window);
	glfwTerminate();
	return 0;
}
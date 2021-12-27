#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "vulkan_application.h"
#include "vulkan_factory.h"

VkBool32 DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData) {
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		std::cerr << "ERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		std::cerr << "WARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
	}
	return VK_FALSE;
}

// Vertex layout
struct Vertex {
	float pos[3];
	float color[3];
};

void VulkanApplication::Run() {
	time_start_ = std::chrono::high_resolution_clock::now();
	WindowInit();
	SetupVulkan();
	MainLoop();
	Cleanup(true);
}

void VulkanApplication::WindowInit() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_ = glfwCreateWindow(width_, height_, name_, nullptr, nullptr);
	glfwSetWindowSizeCallback(window_, VulkanApplication::OnWindowResized);
}

void VulkanApplication::SetupVulkan() {
	old_swap_chain_ = VK_NULL_HANDLE;
	CreateInstance();
	CreateDebugCallback();
	CreateWindowSurface();
	FindPhysicalDevice();
	CheckSwapChainSupport();
	FindQueueFamilies();
	CreateLogicalDevice();
	CreateSemaphores();
	CreateCommandPool();
	CreateVertexBuffer();
	CreateUniformBuffer();
	CreateSwapChain();
	CreateRenderPass();
	CreateImageViews();
	CreateFramebuffers();
	CreateGraphicsPipeline();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateCommandBuffers();
}

void VulkanApplication::MainLoop() {
	while (!glfwWindowShouldClose(window_)) {
		UpdateUniformData();
		Draw();
		glfwPollEvents();
	}
}

void VulkanApplication::OnWindowResized(GLFWwindow* window, int width, int height) {
	VulkanFactory::GetInstance().GetApplicationByGlfwWindow(window)->window_resized_ = true;
}

void VulkanApplication::OnWindowSizeChanged() {
	window_resized_ = false;

	// Only recreate objects that are affected by framebuffer size changes
	Cleanup(false);

	CreateSwapChain();
	CreateRenderPass();
	CreateImageViews();
	CreateFramebuffers();
	CreateGraphicsPipeline();
	CreateCommandBuffers();
}

void VulkanApplication::Cleanup(bool fullClean) {
	vkDeviceWaitIdle(logical_device_);
	vkFreeCommandBuffers(logical_device_, command_pool_, (uint32_t)graphics_command_buffers_.size(), graphics_command_buffers_.data());
	vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
	vkDestroyRenderPass(logical_device_, render_pass_, nullptr);
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		vkDestroyFramebuffer(logical_device_, swap_chain_frame_buffers_[i], nullptr);
		vkDestroyImageView(logical_device_, swap_chain_image_views_[i], nullptr);
	}
	vkDestroyDescriptorSetLayout(logical_device_, descriptor_set_layout_, nullptr);
	if (fullClean) {
		vkDestroySemaphore(logical_device_, image_available_semaphore_, nullptr);
		vkDestroySemaphore(logical_device_, rendering_finished_semaphore_, nullptr);
		vkDestroyCommandPool(logical_device_, command_pool_, nullptr);

		// Clean up uniform buffer related objects
		vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
		vkDestroyBuffer(logical_device_, uniform_buffer_, nullptr);
		vkFreeMemory(logical_device_, uniform_buffer_memory_, nullptr);

		// Buffers must be destroyed after no command buffers are referring to them anymore
		vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
		vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(logical_device_, index_buffer_, nullptr);
		vkFreeMemory(logical_device_, index_buffer_memory_, nullptr);

		// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
		vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
		vkDestroyDevice(logical_device_, nullptr);
		vkDestroySurfaceKHR(instance_, window_surface_, nullptr);
		if (kEnableDebugging_) {
			PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT");
			DestroyDebugReportCallback(instance_, callback_, nullptr);
		}
		vkDestroyInstance(instance_, nullptr);
	}
}

void VulkanApplication::CreateInstance() {
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Solar System Explorer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Celeritas Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	// Get instance extensions required by GLFW to draw to window
	unsigned int glfwExtensionCount;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions;
	for (size_t i = 0; i < glfwExtensionCount; i++) {
		extensions.push_back(glfwExtensions[i]);
	}

	if (kEnableDebugging_) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	// Check for extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	if (extensionCount == 0) {
		std::cerr << "no extensions supported!" << std::endl;
		exit(1);
	}

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

	std::cout << "supported extensions:" << std::endl;

	for (const auto& extension : availableExtensions) {
		std::cout << "\t" << extension.extensionName << std::endl;
	}

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (kEnableDebugging_) {
		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = &kDebugLayer_;
	}

	// Initialize Vulkan instance
	if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
		std::cerr << "failed to create instance!" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created vulkan instance" << std::endl;
	}
}

void VulkanApplication::CreateWindowSurface() {
	if (glfwCreateWindowSurface(instance_, window_, NULL, &window_surface_) != VK_SUCCESS) {
		std::cerr << "failed to create window surface!" << std::endl;
		exit(1);
	}

	std::cout << "created window surface" << std::endl;
}

void VulkanApplication::FindPhysicalDevice() {
	// Try to find 1 Vulkan supported device
	// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions
	uint32_t deviceCount = 0;
	if (vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
		std::cerr << "failed to get number of physical devices" << std::endl;
		exit(1);
	}

	deviceCount = 1;
	VkResult res = vkEnumeratePhysicalDevices(instance_, &deviceCount, &physical_device_);
	if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
		std::cerr << "enumerating physical devices failed!" << std::endl;
		exit(1);
	}

	if (deviceCount == 0) {
		std::cerr << "no physical devices that support vulkan!" << std::endl;
		exit(1);
	}

	std::cout << "physical device with vulkan support found" << std::endl;

	// Check device features
	// Note: will apiVersion >= appInfo.apiVersion? Probably yes, but spec is unclear.
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(physical_device_, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physical_device_, &deviceFeatures);

	uint32_t supportedVersion[] = {
		VK_VERSION_MAJOR(deviceProperties.apiVersion),
		VK_VERSION_MINOR(deviceProperties.apiVersion),
		VK_VERSION_PATCH(deviceProperties.apiVersion)
	};

	std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;
}

void VulkanApplication::CheckSwapChainSupport() {
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0) {
		std::cerr << "physical device doesn't support any extensions" << std::endl;
		exit(1);
	}

	std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extensionCount, deviceExtensions.data());

	for (const auto& extension : deviceExtensions) {
		if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
			std::cout << "physical device supports swap chains" << std::endl;
			return;
		}
	}

	std::cerr << "physical device doesn't support swap chains" << std::endl;
	exit(1);
}

void VulkanApplication::FindQueueFamilies() {
	// Check queue families
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queueFamilyCount, nullptr);

	if (queueFamilyCount == 0) {
		std::cout << "physical device has no queue families!" << std::endl;
		exit(1);
	}

	// Find queue family with graphics support
	// Note: is a transfer queue necessary to copy vertices to the gpu or can a graphics queue handle that?
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queueFamilyCount, queueFamilies.data());

	std::cout << "physical device has " << queueFamilyCount << " queue families" << std::endl;

	bool foundGraphicsQueueFamily = false;
	bool foundPresentQueueFamily = false;

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, window_surface_, &presentSupport);

		if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_queue_family_ = i;
			foundGraphicsQueueFamily = true;

			if (presentSupport) {
				present_queue_family_ = i;
				foundPresentQueueFamily = true;
				break;
			}
		}

		if (!foundPresentQueueFamily && presentSupport) {
			present_queue_family_ = i;
			foundPresentQueueFamily = true;
		}
	}

	if (foundGraphicsQueueFamily) {
		std::cout << "queue family #" << graphics_queue_family_ << " supports graphics" << std::endl;

		if (foundPresentQueueFamily) {
			std::cout << "queue family #" << present_queue_family_ << " supports presentation" << std::endl;
		}
		else {
			std::cerr << "could not find a valid queue family with present support" << std::endl;
			exit(1);
		}
	}
	else {
		std::cerr << "could not find a valid queue family with graphics support" << std::endl;
		exit(1);
	}
}

void VulkanApplication::CreateLogicalDevice() {
	// Greate one graphics queue and optionally a separate presentation queue
	float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo queueCreateInfo[2] = {};

	queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo[0].queueFamilyIndex = graphics_queue_family_;
	queueCreateInfo[0].queueCount = 1;
	queueCreateInfo[0].pQueuePriorities = &queuePriority;

	queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo[0].queueFamilyIndex = present_queue_family_;
	queueCreateInfo[0].queueCount = 1;
	queueCreateInfo[0].pQueuePriorities = &queuePriority;

	// Create logical device from physical device
	// Note: there are separate instance and device extensions!
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;

	if (graphics_queue_family_ == present_queue_family_) {
		deviceCreateInfo.queueCreateInfoCount = 1;
	}
	else {
		deviceCreateInfo.queueCreateInfoCount = 2;
	}

	// Necessary for shader (for some reason)
	VkPhysicalDeviceFeatures enabledFeatures = {};
	enabledFeatures.shaderClipDistance = VK_TRUE;
	enabledFeatures.shaderCullDistance = VK_TRUE;

	const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	if (kEnableDebugging_) {
		deviceCreateInfo.enabledLayerCount = 1;
		deviceCreateInfo.ppEnabledLayerNames = &kDebugLayer_;
	}

	if (vkCreateDevice(physical_device_, &deviceCreateInfo, nullptr, &logical_device_) != VK_SUCCESS) {
		std::cerr << "failed to create logical device" << std::endl;
		exit(1);
	}

	std::cout << "created logical device" << std::endl;

	// Get graphics and presentation queues (which may be the same)
	vkGetDeviceQueue(logical_device_, graphics_queue_family_, 0, &graphics_queue_);
	vkGetDeviceQueue(logical_device_, present_queue_family_, 0, &present_queue_);

	std::cout << "acquired graphics and presentation queues" << std::endl;

	vkGetPhysicalDeviceMemoryProperties(physical_device_, &device_memory_properties_);
}

void VulkanApplication::CreateDebugCallback() {
	if (kEnableDebugging_) {
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

		PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");

		if (CreateDebugReportCallback(instance_, &createInfo, nullptr, &callback_) != VK_SUCCESS) {
			std::cerr << "failed to create debug callback" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created debug callback" << std::endl;
		}
	}
	else {
		std::cout << "skipped creating debug callback" << std::endl;
	}
}

void VulkanApplication::CreateSemaphores() {
	VkSemaphoreCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(logical_device_, &createInfo, nullptr, &image_available_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(logical_device_, &createInfo, nullptr, &rendering_finished_semaphore_) != VK_SUCCESS) {
		std::cerr << "failed to create semaphores" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created semaphores" << std::endl;
	}
}

void VulkanApplication::CreateCommandPool() {
	// Create graphics command pool
	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = graphics_queue_family_;

	if (vkCreateCommandPool(logical_device_, &poolCreateInfo, nullptr, &command_pool_) != VK_SUCCESS) {
		std::cerr << "failed to create command queue for graphics queue family" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created command pool for graphics queue family" << std::endl;
	}
}

void VulkanApplication::CreateVertexBuffer() {
	// Setup vertices
	std::vector<Vertex> vertices = {
		{ { -0.5f, -0.5f,  0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { -0.5f,  0.5f,  0.0f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.5f,  0.5f,  0.0f }, { 0.0f, 0.0f, 1.0f } }
	};
	uint32_t verticesSize = (uint32_t)(vertices.size() * sizeof(vertices[0]));

	// Setup indices
	std::vector<uint32_t> indices = { 0, 1, 2 };
	uint32_t indicesSize = (uint32_t)(indices.size() * sizeof(indices[0]));

	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;
	void* data;

	struct StagingBuffer {
		VkDeviceMemory memory;
		VkBuffer buffer;
	};

	struct {
		StagingBuffer vertices;
		StagingBuffer indices;
	} stagingBuffers;

	// Allocate command buffer for copy operation
	VkCommandBufferAllocateInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufInfo.commandPool = command_pool_;
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;

	VkCommandBuffer copyCommandBuffer;
	vkAllocateCommandBuffers(logical_device_, &cmdBufInfo, &copyCommandBuffer);

	// First copy vertices to host accessible vertex buffer memory
	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = verticesSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	vkCreateBuffer(logical_device_, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer);

	vkGetBufferMemoryRequirements(logical_device_, stagingBuffers.vertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &memAlloc, nullptr, &stagingBuffers.vertices.memory);

	vkMapMemory(logical_device_, stagingBuffers.vertices.memory, 0, verticesSize, 0, &data);
	memcpy(data, vertices.data(), verticesSize);
	vkUnmapMemory(logical_device_, stagingBuffers.vertices.memory);
	vkBindBufferMemory(logical_device_, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0);

	// Then allocate a gpu only buffer for vertices
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(logical_device_, &vertexBufferInfo, nullptr, &vertex_buffer_);
	vkGetBufferMemoryRequirements(logical_device_, vertex_buffer_, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &memAlloc, nullptr, &vertex_buffer_memory_);
	vkBindBufferMemory(logical_device_, vertex_buffer_, vertex_buffer_memory_, 0);

	// Next copy indices to host accessible index buffer memory
	VkBufferCreateInfo indexBufferInfo = {};
	indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexBufferInfo.size = indicesSize;
	indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	vkCreateBuffer(logical_device_, &indexBufferInfo, nullptr, &stagingBuffers.indices.buffer);
	vkGetBufferMemoryRequirements(logical_device_, stagingBuffers.indices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &memAlloc, nullptr, &stagingBuffers.indices.memory);
	vkMapMemory(logical_device_, stagingBuffers.indices.memory, 0, indicesSize, 0, &data);
	memcpy(data, indices.data(), indicesSize);
	vkUnmapMemory(logical_device_, stagingBuffers.indices.memory);
	vkBindBufferMemory(logical_device_, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0);

	// And allocate another gpu only buffer for indices
	indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(logical_device_, &indexBufferInfo, nullptr, &index_buffer_);
	vkGetBufferMemoryRequirements(logical_device_, index_buffer_, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &memAlloc, nullptr, &index_buffer_memory_);
	vkBindBufferMemory(logical_device_, index_buffer_, index_buffer_memory_, 0);

	// Now copy data from host visible buffer to gpu only buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(copyCommandBuffer, &bufferBeginInfo);

	VkBufferCopy copyRegion = {};
	copyRegion.size = verticesSize;
	vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.vertices.buffer, vertex_buffer_, 1, &copyRegion);
	copyRegion.size = indicesSize;
	vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.indices.buffer, index_buffer_, 1, &copyRegion);

	vkEndCommandBuffer(copyCommandBuffer);

	// Submit to queue
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCommandBuffer;

	vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue_);

	vkFreeCommandBuffers(logical_device_, command_pool_, 1, &copyCommandBuffer);

	vkDestroyBuffer(logical_device_, stagingBuffers.vertices.buffer, nullptr);
	vkFreeMemory(logical_device_, stagingBuffers.vertices.memory, nullptr);
	vkDestroyBuffer(logical_device_, stagingBuffers.indices.buffer, nullptr);
	vkFreeMemory(logical_device_, stagingBuffers.indices.memory, nullptr);

	std::cout << "set up vertex and index buffers" << std::endl;

	// Binding and attribute descriptions
	vertex_binding_description_.binding = 0;
	vertex_binding_description_.stride = sizeof(vertices[0]);
	vertex_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// vec2 position
	vertex_attribute_descriptions_.resize(2);
	vertex_attribute_descriptions_[0].binding = 0;
	vertex_attribute_descriptions_[0].location = 0;
	vertex_attribute_descriptions_[0].format = VK_FORMAT_R32G32B32_SFLOAT;

	// vec3 color
	vertex_attribute_descriptions_[1].binding = 0;
	vertex_attribute_descriptions_[1].location = 1;
	vertex_attribute_descriptions_[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertex_attribute_descriptions_[1].offset = sizeof(float) * 3;
}

void VulkanApplication::CreateUniformBuffer() {
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(UniformBufferData);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	vkCreateBuffer(logical_device_, &bufferInfo, nullptr, &uniform_buffer_);

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(logical_device_, uniform_buffer_, &memReqs);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &allocInfo.memoryTypeIndex);

	vkAllocateMemory(logical_device_, &allocInfo, nullptr, &uniform_buffer_memory_);
	vkBindBufferMemory(logical_device_, uniform_buffer_, uniform_buffer_memory_, 0);

	UpdateUniformData();
}

void VulkanApplication::UpdateUniformData() {
	// Rotate based on time
	auto timeNow = std::chrono::high_resolution_clock::now();
	long long millis = std::chrono::duration_cast<std::chrono::milliseconds>(time_start_ - timeNow).count();
	float angle = (millis % 4000) / 4000.0f * glm::radians(360.f);

	// Set up model transformation matrix
	glm::mat4 modelMatrix;
	modelMatrix = glm::rotate(modelMatrix, angle, glm::vec3(0, 0, 1));

	// Set up view transformation matrix
	auto viewMatrix = glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));

	// Set up projection transformation matrix
	auto projMatrix = glm::perspective(glm::radians(70.f), (float)swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f, 10.0f);
	UniformBufferData.transformation_matrix = projMatrix * viewMatrix * modelMatrix;

	void* data;
	vkMapMemory(logical_device_, uniform_buffer_memory_, 0, sizeof(UniformBufferData), 0, &data);
	memcpy(data, &UniformBufferData, sizeof(UniformBufferData));
	vkUnmapMemory(logical_device_, uniform_buffer_memory_);
}

// Find device memory that is supported by the requirements (typeBits) and meets the desired properties
VkBool32 VulkanApplication::GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex) {
	for (uint32_t i = 0; i < 32; i++) {
		if ((typeBits & 1) == 1) {
			if ((device_memory_properties_.memoryTypes[i].propertyFlags & properties) == properties) {
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	return false;
}

void VulkanApplication::CreateSwapChain() {
	// Find surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, window_surface_, &surfaceCapabilities) != VK_SUCCESS) {
		std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
		exit(1);
	}

	// Find supported surface formats
	uint32_t formatCount;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, window_surface_, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
		std::cerr << "failed to get number of supported surface formats" << std::endl;
		exit(1);
	}

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, window_surface_, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
		std::cerr << "failed to get supported surface formats" << std::endl;
		exit(1);
	}

	// Find supported present modes
	uint32_t presentModeCount;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, window_surface_, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
		std::cerr << "failed to get number of supported presentation modes" << std::endl;
		exit(1);
	}

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, window_surface_, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
		std::cerr << "failed to get supported presentation modes" << std::endl;
		exit(1);
	}

	// Determine number of images for swap chain
	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
		imageCount = surfaceCapabilities.maxImageCount;
	}

	std::cout << "using " << imageCount << " images for swap chain" << std::endl;

	// Select a surface format
	VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);

	// Select swap chain size
	swap_chain_extent_ = ChooseSwapExtent(surfaceCapabilities);

	// Determine transformation to use (preferring no transform)
	VkSurfaceTransformFlagBitsKHR surfaceTransform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else {
		surfaceTransform = surfaceCapabilities.currentTransform;
	}

	// Choose presentation mode (preferring MAILBOX ~= triple buffering)
	VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

	// Finally, create the swap chain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = window_surface_;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swap_chain_extent_;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.preTransform = surfaceTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = old_swap_chain_;

	if (vkCreateSwapchainKHR(logical_device_, &createInfo, nullptr, &swap_chain_) != VK_SUCCESS) {
		std::cerr << "failed to create swap chain" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created swap chain" << std::endl;
	}

	if (old_swap_chain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(logical_device_, old_swap_chain_, nullptr);
	}
	old_swap_chain_ = swap_chain_;

	swap_chain_format_ = surfaceFormat.format;

	// Store the images used by the swap chain
	// Note: these are the images that swap chain image indices refer to
	// Note: actual number of images may differ from requested number, since it's a lower bound
	uint32_t actualImageCount = 0;
	if (vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
		std::cerr << "failed to acquire number of swap chain images" << std::endl;
		exit(1);
	}

	swap_chain_images_.resize(actualImageCount);

	if (vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &actualImageCount, swap_chain_images_.data()) != VK_SUCCESS) {
		std::cerr << "failed to acquire swap chain images" << std::endl;
		exit(1);
	}

	std::cout << "acquired swap chain images" << std::endl;
}

VkSurfaceFormatKHR VulkanApplication::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// We can either choose any format
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	// Or go with the standard format - if available
	for (const auto& availableSurfaceFormat : availableFormats) {
		if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM) {
			return availableSurfaceFormat;
		}
	}

	// Or fall back to the first available one
	return availableFormats[0];
}

VkExtent2D VulkanApplication::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
	if (surfaceCapabilities.currentExtent.width == -1) {
		VkExtent2D swapChainExtent = {};

		swapChainExtent.width = std::min(std::max(width_, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
		swapChainExtent.height = std::min(std::max(height_, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);

		return swapChainExtent;
	}
	else {
		return surfaceCapabilities.currentExtent;
	}
}

VkPresentModeKHR VulkanApplication::ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes) {
	for (const auto& presentMode : presentModes) {
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return presentMode;
		}
	}

	// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
	return VK_PRESENT_MODE_FIFO_KHR;
}

void VulkanApplication::CreateRenderPass() {
	VkAttachmentDescription attachmentDescription = {};
	attachmentDescription.format = swap_chain_format_;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Note: hardware will automatically transition attachment to the specified layout
	// Note: index refers to attachment descriptions array
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Note: this is a description of how the attachments of the render pass will be used in this sub pass
	// e.g. if they will be read in shaders and/or drawn to
	VkSubpassDescription subPassDescription = {};
	subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPassDescription.colorAttachmentCount = 1;
	subPassDescription.pColorAttachments = &colorAttachmentReference;

	// Create the render pass
	VkRenderPassCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &attachmentDescription;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subPassDescription;

	if (vkCreateRenderPass(logical_device_, &createInfo, nullptr, &render_pass_) != VK_SUCCESS) {
		std::cerr << "failed to create render pass" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created render pass" << std::endl;
	}
}

void VulkanApplication::CreateImageViews() {
	swap_chain_image_views_.resize(swap_chain_images_.size());

	// Create an image view for every image in the swap chain
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swap_chain_images_[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swap_chain_format_;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(logical_device_, &createInfo, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS) {
			std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
			exit(1);
		}
	}

	std::cout << "created image views for swap chain images" << std::endl;
}

void VulkanApplication::CreateFramebuffers() {
	swap_chain_frame_buffers_.resize(swap_chain_images_.size());

	// Note: Framebuffer is basically a specific choice of attachments for a render pass
	// That means all attachments must have the same dimensions, interesting restriction
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = render_pass_;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &swap_chain_image_views_[i];
		createInfo.width = swap_chain_extent_.width;
		createInfo.height = swap_chain_extent_.height;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(logical_device_, &createInfo, nullptr, &swap_chain_frame_buffers_[i]) != VK_SUCCESS) {
			std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
			exit(1);
		}
	}

	std::cout << "created framebuffers for swap chain image views" << std::endl;
}

VkShaderModule VulkanApplication::CreateShaderModule(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	std::vector<char> fileBytes(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read(fileBytes.data(), fileBytes.size());
	file.close();

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = fileBytes.size();
	createInfo.pCode = (uint32_t*)fileBytes.data();

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(logical_device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		std::cerr << "failed to create shader module for " << filename << std::endl;
		exit(1);
	}

	std::cout << "created shader module for " << filename << std::endl;

	return shaderModule;
}

void VulkanApplication::CreateGraphicsPipeline() {
	//Compile and load the shaders
	system((std::string("start \"\" ") + kShaderPath_ + std::string("shader_compiler.bat\"")).c_str());
	VkShaderModule vertexShaderModule = CreateShaderModule(kShaderPath_ + std::string("vertex_shader.spv\""));
	VkShaderModule fragmentShaderModule = CreateShaderModule(kShaderPath_ + std::string("fragment_shader.spv\""));

	// Set up shader stage info
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

	// Describe vertex input
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &vertex_binding_description_;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 2;
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertex_attribute_descriptions_.data();

	// Describe input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	// Describe viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swap_chain_extent_.width;
	viewport.height = (float)swap_chain_extent_.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = swap_chain_extent_.width;
	scissor.extent.height = swap_chain_extent_.height;

	// Note: scissor test is always enabled (although dynamic scissor is possible)
	// Number of viewports must match number of scissors
	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	// Describe rasterization
	// Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features)
	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
	rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationCreateInfo.depthBiasClamp = 0.0f;
	rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationCreateInfo.lineWidth = 1.0f;

	// Describe multisampling
	// Note: using multisampling also requires turning on device features
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.minSampleShading = 1.0f;
	multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

	// Describing color blending
	// Note: all paramaters except blendEnable and colorWriteMask are irrelevant here
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	// Note: all attachments must have the same values unless a device feature is enabled
	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
	colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;
	colorBlendCreateInfo.blendConstants[0] = 0.0f;
	colorBlendCreateInfo.blendConstants[1] = 0.0f;
	colorBlendCreateInfo.blendConstants[2] = 0.0f;
	colorBlendCreateInfo.blendConstants[3] = 0.0f;

	// Describe pipeline layout
	// Note: this describes the mapping between memory and shader resources (descriptor sets)
	// This is for uniform buffers and samplers
	VkDescriptorSetLayoutBinding layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
	descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayoutCreateInfo.bindingCount = 1;
	descriptorLayoutCreateInfo.pBindings = &layoutBinding;
	if (vkCreateDescriptorSetLayout(logical_device_, &descriptorLayoutCreateInfo, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor layout" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created descriptor layout" << std::endl;
	}
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	layoutCreateInfo.pSetLayouts = &descriptor_set_layout_;
	if (vkCreatePipelineLayout(logical_device_, &layoutCreateInfo, nullptr, &pipeline_layout_) != VK_SUCCESS) {
		std::cerr << "failed to create pipeline layout" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created pipeline layout" << std::endl;
	}

	// Create the graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
	pipelineCreateInfo.layout = pipeline_layout_;
	pipelineCreateInfo.renderPass = render_pass_;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(logical_device_, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphics_pipeline_) != VK_SUCCESS) {
		std::cerr << "failed to create graphics pipeline" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created graphics pipeline" << std::endl;
	}

	// No longer necessary
	vkDestroyShaderModule(logical_device_, vertexShaderModule, nullptr);
	vkDestroyShaderModule(logical_device_, fragmentShaderModule, nullptr);
}

void VulkanApplication::CreateDescriptorPool() {
	// This describes how many descriptor sets we'll create from this pool for each type
	VkDescriptorPoolSize typeCount;
	typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	typeCount.descriptorCount = 1;

	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 1;
	createInfo.pPoolSizes = &typeCount;
	createInfo.maxSets = 1;

	if (vkCreateDescriptorPool(logical_device_, &createInfo, nullptr, &descriptor_pool_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor pool" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created descriptor pool" << std::endl;
	}
}

void VulkanApplication::CreateDescriptorSet() {
	// There needs to be one descriptor set per binding point in the shader
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptor_pool_;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptor_set_layout_;

	if (vkAllocateDescriptorSets(logical_device_, &allocInfo, &descriptor_set_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor set" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created descriptor set" << std::endl;
	}

	// Update descriptor set with uniform binding
	VkDescriptorBufferInfo descriptorBufferInfo = {};
	descriptorBufferInfo.buffer = uniform_buffer_;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = sizeof(UniformBufferData);

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = descriptor_set_;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
	writeDescriptorSet.dstBinding = 0;

	vkUpdateDescriptorSets(logical_device_, 1, &writeDescriptorSet, 0, nullptr);
}

void VulkanApplication::CreateCommandBuffers() {
	// Allocate graphics command buffers
	graphics_command_buffers_.resize(swap_chain_images_.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = command_pool_;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)swap_chain_images_.size();

	if (vkAllocateCommandBuffers(logical_device_, &allocInfo, graphics_command_buffers_.data()) != VK_SUCCESS) {
		std::cerr << "failed to allocate graphics command buffers" << std::endl;
		exit(1);
	}
	else {
		std::cout << "allocated graphics command buffers" << std::endl;
	}

	// Prepare data for recording command buffers
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = 1;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = 1;

	VkClearValue clearColor = {
		{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A
	};

	// Record command buffer for each swap image
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		vkBeginCommandBuffer(graphics_command_buffers_[i], &beginInfo);

		// If present queue family and graphics queue family are different, then a barrier is necessary
		// The barrier is also needed initially to transition the image to the present layout
		VkImageMemoryBarrier presentToDrawBarrier = {};
		presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		presentToDrawBarrier.srcAccessMask = 0;
		presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		if (present_queue_family_ != graphics_queue_family_) {
			presentToDrawBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			presentToDrawBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else {
			presentToDrawBarrier.srcQueueFamilyIndex = present_queue_family_;
			presentToDrawBarrier.dstQueueFamilyIndex = graphics_queue_family_;
		}

		presentToDrawBarrier.image = swap_chain_images_[i];
		presentToDrawBarrier.subresourceRange = subResourceRange;

		vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = render_pass_;
		renderPassBeginInfo.framebuffer = swap_chain_frame_buffers_[i];
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent = swap_chain_extent_;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(graphics_command_buffers_[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindDescriptorSets(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

		vkCmdBindPipeline(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(graphics_command_buffers_[i], 0, 1, &vertex_buffer_, &offset);

		vkCmdBindIndexBuffer(graphics_command_buffers_[i], index_buffer_, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(graphics_command_buffers_[i], 3, 1, 0, 0, 0);

		vkCmdEndRenderPass(graphics_command_buffers_[i]);

		// If present and graphics queue families differ, then another barrier is required
		if (present_queue_family_ != graphics_queue_family_) {
			VkImageMemoryBarrier drawToPresentBarrier = {};
			drawToPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			drawToPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			drawToPresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			drawToPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			drawToPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			drawToPresentBarrier.srcQueueFamilyIndex = graphics_queue_family_;
			drawToPresentBarrier.dstQueueFamilyIndex = present_queue_family_;
			drawToPresentBarrier.image = swap_chain_images_[i];
			drawToPresentBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &drawToPresentBarrier);
		}

		if (vkEndCommandBuffer(graphics_command_buffers_[i]) != VK_SUCCESS) {
			std::cerr << "failed to record command buffer" << std::endl;
			exit(1);
		}
	}

	std::cout << "recorded command buffers" << std::endl;

	// No longer needed
	vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
}

void VulkanApplication::Draw() {
	// Acquire image
	uint32_t imageIndex;
	VkResult res = vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &imageIndex);

	// Unless surface is out of date right now, defer swap chain recreation until end of this frame
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		OnWindowSizeChanged();
		return;
	}
	else if (res != VK_SUCCESS) {
		std::cerr << "failed to acquire image" << std::endl;
		exit(1);
	}

	// Wait for image to be available and draw
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &image_available_semaphore_;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &rendering_finished_semaphore_;

	// This is the stage where the queue should wait on the semaphore
	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	submitInfo.pWaitDstStageMask = &waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &graphics_command_buffers_[imageIndex];
	if (vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		std::cerr << "failed to submit draw command buffer" << std::endl;
		exit(1);
	}

	// Present drawn image
	// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &rendering_finished_semaphore_;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swap_chain_;
	presentInfo.pImageIndices = &imageIndex;
	res = vkQueuePresentKHR(present_queue_, &presentInfo);
	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || window_resized_) {
		OnWindowSizeChanged();
	}
	else if (res != VK_SUCCESS) {
		std::cerr << "failed to submit present command buffer" << std::endl;
		exit(1);
	}
}
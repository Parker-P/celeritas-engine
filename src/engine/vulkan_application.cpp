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
	CreateInstance();
	CreateDebugCallback();
	CreateWindowSurface();
	FindPhysicalDevice();
	CheckSwapChainSupport();
	FindQueueFamilies();
	CreateLogicalDevice();
	CreateSemaphores();
	CreateCommandPool();
	CreateVertexAndIndexBuffers();
	CreateUniformBuffer();
	CreateSwapChain();
	CreateRenderPass();
	CreateImageViews();
	CreateFramebuffers();
	CreateGraphicsPipeline();
	CreateDescriptorPool();
	CreateDescriptorSets();
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

		//Clean up uniform buffer related objects
		vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
		vkDestroyBuffer(logical_device_, uniform_buffer_, nullptr);
		vkFreeMemory(logical_device_, uniform_buffer_memory_, nullptr);

		//Buffers must be destroyed after no command buffers are referring to them anymore
		vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
		vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(logical_device_, index_buffer_, nullptr);
		vkFreeMemory(logical_device_, index_buffer_memory_, nullptr);

		//Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
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
	//Add meta information to the Vulkan application
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = name_;
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Celeritas Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	//Get instance extensions required by GLFW to draw to window. Extensions are just features (pieces of code)
	//that the instance (in this case) provides. For example the VK_KHR_surface extension enables us to use
	//surfaces. If you recall, surfaces are just a connection between the swapchain and glfw (in this case) and
	//we need it in order to send images from the swapchain to the glfw window.
	unsigned int glfw_extension_count;
	const char** glfw_extensions;
	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	std::vector<const char*> extensions;
	for (size_t i = 0; i < glfw_extension_count; i++) {
		extensions.push_back(glfw_extensions[i]);
	}
	if (kEnableDebugging_) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	//Check for extensions
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	if (extension_count == 0) {
		std::cerr << "no extensions supported!" << std::endl;
		exit(1);
	}
	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());
	std::cout << "supported extensions:" << std::endl;
	for (const auto& extension : available_extensions) {
		std::cout << "\t" << extension.extensionName << std::endl;
	}

	//Create the vulkan instance and declare which extensions we want to use
	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = (uint32_t)extensions.size();
	create_info.ppEnabledExtensionNames = extensions.data();
	if (kEnableDebugging_) {
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &kDebugLayer_;
	}
	if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
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
	//After creating an instance, the application needs to select a physical device.
	//A physical device represents a piece of hardware installed in the system, most commonly a single GPU.
	//The application can retrieve a list of the available physical devicesand select the one which has the 
	//properties and features which most closely matches the application's requirements. 
	//These properties and features can be simply things like whether the device is integrated or discrete, 
	//or something more complex like the maximum size of the pool of push constant memory.

	//Try to find 1 Vulkan supported device
	uint32_t device_count = 0;
	if (vkEnumeratePhysicalDevices(instance_, &device_count, nullptr) != VK_SUCCESS || device_count == 0) {
		std::cerr << "failed to get number of physical devices" << std::endl;
		exit(1);
	}

	//Set device count to 1 to force Vulkan to use the first device we found
	device_count = 1;

	//Get the list of GPUs
	VkResult res = vkEnumeratePhysicalDevices(instance_, &device_count, &physical_device_);
	if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
		std::cerr << "enumerating physical devices failed!" << std::endl;
		exit(1);
	}
	if (device_count == 0) {
		std::cerr << "no physical devices that support vulkan!" << std::endl;
		exit(1);
	}
	std::cout << "physical device with vulkan support found" << std::endl;

	//Check device features and properties
	VkPhysicalDeviceProperties device_properties;
	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceProperties(physical_device_, &device_properties);
	vkGetPhysicalDeviceFeatures(physical_device_, &device_features);
	uint32_t supported_version[] = {
		VK_VERSION_MAJOR(device_properties.apiVersion),
		VK_VERSION_MINOR(device_properties.apiVersion),
		VK_VERSION_PATCH(device_properties.apiVersion)
	};
	std::cout << "physical device supports version " << supported_version[0] << "." << supported_version[1] << "." << supported_version[2] << std::endl;
}

void VulkanApplication::CheckSwapChainSupport() {
	//Check how many extensions the physical device supports. An extension is simply a feature, a piece of code, that the graphics driver provides.
	//There are device extensions which are extensions (code features) that pertain to the behaviour of a device and there are
	//instance extensions which pertain to the behaviour of the instance. For example the VK_KHR_surface extension that enables us
	//to connect the swapchain to the glfw window is an instance extension, whereas the swapchain is a concept that is tightly related
	//to the graphics pipeline so it is a device extension.
	uint32_t extension_count = 0;
	vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);
	if (extension_count == 0) {
		std::cerr << "physical device doesn't support any extensions" << std::endl;
		exit(1);
	}

	//Check if the physical device supports swap chains
	std::vector<VkExtensionProperties> device_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, device_extensions.data());
	for (const auto& extension : device_extensions) {
		if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
			std::cout << "physical device supports swap chains" << std::endl;
			return;
		}
	}
	std::cerr << "physical device doesn't support swap chains" << std::endl;
	exit(1);
}

void VulkanApplication::FindQueueFamilies() {
	//Once a physical device has been selected, the device needs to be queried for available queues.
	//In Vulkan, all commands need to be executed on queues. Each device makes a set of queues available 
	//which can execute certain operations such as compute, rendering, presenting, and so on.
	//Queues that share certain properties, for instance those used to execute the same type of operation,
	//are grouped together into queue families. In order to use queues, a queue family which supports the desired 
	//operations of the application needs to be selected. For example, if the application needs to render anything, 
	//a queue family which supports rendering operations should be selected.

	// Check queue families
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
	if (queue_family_count == 0) {
		std::cout << "physical device has no queue families!" << std::endl;
		exit(1);
	}

	// Find queue family with graphics support
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());
	std::cout << "physical device has " << queue_family_count << " queue families" << std::endl;
	bool found_graphics_queue_family = false;
	bool found_present_queue_family = false;
	for (int i = 0; i < queue_family_count; ++i) {
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, window_surface_, &present_support);
		if (queue_families[i].queueCount > 0 && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_queue_family_ = i;
			found_graphics_queue_family = true;
			if (present_support) {
				present_queue_family_ = i;
				found_present_queue_family = true;
				break;
			}
		}
		if (!found_present_queue_family && present_support) {
			present_queue_family_ = i;
			found_present_queue_family = true;
		}
	}
	if (found_graphics_queue_family) {
		std::cout << "queue family #" << graphics_queue_family_ << " supports graphics" << std::endl;

		if (found_present_queue_family) {
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
	//After the physical deviceand queue family have been selected, they can be used to generate a logical device handle.
	//A logical device is the main interface between the GPU(physical device) and the application and is required for the 
	//creation of most objects. It can be used to create the queues that will be used to render and present images.
	//A logical device is often referenced when calling the creation function of many objects.

	//Prepare the info for queue creation
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queue_create_info[2] = {};
	queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info[0].queueFamilyIndex = graphics_queue_family_;
	queue_create_info[0].queueCount = 1;
	queue_create_info[0].pQueuePriorities = &queuePriority;
	queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info[1].queueFamilyIndex = present_queue_family_;
	queue_create_info[1].queueCount = 1;
	queue_create_info[1].pQueuePriorities = &queuePriority;

	//Create logical device from physical device using the two queues defined above
	VkDeviceCreateInfo device_create_info = {};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pQueueCreateInfos = queue_create_info;
	if (graphics_queue_family_ == present_queue_family_) {
		device_create_info.queueCreateInfoCount = 1;
	}
	else {
		device_create_info.queueCreateInfoCount = 2;
	}

	//Declare which device extensions we want enabled
	VkPhysicalDeviceFeatures enabled_features = {};
	enabled_features.shaderClipDistance = VK_TRUE;
	enabled_features.shaderCullDistance = VK_TRUE;
	const char* device_extensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	device_create_info.enabledExtensionCount = 1;
	device_create_info.ppEnabledExtensionNames = &device_extensions;
	device_create_info.pEnabledFeatures = &enabled_features;
	if (kEnableDebugging_) {
		device_create_info.enabledLayerCount = 1;
		device_create_info.ppEnabledLayerNames = &kDebugLayer_;
	}

	//Create the device
	if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &logical_device_) != VK_SUCCESS) {
		std::cerr << "failed to create logical device" << std::endl;
		exit(1);
	}
	std::cout << "created logical device" << std::endl;

	//Get graphics and presentation queues (which may be the same because they could belong to the same family since 
	//graphics_queue_family_ isn't necessarily different from present_queue_family_)
	vkGetDeviceQueue(logical_device_, graphics_queue_family_, 0, &graphics_queue_);
	vkGetDeviceQueue(logical_device_, present_queue_family_, 0, &present_queue_);
	std::cout << "acquired graphics and presentation queues" << std::endl;

	//Get physical device memory properties (VRAM) so we have the information to do memory management
	vkGetPhysicalDeviceMemoryProperties(physical_device_, &device_memory_properties_);
}

void VulkanApplication::CreateDebugCallback() {
	if (kEnableDebugging_) {
		VkDebugReportCallbackCreateInfoEXT create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		create_info.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
		create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT");
		if (CreateDebugReportCallback(instance_, &create_info, nullptr, &callback_) != VK_SUCCESS) {
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
	VkSemaphoreCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(logical_device_, &create_info, nullptr, &image_available_semaphore_) != VK_SUCCESS ||
		vkCreateSemaphore(logical_device_, &create_info, nullptr, &rendering_finished_semaphore_) != VK_SUCCESS) {
		std::cerr << "failed to create semaphores" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created semaphores" << std::endl;
	}
}

void VulkanApplication::CreateCommandPool() {
	//Create graphics command pool for the graphics queue family since we want to send commands on that queue
	VkCommandPoolCreateInfo pool_create_info = {};
	pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_create_info.queueFamilyIndex = graphics_queue_family_;
	if (vkCreateCommandPool(logical_device_, &pool_create_info, nullptr, &command_pool_) != VK_SUCCESS) {
		std::cerr << "failed to create command queue for graphics queue family" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created command pool for graphics queue family" << std::endl;
	}
}

void VulkanApplication::CreateVertexAndIndexBuffers() {
	//In this function we copy vertex and face information to the GPU.
	//With a GPU we have two types of memory: RAM (on the motherboard) and VRAM (on the GPU).
	//The memory this program uses to allocate variables is the RAM because the instructions are processed by the CPU
	//which uses the RAM. Our goal is to have the GPU read vertex information, but for it to be as fast as possible
	//we want it to be reading from the VRAM because it sits inside the GPU and would be much quicker to read, so our goal 
	//is to transfer the vertex information from here (the RAM) to the VRAM once so it's much quicker to read multiple times.
	//The procedure to copy data to the VRAM is quite
	//complicated because of the nature of how the GPU works. What we need to do is:
	//1) Allocate some memory on the VRAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
	//This staging buffer is a temporary location that we use to expose to the GPU the data we want to send it
	//2) Copy the vertex information to the allocated memory (to the staging buffer)
	//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
	//and the destination to where we will copy the data we previously copied to the staging buffer
	//4) Create a command buffer and fill it with instructions to copy data from the staging buffer to the VRAM
	//5) Submit the command buffer to a queue for it to be processed by the GPU

	//Setup vertices. Vulkan's normalized viewport coordinate system is very weird: +Y points down, +X points to the right, 
	//+Z points towards you. The origin is at the exact center of the viewport. Very unintuitive.
	std::vector<Vertex> vertices = {
		{ 0.0, 0.0, -0.0f },
		{ 0.75, 0.25, -0.0f },
		{ 0.75, -0.25, -0.0f },
		{ 0.25, -0.25, -0.0f }
	};
	uint32_t vertices_size = (uint32_t)(vertices.size() * (sizeof(float) * 3));

	//Setup indices (faces)
	std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };
	uint32_t indices_size = (uint32_t)(indices.size() * (sizeof(int) * 3));

	//This tells the GPU how to read vertex data
	vertex_binding_description_.binding = 0;
	vertex_binding_description_.stride = sizeof(vertices[0]);
	vertex_binding_description_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	//This tells the GPU how to connect shader variables and vertex data
	vertex_attribute_descriptions_.resize(1);
	vertex_attribute_descriptions_[0].binding = 0;
	vertex_attribute_descriptions_[0].location = 0;
	vertex_attribute_descriptions_[0].format = VK_FORMAT_R32G32B32_SFLOAT;

	//Get memory related variables ready. Note that the HOST_COHERENT_BIT flag allows us to not have to flush
	//to the VRAM once allocating memory, it tells Vulkan to do this automatically
	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.memoryTypeIndex = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryRequirements mem_reqs;

	//Prepare the staging buffer data structure/layout.
	struct StagingBuffer {
		VkDeviceMemory memory;
		VkBuffer buffer;
	};
	struct {
		StagingBuffer vertices;
		StagingBuffer indices;
	} staging_buffers;

	//1) Allocate some memory on the VRAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
	VkBufferCreateInfo vertex_buffer_info = {};
	vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertex_buffer_info.size = vertices_size;
	vertex_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vkCreateBuffer(logical_device_, &vertex_buffer_info, nullptr, &staging_buffers.vertices.buffer);
	vkGetBufferMemoryRequirements(logical_device_, staging_buffers.vertices.buffer, &mem_reqs);
	mem_alloc.allocationSize = mem_reqs.size;
	GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &staging_buffers.vertices.memory);

	//2) Copy the vertex information to the allocated memory (to the staging buffer)
	void* data;
	vkMapMemory(logical_device_, staging_buffers.vertices.memory, 0, vertices_size, 0, &data);
	memcpy(data, vertices.data(), vertices_size);
	vkUnmapMemory(logical_device_, staging_buffers.vertices.memory);
	vkBindBufferMemory(logical_device_, staging_buffers.vertices.buffer, staging_buffers.vertices.memory, 0);

	//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
	vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(logical_device_, &vertex_buffer_info, nullptr, &vertex_buffer_);
	vkGetBufferMemoryRequirements(logical_device_, vertex_buffer_, &mem_reqs);
	mem_alloc.allocationSize = mem_reqs.size;
	GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &vertex_buffer_memory_);
	vkBindBufferMemory(logical_device_, vertex_buffer_, vertex_buffer_memory_, 0);

	//Now we repeat the same steps for the index buffer (face information)
	//1) Allocate some memory on the RAM that is visible to both the CPU and the GPU. This will be what we call the staging buffer
	VkBufferCreateInfo index_buffer_info = {};
	index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	index_buffer_info.size = indices_size;
	index_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vkCreateBuffer(logical_device_, &index_buffer_info, nullptr, &staging_buffers.indices.buffer);
	vkGetBufferMemoryRequirements(logical_device_, staging_buffers.indices.buffer, &mem_reqs);
	mem_alloc.allocationSize = mem_reqs.size;
	GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &staging_buffers.indices.memory);

	//2) Copy the vertex information to the allocated memory (to the staging buffer)
	vkMapMemory(logical_device_, staging_buffers.indices.memory, 0, indices_size, 0, &data);
	memcpy(data, indices.data(), indices_size);
	vkUnmapMemory(logical_device_, staging_buffers.indices.memory);
	vkBindBufferMemory(logical_device_, staging_buffers.indices.buffer, staging_buffers.indices.memory, 0);

	//3) Allocate memory on the VRAM that is only visible to the GPU. This is the memory location that will be used by the shaders
	index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkCreateBuffer(logical_device_, &index_buffer_info, nullptr, &index_buffer_);
	vkGetBufferMemoryRequirements(logical_device_, index_buffer_, &mem_reqs);
	mem_alloc.allocationSize = mem_reqs.size;
	GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &mem_alloc, nullptr, &index_buffer_memory_);
	vkBindBufferMemory(logical_device_, index_buffer_, index_buffer_memory_, 0);

	//We are done with passing information to the staging buffers, now we need to actually copy the buffers to the VRAM

	//4) Create a command buffer and fill it with instructions to copy data from the staging buffers to the VRAM
	VkCommandBufferAllocateInfo cmd_buf_info = {};
	cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_buf_info.commandPool = command_pool_;
	cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_buf_info.commandBufferCount = 1;
	VkCommandBuffer copy_command_buffer;
	vkAllocateCommandBuffers(logical_device_, &cmd_buf_info, &copy_command_buffer);
	VkCommandBufferBeginInfo buffer_begin_info = {};
	buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(copy_command_buffer, &buffer_begin_info);
	VkBufferCopy copyRegion = {};
	copyRegion.size = vertices_size;
	vkCmdCopyBuffer(copy_command_buffer, staging_buffers.vertices.buffer, vertex_buffer_, 1, &copyRegion);
	copyRegion.size = indices_size;
	vkCmdCopyBuffer(copy_command_buffer, staging_buffers.indices.buffer, index_buffer_, 1, &copyRegion);
	vkEndCommandBuffer(copy_command_buffer);

	//5) Submit the command buffer to a queue for it to be processed by the GPU
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &copy_command_buffer;
	vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue_);
	vkFreeCommandBuffers(logical_device_, command_pool_, 1, &copy_command_buffer);

	//Destroy the temporary staging buffers to free VRAM
	vkDestroyBuffer(logical_device_, staging_buffers.vertices.buffer, nullptr);
	vkFreeMemory(logical_device_, staging_buffers.vertices.memory, nullptr);
	vkDestroyBuffer(logical_device_, staging_buffers.indices.buffer, nullptr);
	vkFreeMemory(logical_device_, staging_buffers.indices.memory, nullptr);

	std::cout << "set up vertex and index buffers" << std::endl;
}

void VulkanApplication::CreateUniformBuffer() {
	//Configure the uniform buffer creation
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = sizeof(uniform_buffer_data_);
	buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	vkCreateBuffer(logical_device_, &buffer_info, nullptr, &uniform_buffer_);

	//Get memory requirements for the uniform buffer
	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(logical_device_, uniform_buffer_, &mem_reqs);

	//Allocate memory for the uniform buffer on the VRAM (using the HOST_VISIBLE_BIT flag) and make it CPU accessible (using the vkMapMemory call)
	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_reqs.size;
	GetMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &alloc_info.memoryTypeIndex);
	vkAllocateMemory(logical_device_, &alloc_info, nullptr, &uniform_buffer_memory_);
	vkBindBufferMemory(logical_device_, uniform_buffer_, uniform_buffer_memory_, 0);

	//Copy the buffer to the VRAM
	UpdateUniformData();
}

void VulkanApplication::UpdateUniformData() {
	//Set up transformation matrices
	glm::mat4 modelMatrix;
	uniform_buffer_data_.model_matrix = glm::translate(glm::mat4x4(1), glm::vec3(0.0f, 0.0f, -1.0f));
	//uniform_buffer_data_.view_matrix = glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
	uniform_buffer_data_.projection_matrix = glm::perspective(glm::radians(70.f), (float)swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f, 10.0f);

	//Copy the data to the VRAM (this procedure is similar to what we do when creating the vertex and index buffers)
	void* data;
	vkMapMemory(logical_device_, uniform_buffer_memory_, 0, sizeof(uniform_buffer_data_), 0, &data);
	memcpy(data, &uniform_buffer_data_, sizeof(uniform_buffer_data_));
	vkUnmapMemory(logical_device_, uniform_buffer_memory_);
}

//Find device memory that is supported by the requirements (typeBits) and meets the desired properties
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
	//Find surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, window_surface_, &surface_capabilities) != VK_SUCCESS) {
		std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
		exit(1);
	}

	//Find supported surface formats for the swapchain's images
	uint32_t format_count;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, window_surface_, &format_count, nullptr) != VK_SUCCESS || format_count == 0) {
		std::cerr << "failed to get number of supported surface formats" << std::endl;
		exit(1);
	}
	std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, window_surface_, &format_count, surface_formats.data()) != VK_SUCCESS) {
		std::cerr << "failed to get supported surface formats" << std::endl;
		exit(1);
	}

	//Find supported present modes
	uint32_t present_mode_count;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, window_surface_, &present_mode_count, nullptr) != VK_SUCCESS || present_mode_count == 0) {
		std::cerr << "failed to get number of supported presentation modes" << std::endl;
		exit(1);
	}
	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, window_surface_, &present_mode_count, present_modes.data()) != VK_SUCCESS) {
		std::cerr << "failed to get supported presentation modes" << std::endl;
		exit(1);
	}

	//Determine number of images for swap chain
	uint32_t image_count = surface_capabilities.minImageCount + 1;
	if (surface_capabilities.maxImageCount != 0 && image_count > surface_capabilities.maxImageCount) {
		image_count = surface_capabilities.maxImageCount;
	}
	std::cout << "using " << image_count << " images for swap chain" << std::endl;

	//Select a surface format
	VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(surface_formats);

	//Select swap chain size
	swap_chain_extent_ = ChooseSwapExtent(surface_capabilities);

	//Determine transformation to use (preferring no transform)
	VkSurfaceTransformFlagBitsKHR surface_transform;
	if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		surface_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else {
		surface_transform = surface_capabilities.currentTransform;
	}

	//Choose presentation mode (preferring MAILBOX ~= triple buffering)
	VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);

	//Finally, create the swap chain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = window_surface_;
	createInfo.minImageCount = image_count;
	createInfo.imageFormat = surface_format.format;
	createInfo.imageColorSpace = surface_format.colorSpace;
	createInfo.imageExtent = swap_chain_extent_;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.preTransform = surface_transform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = present_mode;
	createInfo.clipped = VK_TRUE;
	if (vkCreateSwapchainKHR(logical_device_, &createInfo, nullptr, &swap_chain_) != VK_SUCCESS) {
		std::cerr << "failed to create swap chain" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created swap chain" << std::endl;
	}
	swap_chain_format_ = surface_format.format;

	//Store the images used by the swap chain
	//Note: these are the images that swap chain image indices refer to
	//Note: actual number of images may differ from requested number, since it's a lower bound
	uint32_t actual_image_count = 0;
	if (vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &actual_image_count, nullptr) != VK_SUCCESS || actual_image_count == 0) {
		std::cerr << "failed to acquire number of swap chain images" << std::endl;
		exit(1);
	}
	swap_chain_images_.resize(actual_image_count);
	if (vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &actual_image_count, swap_chain_images_.data()) != VK_SUCCESS) {
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
	for (const auto& available_surface_format : availableFormats) {
		if (available_surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
			return available_surface_format;
		}
	}

	// Or fall back to the first available one
	return availableFormats[0];
}

VkExtent2D VulkanApplication::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
	//Choose the size of the images in the swapchain based on viewport width and size, and device output image capabilities
	if (surfaceCapabilities.currentExtent.width == -1) {
		VkExtent2D swapchain_extent = {};
		swapchain_extent.width = std::min(std::max(width_, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
		swapchain_extent.height = std::min(std::max(height_, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);
		return swapchain_extent;
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
	//It all starts from the swapchain. The swapchain is the queue of images that are either being presented to the glfw window (in our case)
	//or waiting to be presented. The swapchain defines how many images to use to show to the window (the
	//glfw window in our case) and the way it presents them. Remember that to present images to the glfw window, Vulkan uses surfaces. 
	//In the case where the swapchain uses triple buffering, there will be three images in use at once: one for rendering to, one to 
	//present and the other one to present next after the current image has been presented.
	//A swapchain image is the final destination of a render pass. A render pass is a protocol, a set of operations and resources
	//needed to generate an image from the information provided. The information provided is vertices and indices (faces), the resources are:
	//- 1) the shaders that process the data and output data that can be used by the render pass to build an image
	//- 2) the area of memory where to write the final image once the render pass has completed
	//Think of the render pass as a musical orchestrator, the sheets of music as the data to be processed, the instruments as the
	//shaders and the musicians playing the instruments as physical microprocessors on the GPU doing work with the shaders.
	//The music produced is the final image.
	//With that said, the area of memory where the image will be written is called an attachment. We know that an area of memory
	//is a buffer, it's just a container of information, thus, an attachment is an area of memory or buffer where the render pass will output
	//the rendered image. Attachments are thus shared by framebuffers and render passes. What is a framebuffer? A framebuffer is the way 
	//that the render pass connects to the swapchain. By writing its output to an attachment, the render pass is telling the framebuffer: 
	//hey, i have rendered an image. The swapchain will then query the framebuffers when it is asked to show an image. 
	//It's a very rigid hierarchy.
	//It is EXTREMELY IMPORTANT to know that in actuality, a render pass is used as a blueprint to create
	//multiple render pass instances. Each instance performs the same operations, but writes its output to different attachments in
	//different framebuffers. Of course this is because if there was only one render pass instance we would only have an image 
	//rendered at a time without having the benefits of double or triple buffering, so remember, for each framebuffer there is 
	//a render pass instance that generates the image it will store.
	//Now lets dive a little deeper in render passes:
	//As we said, a render pass describes the set of data necessary to accomplish a rendering operation.
	//In Vulkan, this is a set of framebuffer attachments that will be used during rendering.
	//These attachments include any buffers that will be read from or written into during rendering, such as colour, depth, 
	//and stencil buffers. This can also include input attachments which are intermediate buffers which 
	//are written into in one subpass and then read out of by another one.
	//Following the standard Vulkan paradigm, these attachments have to be explicitly defined when creating 
	//a render pass, with information like image format, number of samples, and load and store behaviour specified.
	//This reduces the driver workload during runtime, as it does not have to deduce this information itself.
	//In addition to attachments, render passes also contain one or more subpasses that order the rendering operations.
	//Subpasses essentially represent a phase of rendering in which rendering work is done with a subset of the 
	//attachments in the render pass. A set of commands are recorded into each subpass to describe what work needs 
	//to be done in that subpass.
	//The render pass also defines a set of subpass dependencies which determine the order of execution for pairs of
	//subpasses. They act as execution and memory dependencies. Dependencies are vital when two or more subpasses
	//access the same attachment, as Vulkan does not guarantee the order in which the subpasses will be executed by the GPU.
	//It is important to note that while a render pass describes the characteristics of all of the attachments 
	//used and what to do with them, it does not point to any actual objects. This is handled by framebuffer objects.

	//Define the description for the attachments
	VkAttachmentDescription attachment_description = {};
	attachment_description.format = swap_chain_format_;
	attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_description.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//Note: hardware will automatically transition attachment to the specified layout
	//Note: index refers to attachment descriptions array
	VkAttachmentReference color_attachment_reference = {};
	color_attachment_reference.attachment = 0;
	color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//Note: this is a description of how the attachments of the render pass will be used in this sub pass
	//e.g. if they will be read in shaders and/or drawn to
	VkSubpassDescription sub_pass_description = {};
	sub_pass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub_pass_description.colorAttachmentCount = 1;
	sub_pass_description.pColorAttachments = &color_attachment_reference;

	//Create the render pass
	VkRenderPassCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.attachmentCount = 1;
	create_info.pAttachments = &attachment_description;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &sub_pass_description;
	if (vkCreateRenderPass(logical_device_, &create_info, nullptr, &render_pass_) != VK_SUCCESS) {
		std::cerr << "failed to create render pass" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created render pass" << std::endl;
	}
}

void VulkanApplication::CreateImageViews() {
	//Create an image view for every image in the swap chain. An image view is an image descriptor, it's just metadata that describes
	//the image (such as knowing what format it is, the type etc...)
	swap_chain_image_views_.resize(swap_chain_images_.size());
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = swap_chain_images_[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = swap_chain_format_;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		if (vkCreateImageView(logical_device_, &create_info, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS) {
			std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
			exit(1);
		}
	}
	std::cout << "created image views for swap chain images" << std::endl;
}

void VulkanApplication::CreateFramebuffers() {
	//Remember, a buffer is just an aera of memory. A framebuffer is no different: a frame buffer is
	//an area of memory that contains a frame. A frame is another buffer that contains a list of buffers, called attachments (in this context). 
	//An attachment contains an image view. Recall that an image view is just an image descriptor. 
	//This descriptor contains the image itself but also adds other information such as the type of image and the format.
	//The image view in an attachment can be a color, depth or stencil buffer for example.
	//The color buffer is just your regular image, but what are are depth and stencil buffers? 
	//Think of a stencil buffer as a portion of memory that represents an image that for each pixel contains a value. 
	//That value represents the masking, therefore it acts as a stencil.
	//It's just like cutting a hole in a piece of paper, then placing it on a surface you want to spray paint and using
	//it as a mask to spray that exact pattern you cut out of the piece of paper on that surface.
	//You could accomplish the same with just saying: if this pixel has value 0, don't draw it: if the pixel has value 1
	//then you can draw it. This can and is used to occlude certain areas of a model being rendered.
	//A depth buffer is an area of memory that represents an image that for each pixel has a value that represents the
	//distance from the camera to the surface in the scene that that pixel belongs to. Lets say you have a wall in front
	//of you in the scene that is exactly one meter away and you are looking at it from a perfectly perpendicular angle
	//with your camera in orthogonal mode: for each pixel that represents the rendered wall, the depth buffer will contain that one meter
	//distance value to that area on the wall represented by that pixel. This information is useful and actually fundamental
	//for making sure to draw what is visible and not what is not theoretically visible. If you have 2 overlapping planes
	//you can't know which one to render if you don't have the depth information.

	//Create a framebuffer for each image
	swap_chain_frame_buffers_.resize(swap_chain_images_.size());
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {
		VkFramebufferCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		create_info.renderPass = render_pass_;
		create_info.attachmentCount = 1;
		create_info.pAttachments = &swap_chain_image_views_[i];
		create_info.width = swap_chain_extent_.width;
		create_info.height = swap_chain_extent_.height;
		create_info.layers = 1;
		if (vkCreateFramebuffer(logical_device_, &create_info, nullptr, &swap_chain_frame_buffers_[i]) != VK_SUCCESS) {
			std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
			exit(1);
		}
	}
	std::cout << "created framebuffers for swap chain image views" << std::endl;
}

VkShaderModule VulkanApplication::CreateShaderModule(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (file) {
		std::vector<char> file_bytes(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(file_bytes.data(), file_bytes.size());
		file.close();

		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = file_bytes.size();
		create_info.pCode = (uint32_t*)file_bytes.data();

		VkShaderModule shader_module;
		if (vkCreateShaderModule(logical_device_, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
			std::cerr << "failed to create shader module for " << filename << std::endl;
			exit(1);
		}

		std::cout << "created shader module for " << filename << std::endl;
		return shader_module;
	}
	else {
		std::cout << "could not open file " << filename << std::endl;
	}
}

void VulkanApplication::CreateGraphicsPipeline() {
	//Compile and load the shaders
	//system((std::string("start \"\" \"") + kShaderPath_ + std::string("shader_compiler.bat\"")).c_str());
	VkShaderModule vertex_shader_module = CreateShaderModule(kShaderPath_ + std::string("vertex_shader.spv"));
	VkShaderModule fragment_shader_module = CreateShaderModule(kShaderPath_ + std::string("fragment_shader.spv"));

	//Set up shader stage info
	VkPipelineShaderStageCreateInfo vertex_shader_create_info = {};
	vertex_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_shader_create_info.module = vertex_shader_module;
	vertex_shader_create_info.pName = "main";
	VkPipelineShaderStageCreateInfo fragment_shader_create_info = {};
	fragment_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_shader_create_info.module = fragment_shader_module;
	fragment_shader_create_info.pName = "main";
	VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_shader_create_info, fragment_shader_create_info };

	//Describe vertex input meaning how the graphics driver should interpret the information given in the vertex buffer
	VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {};
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions = &vertex_binding_description_;
	vertex_input_create_info.vertexAttributeDescriptionCount = 1;
	vertex_input_create_info.pVertexAttributeDescriptions = vertex_attribute_descriptions_.data();

	//Describe input assembly meaning what we are going to draw to the screen. We want to draw triangles
	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};
	input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

	//Describe viewport and scissor. The viewport specifies how the normalized window coordinates 
	//(-1 to 1 for both width and height) are transformed into the pixel coordinates of the framebuffer.
	//Scissor is the area where you can render, this is similar to the viewport in that regard but changing the scissor 
	//rectangle doesn't affect the coordinates
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

	//Note: scissor test is always enabled (although dynamic scissor is possible)
	//Number of viewports must match number of scissors
	VkPipelineViewportStateCreateInfo viewport_create_info = {};
	viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_create_info.viewportCount = 1;
	viewport_create_info.pViewports = &viewport;
	viewport_create_info.scissorCount = 1;
	viewport_create_info.pScissors = &scissor;

	//Describe rasterization
	//Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features)
	VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
	rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_create_info.depthClampEnable = VK_FALSE;
	rasterization_create_info.rasterizerDiscardEnable = VK_FALSE;
	rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization_create_info.depthBiasEnable = VK_FALSE;
	rasterization_create_info.depthBiasConstantFactor = 0.0f;
	rasterization_create_info.depthBiasClamp = 0.0f;
	rasterization_create_info.depthBiasSlopeFactor = 0.0f;
	rasterization_create_info.lineWidth = 1.0f;

	//Describe multisampling
	//Note: using multisampling also requires turning on device features
	VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
	multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.minSampleShading = 1.0f;
	multisample_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_create_info.alphaToOneEnable = VK_FALSE;

	//Describing color blending
	//Note: all paramaters except blendEnable and colorWriteMask are irrelevant here
	VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
	color_blend_attachment_state.blendEnable = VK_FALSE;
	color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	//Note: all attachments must have the same values unless a device feature is enabled
	VkPipelineColorBlendStateCreateInfo color_blend_create_info = {};
	color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_create_info.logicOpEnable = VK_FALSE;
	color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
	color_blend_create_info.attachmentCount = 1;
	color_blend_create_info.pAttachments = &color_blend_attachment_state;
	color_blend_create_info.blendConstants[0] = 0.0f;
	color_blend_create_info.blendConstants[1] = 0.0f;
	color_blend_create_info.blendConstants[2] = 0.0f;
	color_blend_create_info.blendConstants[3] = 0.0f;

	//Describe pipeline layout
	//Note: this describes the mapping between memory and shader resources (descriptor sets)
	//This is for uniform buffers and samplers
	VkDescriptorSetLayoutBinding layout_binding = {};
	layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layout_binding.descriptorCount = 1;
	layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
	descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_create_info.bindingCount = 1;
	descriptor_set_layout_create_info.pBindings = &layout_binding;
	if (vkCreateDescriptorSetLayout(logical_device_, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor layout" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created descriptor layout" << std::endl;
	}
	VkPipelineLayoutCreateInfo layout_create_info = {};
	layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_create_info.setLayoutCount = 1;
	layout_create_info.pSetLayouts = &descriptor_set_layout_;
	if (vkCreatePipelineLayout(logical_device_, &layout_create_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
		std::cerr << "failed to create pipeline layout" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created pipeline layout" << std::endl;
	}

	//Configure the creation of the graphics pipeline
	VkGraphicsPipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages = shader_stages;
	pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
	pipeline_create_info.pViewportState = &viewport_create_info;
	pipeline_create_info.pRasterizationState = &rasterization_create_info;
	pipeline_create_info.pMultisampleState = &multisample_create_info;
	pipeline_create_info.pColorBlendState = &color_blend_create_info;
	pipeline_create_info.layout = pipeline_layout_;
	pipeline_create_info.renderPass = render_pass_;
	pipeline_create_info.subpass = 0;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex = -1;

	//Create the pipeline
	if (vkCreateGraphicsPipelines(logical_device_, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &graphics_pipeline_) != VK_SUCCESS) {
		std::cerr << "failed to create graphics pipeline" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created graphics pipeline" << std::endl;
	}

	//Delete the unused shader modules as they have been transferred to the VRAM now
	vkDestroyShaderModule(logical_device_, vertex_shader_module, nullptr);
	vkDestroyShaderModule(logical_device_, fragment_shader_module, nullptr);
}

void VulkanApplication::CreateDescriptorPool() {
	//This describes how many descriptor sets we'll create from this pool for each type
	VkDescriptorPoolSize type_count;
	type_count.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	type_count.descriptorCount = 1;

	//Configure the pool creation
	VkDescriptorPoolCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	create_info.poolSizeCount = 1;
	create_info.pPoolSizes = &type_count;
	create_info.maxSets = 1;

	//Create the pool with the specified config
	if (vkCreateDescriptorPool(logical_device_, &create_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor pool" << std::endl;
		exit(1);
	}
	else {
		std::cout << "descriptor pool created" << std::endl;
	}
}

void VulkanApplication::CreateDescriptorSets() {
	//With descriptor sets there are three levels. You have the descriptor set that contains descriptors.
	//Descriptors are buffers (pieces of memory) that point to uniform buffers and also contain other
	//information such as the size and the type of the uniform buffer they point to.
	//The uniform buffer is the last in the chain: the uniform buffer contains the actual data we want
	//to pass to the shaders.
	//Desciptor sets are allocated using a descriptor pool, but the behaviour of the pool is handled by
	//the Vulkan drivers so we don't need to worry about how it works.

	//There needs to be one descriptor set per binding point in the shader
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = descriptor_pool_;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_set_layout_;

	//Create the descriptor set
	if (vkAllocateDescriptorSets(logical_device_, &alloc_info, &descriptor_set_) != VK_SUCCESS) {
		std::cerr << "failed to create descriptor set" << std::endl;
		exit(1);
	}
	else {
		std::cout << "created descriptor set" << std::endl;
	}

	//Bind the uniform buffer to the descriptor. This descriptor will then be bound to a descriptor set and then that descriptor
	//set will be uploaded to the VRAM
	VkDescriptorBufferInfo descriptor_buffer_info = {};
	descriptor_buffer_info.buffer = uniform_buffer_;
	descriptor_buffer_info.offset = 0;
	descriptor_buffer_info.range = sizeof(uniform_buffer_data_);

	//Bind the descriptor to the descriptor set
	VkWriteDescriptorSet write_descriptor_set = {};
	write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor_set.dstSet = descriptor_set_;
	write_descriptor_set.descriptorCount = 1;
	write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
	write_descriptor_set.dstBinding = 0;

	//Send the descriptor set to the VRAM
	vkUpdateDescriptorSets(logical_device_, 1, &write_descriptor_set, 0, nullptr);
}

void VulkanApplication::CreateCommandBuffers() {
	//This is where it all comes together. In this function we allocate memory for the command buffers from the command pool,
	//then, we configure and record commands on it and we submit it to a queue for it to be processed by the GPU.

	//Configure and allocate command buffers from the command pool
	graphics_command_buffers_.resize(swap_chain_images_.size());
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool_;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = (uint32_t)graphics_command_buffers_.size();
	if (vkAllocateCommandBuffers(logical_device_, &alloc_info, graphics_command_buffers_.data()) != VK_SUCCESS) {
		std::cerr << "failed to allocate graphics command buffers" << std::endl;
		exit(1);
	}
	else {
		std::cout << "allocated graphics command buffers" << std::endl;
	}

	//Configure command buffer command recording
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	VkImageSubresourceRange sub_resource_range = {};
	sub_resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	sub_resource_range.baseMipLevel = 0;
	sub_resource_range.levelCount = 1;
	sub_resource_range.baseArrayLayer = 0;
	sub_resource_range.layerCount = 1;

	//Set the background color
	VkClearValue clearColor = {
		{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A
	};

	//For each image in the swapchain, we record the same set of commands
	for (size_t i = 0; i < swap_chain_images_.size(); i++) {

		//Start recording commands
		vkBeginCommandBuffer(graphics_command_buffers_[i], &begin_info);

		//If present queue family and graphics queue family are different, then a barrier is necessary
		//The barrier is also needed initially to transition the image to the present layout
		VkImageMemoryBarrier present_to_draw_barrier = {};
		present_to_draw_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		present_to_draw_barrier.srcAccessMask = 0;
		present_to_draw_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		present_to_draw_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		present_to_draw_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		if (present_queue_family_ != graphics_queue_family_) {
			present_to_draw_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			present_to_draw_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
		else {
			present_to_draw_barrier.srcQueueFamilyIndex = present_queue_family_;
			present_to_draw_barrier.dstQueueFamilyIndex = graphics_queue_family_;
		}
		present_to_draw_barrier.image = swap_chain_images_[i];
		present_to_draw_barrier.subresourceRange = sub_resource_range;
		vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &present_to_draw_barrier);

		//Configure a render pass instance and tell Vulkan to instantiate a render pass and run it
		VkRenderPassBeginInfo render_pass_begin_info = {};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = render_pass_;
		render_pass_begin_info.framebuffer = swap_chain_frame_buffers_[i];
		render_pass_begin_info.renderArea.offset.x = 0;
		render_pass_begin_info.renderArea.offset.y = 0;
		render_pass_begin_info.renderArea.extent = swap_chain_extent_;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clearColor;
		vkCmdBeginRenderPass(graphics_command_buffers_[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		//Bind the data to be sent to the shaders (descriptor sets)
		vkCmdBindDescriptorSets(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

		//Bind the graphics pipeline. The graphics pipeline contains all the information Vulkan needs to render an image
		vkCmdBindPipeline(graphics_command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_);

		//Bind the index and vertex buffers
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(graphics_command_buffers_[i], 0, 1, &vertex_buffer_, &offset);
		vkCmdBindIndexBuffer(graphics_command_buffers_[i], index_buffer_, 0, VK_INDEX_TYPE_UINT32);

		//Draw the triangles
		vkCmdDrawIndexed(graphics_command_buffers_[i], 6, 1, 0, 0, 0);

		//End the render pass
		vkCmdEndRenderPass(graphics_command_buffers_[i]);

		// If present and graphics queue families differ, then another barrier is required
		if (present_queue_family_ != graphics_queue_family_) {
			VkImageMemoryBarrier draw_to_present_barrier = {};
			draw_to_present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			draw_to_present_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			draw_to_present_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			draw_to_present_barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			draw_to_present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			draw_to_present_barrier.srcQueueFamilyIndex = graphics_queue_family_;
			draw_to_present_barrier.dstQueueFamilyIndex = present_queue_family_;
			draw_to_present_barrier.image = swap_chain_images_[i];
			draw_to_present_barrier.subresourceRange = sub_resource_range;
			vkCmdPipelineBarrier(graphics_command_buffers_[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &draw_to_present_barrier);
		}

		//Stop recording commands
		if (vkEndCommandBuffer(graphics_command_buffers_[i]) != VK_SUCCESS) {
			std::cerr << "failed to record command buffer" << std::endl;
			exit(1);
		}
	}

	std::cout << "recorded command buffers" << std::endl;

	//No longer needed
	vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
}

void VulkanApplication::Draw() {
	//Acquire image
	uint32_t imageIndex;
	VkResult res = vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &imageIndex);

	//Unless surface is out of date right now, defer swap chain recreation until end of this frame
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		OnWindowSizeChanged();
		return;
	}
	else if (res != VK_SUCCESS) {
		std::cerr << "failed to acquire image" << std::endl;
		exit(1);
	}

	//Wait for image to be available and draw
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &image_available_semaphore_;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &rendering_finished_semaphore_;

	//This is the stage where the queue should wait on the semaphore
	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	submitInfo.pWaitDstStageMask = &waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &graphics_command_buffers_[imageIndex];
	if (vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		std::cerr << "failed to submit draw command buffer" << std::endl;
		exit(1);
	}

	//Present drawn image
	//Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue
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
#define GLFW_INCLUDE_VULKAN
// STL
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <filesystem>

// Math
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Project local classes
#include "Singleton.hpp"
#include "Input.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "GltfLoader.hpp"

// Configuration
const uint32_t WIDTH = 640;
const uint32_t HEIGHT = 480;
const bool enableValidationLayers = false;
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

// Debug callback
VkBool32 debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData) {
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
		std::cerr << "ERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
	}
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
		std::cerr << "WARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
	}

	//exit(1);

	return VK_FALSE;
}

bool windowResized = false;

/// <summary>
/// This is a class for a generic Vulkan application
/// </summary>
class VulkanApplication {
public:
	VulkanApplication() {
		timeStart = std::chrono::high_resolution_clock::now();
	}

	void run() {
		// Note: dynamically loading loader may be a better idea to fail gracefully when Vulkan is not supported

		// Create window for Vulkan
		glfwInit();


		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Solar System Explorer", nullptr, nullptr);

		input = Input::Instance();
		input.Init(window);
		mouseSensitivity = 0.1f;

		glfwSetWindowSizeCallback(window, VulkanApplication::onWindowResized);

		// Use Vulkan
		setupVulkan();

		mainLoop();

		cleanup(true);
	}

private:
	// Window
	GLFWwindow* window;

	// Basic vulkan stuff
	VkInstance							instance;
	VkSurfaceKHR						windowSurface;
	VkPhysicalDevice					physicalDevice;
	VkDevice							logicalDevice;
	VkDebugReportCallbackEXT			callback;
	VkQueue								graphicsQueue;
	uint32_t							graphicsQueueFamily;
	VkQueue								presentQueue;
	uint32_t							presentQueueFamily;
	VkPhysicalDeviceMemoryProperties	deviceMemoryProperties;
	VkSemaphore							imageAvailableSemaphore;
	VkSemaphore							renderingFinishedSemaphore;
	VkRenderPass						renderPass;
	VkPipeline							graphicsPipeline;

	// Vertex and index buffers
	VkBuffer										vertexBuffer;
	VkDeviceMemory									vertexBufferMemory;
	VkBuffer										indexBuffer;
	VkDeviceMemory									indexBufferMemory;
	VkVertexInputBindingDescription					vertexBindingDescription;
	std::vector<VkVertexInputAttributeDescription>	vertexAttributeDescriptions;

	// Shader resources (descriptor sets and push constants)
	VkBuffer				uniformBuffer;
	VkDeviceMemory			uniformBufferMemory;
	VkDescriptorSetLayout	descriptorSetLayout;
	VkDescriptorPool		descriptorPool;
	VkDescriptorSet			descriptorSet;
	VkPipelineLayout		pipelineLayout;
	struct {
		glm::mat4 transformationMatrix;
	}						uniformBufferData;

	// Swap chain
	VkExtent2D					swapChainExtent;
	VkFormat					swapChainFormat;
	VkSwapchainKHR				oldSwapChain;
	VkSwapchainKHR				swapChain;
	std::vector<VkImage>		swapChainImages;
	std::vector<VkImageView>	swapChainImageViews;
	std::vector<VkFramebuffer>	swapChainFramebuffers;

	// Vulkan commands
	VkCommandPool					commandPool;
	std::vector<VkCommandBuffer>	graphicsCommandBuffers;


	// Misc
	std::chrono::high_resolution_clock::time_point timeStart;
	Scene scene;
	Input input;
	Camera mainCamera;
	glm::mat4 modelMatrix;
	float mouseSensitivity;

	void setupVulkan() {
		oldSwapChain = VK_NULL_HANDLE;
		createInstance();
		createDebugCallback();
		createWindowSurface();
		findPhysicalDevice();
		checkSwapChainSupport();
		findQueueFamilies();
		createLogicalDevice();
		createSemaphores();
		createCommandPool();
		createVertexAndIndexBuffers();
		createUniformBuffer();
		createSwapChain();
		createRenderPass();
		createImageViews();
		createFramebuffers();
		createGraphicsPipeline();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			updateUniformData();
			draw();
			glfwPollEvents();
		}
	}

	static void onWindowResized(GLFWwindow* window, int width, int height) {
		windowResized = true;
	}

	void onWindowSizeChanged() {
		windowResized = false;

		// Only recreate objects that are affected by framebuffer size changes
		cleanup(false);

		createSwapChain();
		createRenderPass();
		createImageViews();
		createFramebuffers();
		createGraphicsPipeline();
		createCommandBuffers();
	}

	void cleanup(bool fullClean) {
		vkDeviceWaitIdle(logicalDevice);

		vkFreeCommandBuffers(logicalDevice, commandPool, (uint32_t)graphicsCommandBuffers.size(), graphicsCommandBuffers.data());

		vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
		vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			vkDestroyFramebuffer(logicalDevice, swapChainFramebuffers[i], nullptr);
			vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);
		}

		vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

		if (fullClean) {
			vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(logicalDevice, renderingFinishedSemaphore, nullptr);

			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

			// Clean up uniform buffer related objects
			vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
			vkDestroyBuffer(logicalDevice, uniformBuffer, nullptr);
			vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);

			// Buffers must be destroyed after no command buffers are referring to them anymore
			vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
			vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
			vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
			vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);

			// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
			vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);

			vkDestroyDevice(logicalDevice, nullptr);

			vkDestroySurfaceKHR(instance, windowSurface, nullptr);

			if (enableValidationLayers) {
				PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
				DestroyDebugReportCallback(instance, callback, nullptr);
			}

			vkDestroyInstance(instance, nullptr);
		}
	}

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}
		return true;
	}

	void createInstance() {
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "VulkanClear";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "ClearScreenEngine";
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

		if (enableValidationLayers) {
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

		if (enableValidationLayers) {
			if (checkValidationLayerSupport()) {
				createInfo.enabledLayerCount = 1;
				createInfo.ppEnabledLayerNames = validationLayers.data();
			}
		}

		// Initialize Vulkan instance
		if (vkCreateInstance(&createInfo, nullptr, &instance)) {
			std::cerr << "failed to create instance!" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan instance" << std::endl;
		}
	}

	void createWindowSurface() {
		if (glfwCreateWindowSurface(instance, window, NULL, &windowSurface) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}

		std::cout << "created window surface" << std::endl;
	}

	void findPhysicalDevice() {
		// Try to find 1 Vulkan supported device
		// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions
		uint32_t deviceCount = 0;
		if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
			std::cerr << "failed to get number of physical devices" << std::endl;
			exit(1);
		}

		deviceCount = 1;
		VkResult res = vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice);
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
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		uint32_t supportedVersion[] = {
			VK_VERSION_MAJOR(deviceProperties.apiVersion),
			VK_VERSION_MINOR(deviceProperties.apiVersion),
			VK_VERSION_PATCH(deviceProperties.apiVersion)
		};

		std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;
	}

	void checkSwapChainSupport() {
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "physical device doesn't support any extensions" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, deviceExtensions.data());

		for (const auto& extension : deviceExtensions) {
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				std::cout << "physical device supports swap chains" << std::endl;
				return;
			}
		}

		std::cerr << "physical device doesn't support swap chains" << std::endl;
		exit(1);
	}

	void findQueueFamilies() {
		// Check queue families
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		if (queueFamilyCount == 0) {
			std::cout << "physical device has no queue families!" << std::endl;
			exit(1);
		}

		// Find queue family with graphics support
		// Note: is a transfer queue necessary to copy vertices to the gpu or can a graphics queue handle that?
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		std::cout << "physical device has " << queueFamilyCount << " queue families" << std::endl;

		bool foundGraphicsQueueFamily = false;
		bool foundPresentQueueFamily = false;

		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, windowSurface, &presentSupport);

			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphicsQueueFamily = i;
				foundGraphicsQueueFamily = true;

				if (presentSupport) {
					presentQueueFamily = i;
					foundPresentQueueFamily = true;
					break;
				}
			}

			if (!foundPresentQueueFamily && presentSupport) {
				presentQueueFamily = i;
				foundPresentQueueFamily = true;
			}
		}

		if (foundGraphicsQueueFamily) {
			std::cout << "queue family #" << graphicsQueueFamily << " supports graphics" << std::endl;

			if (foundPresentQueueFamily) {
				std::cout << "queue family #" << presentQueueFamily << " supports presentation" << std::endl;
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

	void createLogicalDevice() {
		// Greate one graphics queue and optionally a separate presentation queue
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfo[2] = {};

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = graphicsQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = presentQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		// Create logical device from physical device
		// Note: there are separate instance and device extensions!
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;

		if (graphicsQueueFamily == presentQueueFamily) {
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

		if (enableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = 1;
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
			std::cerr << "failed to create logical device" << std::endl;
			exit(1);
		}

		std::cout << "created logical device" << std::endl;

		// Get graphics and presentation queues (which may be the same)
		vkGetDeviceQueue(logicalDevice, graphicsQueueFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(logicalDevice, presentQueueFamily, 0, &presentQueue);

		std::cout << "acquired graphics and presentation queues" << std::endl;

		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
	}

	void createDebugCallback() {
		if (enableValidationLayers) {
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugCallback;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

			PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

			if (CreateDebugReportCallback(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
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

	void createSemaphores() {
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(logicalDevice, &createInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice, &createInfo, nullptr, &renderingFinishedSemaphore) != VK_SUCCESS) {
			std::cerr << "failed to create semaphores" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created semaphores" << std::endl;
		}
	}

	void createCommandPool() {
		// Create graphics command pool
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = graphicsQueueFamily;

		if (vkCreateCommandPool(logicalDevice, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	void createVertexAndIndexBuffers() {

		#pragma region PreviousImplementation
		//// Create an instance of the Importer class
		//Assimp::Importer importer;

		//// And have it read the given file with some example postprocessing
		//// Usually - if speed is not the most important aspect for you - you'll
		//// probably to request more postprocessing than we do in this example.
		//const aiScene* scene = importer.ReadFile("C:\\Users\\paolo.parker\\source\\repos\\celeritas-engine\\models\\monkey.dae",
		//	aiProcess_CalcTangentSpace |
		//	aiProcess_Triangulate |
		//	aiProcess_JoinIdenticalVertices |
		//	aiProcess_SortByPType);

		//// If the import failed, report it
		//if (nullptr == scene) {
		//	std::cout << "import failed\n";
		//}

		//for (int i = 0; i < scene->mMeshes[0]->mNumVertices; ++i) {
		//	float xCoord = scene->mMeshes[0]->mVertices[i].x;
		//	float yCoord = scene->mMeshes[0]->mVertices[i].y;
		//	float zCoord = scene->mMeshes[0]->mVertices[i].z;
		//	model.vertices.push_back(Vertex{ xCoord, yCoord, zCoord });
		//};
		//model.verticesSize = (uint32_t)(model.vertices.size() * sizeof(model.vertices[0]));

		//// Setup indices
		//for (int i = 0; i < scene->mMeshes[0]->mNumFaces; ++i) {
		//	uint32_t index1 = scene->mMeshes[0]->mFaces[i].mIndices[0];
		//	uint32_t index2 = scene->mMeshes[0]->mFaces[i].mIndices[1];
		//	uint32_t index3 = scene->mMeshes[0]->mFaces[i].mIndices[2];
		//	model.indices.push_back(index1);
		//	model.indices.push_back(index2);
		//	model.indices.push_back(index3);
		//};
		//model.indicesSize = (uint32_t)(model.indices.size() * sizeof(model.indices[0]));
		#pragma endregion

		scene = GltfLoader::Load(std::filesystem::current_path().string() + R"(\models\monkey.glb)");

		struct StagingBuffer {
			VkDeviceMemory memory;
			VkBuffer buffer;
		};

		struct {
			StagingBuffer vertices;
			StagingBuffer indices;
		} stagingBuffers;

		// Allocate a command buffer for the copy operations to follow
		VkCommandBufferAllocateInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandPool = commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &cmdBufInfo, &copyCommandBuffer);

		// First copy vertices to host accessible vertex buffer memory
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = scene.meshes[0].faceIndices.size();
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		vkCreateBuffer(logicalDevice, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer);

		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		void* data;

		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffers.vertices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &stagingBuffers.vertices.memory);

		auto verticesSize = sizeof(decltype(scene.meshes[0].vertexPositions)::value_type) * scene.meshes[0].vertexPositions.size();
		vkMapMemory(logicalDevice, stagingBuffers.vertices.memory, 0, verticesSize, 0, &data);
		memcpy(data, scene.meshes[0].vertexPositions.data(), 3072);
		vkUnmapMemory(logicalDevice, stagingBuffers.vertices.memory);
		vkBindBufferMemory(logicalDevice, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0);

		// Then allocate a gpu only buffer for vertices
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logicalDevice, &vertexBufferInfo, nullptr, &vertexBuffer);
		vkGetBufferMemoryRequirements(logicalDevice, vertexBuffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &vertexBufferMemory);
		vkBindBufferMemory(logicalDevice, vertexBuffer, vertexBufferMemory, 0);

		// Next copy indices to host accessible index buffer memory
		VkBufferCreateInfo indexBufferInfo = {};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = scene.meshes[0].faceIndices.size();
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		vkCreateBuffer(logicalDevice, &indexBufferInfo, nullptr, &stagingBuffers.indices.buffer);
		vkGetBufferMemoryRequirements(logicalDevice, stagingBuffers.indices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &stagingBuffers.indices.memory);

		auto indicesSize = sizeof(decltype(scene.meshes[0].faceIndices)::value_type) * scene.meshes[0].faceIndices.size();
		vkMapMemory(logicalDevice, stagingBuffers.indices.memory, 0, scene.meshes[0].faceIndices.size(), 0, &data);
		memcpy(data, scene.meshes[0].faceIndices.data(), scene.meshes[0].faceIndices.size());
		vkUnmapMemory(logicalDevice, stagingBuffers.indices.memory);
		vkBindBufferMemory(logicalDevice, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0);

		// And allocate another gpu only buffer for indices
		indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(logicalDevice, &indexBufferInfo, nullptr, &indexBuffer);
		vkGetBufferMemoryRequirements(logicalDevice, indexBuffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &indexBufferMemory);
		vkBindBufferMemory(logicalDevice, indexBuffer, indexBufferMemory, 0);

		// Now copy data from host visible buffer to gpu only buffer
		VkCommandBufferBeginInfo bufferBeginInfo = {};
		bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(copyCommandBuffer, &bufferBeginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.size = scene.meshes[0].vertexPositions.size();
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.vertices.buffer, vertexBuffer, 1, &copyRegion);
		copyRegion.size = scene.meshes[0].faceIndices.size();
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.indices.buffer, indexBuffer, 1, &copyRegion);

		vkEndCommandBuffer(copyCommandBuffer);

		// Submit to queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);

		vkDestroyBuffer(logicalDevice, stagingBuffers.vertices.buffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBuffers.vertices.memory, nullptr);
		vkDestroyBuffer(logicalDevice, stagingBuffers.indices.buffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBuffers.indices.memory, nullptr);

		std::cout << "set up vertex and index buffers" << std::endl;

		// Binding and attribute descriptions
		vertexBindingDescription.binding = 0;
		vertexBindingDescription.stride = sizeof(decltype(scene.meshes[0].vertexPositions)::value_type);
		vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// vec3 position
		vertexAttributeDescriptions.resize(1);
		vertexAttributeDescriptions[0].binding = 0;
		vertexAttributeDescriptions[0].location = 0;
		vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	}

	void createUniformBuffer() {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(uniformBufferData);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &uniformBuffer);

		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(logicalDevice, uniformBuffer, &memReqs);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &allocInfo.memoryTypeIndex);

		vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &uniformBufferMemory);
		vkBindBufferMemory(logicalDevice, uniformBuffer, uniformBufferMemory, 0);

		updateUniformData();
	}

	void updateUniformData() {
		// Get the mouse x and y. These values are stored cumulatively and will be used as degrees of rotation when calculating the cameraForward vector
		float yaw = Input::Instance().mouseX * mouseSensitivity;
		float pitch = Input::Instance().mouseY * mouseSensitivity;

		// Calculate the camera vectors
		glm::vec3 cameraForward;
		cameraForward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch)); // Use yaw and pitch as degrees for calculation
		cameraForward.y = sin(glm::radians(pitch));
		cameraForward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		glm::vec3 cameraPosition = mainCamera.position;
		glm::vec3 cameraRight = glm::cross(cameraForward, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec3 cameraUp = glm::cross(cameraRight, cameraForward);
		glm::normalize(cameraForward);
		glm::normalize(cameraRight);
		glm::normalize(cameraUp);

		// Create a transformation matrix that maps the world's X to cameraRight, the world's Y to cameraUp and the world's Z to cameraForward
		// This way the vertex shader will put the vertices in the correct position by multiplying each vertex's position by the resulting matrix
		mainCamera.view = glm::lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp);

		if (input.IsKeyHeldDown("w")) {
			//std::cout << "w key is being held down\n";
			mainCamera.position += cameraForward * 0.1f;
		}

		if (input.IsKeyHeldDown("a")) {
			//std::cout << "a key is being held down\n";
			mainCamera.position += -cameraRight * 0.1f;
		}

		if (input.IsKeyHeldDown("s")) {
			//std::cout << "s key is being held down\n";
			mainCamera.position += -cameraForward * 0.1f;
		}

		if (input.IsKeyHeldDown("d")) {
			//std::cout << "d key is being held down\n";
			mainCamera.position += cameraRight * 0.1f;
		}

		//std::cout << mainCamera.position.x << ", " << mainCamera.position.y << ", " << mainCamera.position.z << std::endl;

		//std::cout << "Mouse x is " << Input::Instance().mouseX << std::endl;
		//std::cout << "Mouse y is " << Input::Instance().mouseY << std::endl;
		//mainCamera.view = glm::rotate(mainCamera.view, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
		//mainCamera.view = glm::rotate(mainCamera.view, (float)input.mouseX * mouseSensitivity, glm::vec3(0.0f, 1.0f, 0.0f));
		//mainCamera.view = glm::rotate(mainCamera.view, (float)input.mouseY * mouseSensitivity, glm::vec3(1.0f, 0.0f, 0.0f));
		//modelMatrix = glm::rotate(modelMatrix, glm::radians(0.1f), glm::vec3(0.0f, 1.0f, 0.0f));
		//glm::translate(modelMatrix, glm::vec3(0.0f, 0.0f, -0.1f));

		// Generate the projection matrix. This matrix maps the position in camera space to 2D screen space.
		mainCamera.projection = glm::perspective(glm::radians(60.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 1000.0f);
		uniformBufferData.transformationMatrix = mainCamera.projection * mainCamera.view * modelMatrix;

		// Send the uniform buffer data (which contains the combined transformation matrices) to the GPU
		void* data;
		vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(uniformBufferData), 0, &data);
		memcpy(data, &uniformBufferData, sizeof(uniformBufferData));
		vkUnmapMemory(logicalDevice, uniformBufferMemory);
	}

	// Find device memory that is supported by the requirements (typeBits) and meets the desired properties
	VkBool32 getMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex) {
		for (uint32_t i = 0; i < 32; i++) {
			if ((typeBits & 1) == 1) {
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					*typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		return false;
	}

	void createSwapChain() {
		// Find surface capabilities
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, windowSurface, &surfaceCapabilities) != VK_SUCCESS) {
			std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
			exit(1);
		}

		// Find supported surface formats
		uint32_t formatCount;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
			std::cerr << "failed to get number of supported surface formats" << std::endl;
			exit(1);
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, windowSurface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported surface formats" << std::endl;
			exit(1);
		}

		// Find supported present modes
		uint32_t presentModeCount;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
			std::cerr << "failed to get number of supported presentation modes" << std::endl;
			exit(1);
		}

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, windowSurface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
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
		VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(surfaceFormats);

		// Select swap chain size
		swapChainExtent = chooseSwapExtent(surfaceCapabilities);

		// Determine transformation to use (preferring no transform)
		VkSurfaceTransformFlagBitsKHR surfaceTransform;
		if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			surfaceTransform = surfaceCapabilities.currentTransform;
		}

		// Choose presentation mode (preferring MAILBOX ~= triple buffering)
		VkPresentModeKHR presentMode = choosePresentMode(presentModes);

		// Finally, create the swap chain
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapChainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapChain;

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			std::cerr << "failed to create swap chain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swap chain" << std::endl;
		}

		if (oldSwapChain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(logicalDevice, oldSwapChain, nullptr);
		}
		oldSwapChain = swapChain;

		swapChainFormat = surfaceFormat.format;

		// Store the images used by the swap chain
		// Note: these are the images that swap chain image indices refer to
		// Note: actual number of images may differ from requested number, since it's a lower bound
		uint32_t actualImageCount = 0;
		if (vkGetSwapchainImagesKHR(logicalDevice, swapChain, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}

		swapChainImages.resize(actualImageCount);

		if (vkGetSwapchainImagesKHR(logicalDevice, swapChain, &actualImageCount, swapChainImages.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swap chain images" << std::endl;
			exit(1);
		}

		std::cout << "acquired swap chain images" << std::endl;
	}

	VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
		if (surfaceCapabilities.currentExtent.width == -1) {
			VkExtent2D swapChainExtent = {};

			swapChainExtent.width = std::min(std::max(WIDTH, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
			swapChainExtent.height = std::min(std::max(HEIGHT, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);

			return swapChainExtent;
		}
		else {
			return surfaceCapabilities.currentExtent;
		}
	}

	VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> presentModes) {
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	void createRenderPass() {
		VkAttachmentDescription attachmentDescription = {};
		attachmentDescription.format = swapChainFormat;
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

		if (vkCreateRenderPass(logicalDevice, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
	}

	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());

		// Create an image view for every image in the swap chain
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created image views for swap chain images" << std::endl;
	}

	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImages.size());

		// Note: Framebuffer is basically a specific choice of attachments for a render pass
		// That means all attachments must have the same dimensions, interesting restriction
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = &swapChainImageViews[i];
			createInfo.width = swapChainExtent.width;
			createInfo.height = swapChainExtent.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &createInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created framebuffers for swap chain image views" << std::endl;
	}

	VkShaderModule createShaderModule(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		VkShaderModule shaderModule = nullptr;
		if (file.is_open()) {
			std::vector<char> fileBytes(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(fileBytes.data(), fileBytes.size());
			file.close();

			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = fileBytes.size();
			createInfo.pCode = (uint32_t*)fileBytes.data();

			if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				std::cerr << "failed to create shader module for " << filename << std::endl;
				exit(1);
			}

			std::cout << "created shader module for " << filename << std::endl;
		}
		else { std::cout << "failed to open file " + filename << std::endl; exit(0); }

		return shaderModule;
	}

	void createGraphicsPipeline() {
		VkShaderModule vertexShaderModule = createShaderModule(std::filesystem::current_path().string() + R"(\src\engine\VertexShader.spv)");
		VkShaderModule fragmentShaderModule = createShaderModule(std::filesystem::current_path().string() + R"(\src\engine\FragmentShader.spv)");

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
		vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

		// Describe input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// Describe viewport and scissor
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = swapChainExtent.width;
		scissor.extent.height = swapChainExtent.height;

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

		if (vkCreateDescriptorSetLayout(logicalDevice, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor layout" << std::endl;
		}

		VkPipelineLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 1;
		layoutCreateInfo.pSetLayouts = &descriptorSetLayout;

		if (vkCreatePipelineLayout(logicalDevice, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
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
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		// No longer necessary
		vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
	}

	void createDescriptorPool() {
		// This describes how many descriptor sets we'll create from this pool for each type
		VkDescriptorPoolSize typeCount;
		typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCount.descriptorCount = 1;

		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = &typeCount;
		createInfo.maxSets = 1;

		if (vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor pool" << std::endl;
		}
	}

	void createDescriptorSets() {
		// There needs to be one descriptor set per binding point in the shader
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set" << std::endl;
		}

		// Update descriptor set with uniform binding
		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = uniformBuffer;
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(uniformBufferData);

		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	void createCommandBuffers() {
		// Allocate graphics command buffers
		graphicsCommandBuffers.resize(swapChainImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)swapChainImages.size();

		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
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
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			vkBeginCommandBuffer(graphicsCommandBuffers[i], &beginInfo);

			// If present queue family and graphics queue family are different, then a barrier is necessary
			// The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier presentToDrawBarrier = {};
			presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presentToDrawBarrier.srcAccessMask = 0;
			presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			if (presentQueueFamily != graphicsQueueFamily) {
				presentToDrawBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				presentToDrawBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}
			else {
				presentToDrawBarrier.srcQueueFamilyIndex = presentQueueFamily;
				presentToDrawBarrier.dstQueueFamilyIndex = graphicsQueueFamily;
			}

			presentToDrawBarrier.image = swapChainImages[i];
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = swapChainExtent;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(graphicsCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindDescriptorSets(graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(graphicsCommandBuffers[i], 0, 1, &vertexBuffer, &offset);
			vkCmdBindIndexBuffer(graphicsCommandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(graphicsCommandBuffers[i], scene.meshes[0].faceIndices.size(), 1, 0, 0, 0);
			vkCmdEndRenderPass(graphicsCommandBuffers[i]);

			// If present and graphics queue families differ, then another barrier is required
			if (presentQueueFamily != graphicsQueueFamily) {
				VkImageMemoryBarrier drawToPresentBarrier = {};
				drawToPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				drawToPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				drawToPresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				drawToPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.srcQueueFamilyIndex = graphicsQueueFamily;
				drawToPresentBarrier.dstQueueFamilyIndex = presentQueueFamily;
				drawToPresentBarrier.image = swapChainImages[i];
				drawToPresentBarrier.subresourceRange = subResourceRange;

				vkCmdPipelineBarrier(graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &drawToPresentBarrier);
			}

			if (vkEndCommandBuffer(graphicsCommandBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
			}
		}

		std::cout << "recorded command buffers" << std::endl;

		// No longer needed
		vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	}

	void draw() {
		// Acquire image
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		// Unless surface is out of date right now, defer swap chain recreation until end of this frame
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			onWindowSizeChanged();
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
		submitInfo.pWaitSemaphores = &imageAvailableSemaphore;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderingFinishedSemaphore;

		// This is the stage where the queue should wait on the semaphore
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsCommandBuffers[imageIndex];

		if (auto res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
			std::cerr << "failed to submit draw command buffer" << std::endl;
			exit(1);
		}

		// Present drawn image
		// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderingFinishedSemaphore;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
			onWindowSizeChanged();
		}
		else if (res != VK_SUCCESS) {
			std::cerr << "failed to submit present command buffer" << std::endl;
			exit(1);
		}
	}
};

int main() {
	VulkanApplication app;
	app.run();

	return 0;
}
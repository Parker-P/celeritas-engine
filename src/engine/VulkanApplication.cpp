#define GLFW_INCLUDE_VULKAN

#include <windows.h>

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

// Project local classes
#include "Singleton.hpp"
#include "Time.hpp"
#include "Input.hpp"
#include "Transform.hpp"
#include "GameObject.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "Utils.hpp"
#include "GltfLoader.hpp"

// Configuration
const uint32_t WIDTH = 640;
const uint32_t HEIGHT = 480;
const bool enableValidationLayers = true;
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
		_timeStart = std::chrono::high_resolution_clock::now();
	}

	void Run() {
		// Note: dynamically loading loader may be a better idea to fail gracefully when Vulkan is not supported

		// Create window for Vulkan
		glfwInit();


		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		_window = glfwCreateWindow(WIDTH, HEIGHT, "Solar System Explorer", nullptr, nullptr);

		_input = Input::Instance();
		_input.Init(_window);

		glfwSetWindowSizeCallback(_window, VulkanApplication::OnWindowResized);

		// Use Vulkan
		SetupVulkan();
		system("cls");
		MainLoop();;

		Cleanup(true);
	}

private:
	// Window
	GLFWwindow* _window;

	// Basic vulkan stuff
	VkInstance							_instance;
	VkSurfaceKHR						_windowSurface;
	VkPhysicalDevice					_physicalDevice;
	VkDevice							_logicalDevice;
	VkDebugReportCallbackEXT			_callback;
	VkQueue								_graphicsQueue;
	uint32_t							_graphicsQueueFamily;
	VkQueue								_presentQueue;
	uint32_t							_presentQueueFamily;
	VkPhysicalDeviceMemoryProperties	_deviceMemoryProperties;
	VkSemaphore							_imageAvailableSemaphore;
	VkSemaphore							_renderingFinishedSemaphore;
	VkRenderPass						_renderPass;
	VkPipeline							_graphicsPipeline;

	// Vertex and index buffers
	VkBuffer										_vertexBuffer;
	VkDeviceMemory									_vertexBufferMemory;
	VkBuffer										_indexBuffer;
	VkDeviceMemory									_indexBufferMemory;
	VkVertexInputBindingDescription					_vertexBindingDescription;
	std::vector<VkVertexInputAttributeDescription>	_vertexAttributeDescriptions;

	// Shader resources (descriptor sets and push constants)
	VkBuffer				_uniformBuffer;
	VkDeviceMemory			_uniformBufferMemory;
	VkDescriptorSetLayout	_descriptorSetLayout;
	VkDescriptorPool		_descriptorPool;
	VkDescriptorSet			_descriptorSet;
	VkPipelineLayout		_pipelineLayout;
	struct {
		glm::mat4 transformationMatrix;
	}						_uniformBufferData;

	// Swap chain
	VkExtent2D					_swapChainExtent;
	VkFormat					_swapChainFormat;
	VkSwapchainKHR				_oldSwapChain;
	VkSwapchainKHR				_swapChain;
	std::vector<VkImage>		_swapChainImages;
	std::vector<VkImageView>	_swapChainImageViews;
	std::vector<VkFramebuffer>	_swapChainFramebuffers;

	// Vulkan commands
	VkCommandPool					_commandPool;
	std::vector<VkCommandBuffer>	_graphicsCommandBuffers;


	// Time
	std::chrono::high_resolution_clock::time_point	_timeStart;
	std::chrono::high_resolution_clock::time_point	_lastFrameTime; // Time last frame started
	double											_deltaTime;		// The time since last frame started in milliseconds

	// Misc
	Scene _scene;
	Input _input;
	Camera _mainCamera;
	glm::mat4 _modelMatrix;

	void CalculateDeltaTime() {
		auto tmp = (std::chrono::high_resolution_clock::now() - _lastFrameTime).count();
		_lastFrameTime = std::chrono::high_resolution_clock::now();
		Time::Instance()._deltaTime = tmp * 0.000001;
	}

	void SetupVulkan() {
		_oldSwapChain = VK_NULL_HANDLE;
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
		CreateSwapChain();
		CreateUniformBuffer();
		CreateRenderPass();
		CreateImageViews();
		CreateFramebuffers();
		CreateGraphicsPipeline();
		CreateDescriptorPool();
		CreateDescriptorSets();
		CreateCommandBuffers();
	}

	void MainLoop() {
		while (!glfwWindowShouldClose(_window)) {
			CalculateDeltaTime();
			UpdateUniformData();
			Draw();
			glfwPollEvents();
		}
	}

	static void OnWindowResized(GLFWwindow* window, int width, int height) {
		windowResized = true;
	}

	void OnWindowSizeChanged() {
		windowResized = false;

		// Only recreate objects that are affected by framebuffer size changes
		Cleanup(false);

		CreateSwapChain();
		CreateRenderPass();
		CreateImageViews();
		CreateFramebuffers();
		CreateGraphicsPipeline();
		CreateCommandBuffers();
	}

	void Cleanup(bool fullClean) {
		vkDeviceWaitIdle(_logicalDevice);

		vkFreeCommandBuffers(_logicalDevice, _commandPool, (uint32_t)_graphicsCommandBuffers.size(), _graphicsCommandBuffers.data());

		vkDestroyPipeline(_logicalDevice, _graphicsPipeline, nullptr);
		vkDestroyRenderPass(_logicalDevice, _renderPass, nullptr);

		for (size_t i = 0; i < _swapChainImages.size(); i++) {
			vkDestroyFramebuffer(_logicalDevice, _swapChainFramebuffers[i], nullptr);
			vkDestroyImageView(_logicalDevice, _swapChainImageViews[i], nullptr);
		}

		vkDestroyDescriptorSetLayout(_logicalDevice, _descriptorSetLayout, nullptr);

		if (fullClean) {
			vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(_logicalDevice, _renderingFinishedSemaphore, nullptr);

			vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

			// Clean up uniform buffer related objects
			vkDestroyDescriptorPool(_logicalDevice, _descriptorPool, nullptr);
			vkDestroyBuffer(_logicalDevice, _uniformBuffer, nullptr);
			vkFreeMemory(_logicalDevice, _uniformBufferMemory, nullptr);

			// Buffers must be destroyed after no command buffers are referring to them anymore
			vkDestroyBuffer(_logicalDevice, _vertexBuffer, nullptr);
			vkFreeMemory(_logicalDevice, _vertexBufferMemory, nullptr);
			vkDestroyBuffer(_logicalDevice, _indexBuffer, nullptr);
			vkFreeMemory(_logicalDevice, _indexBufferMemory, nullptr);

			// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly)
			vkDestroySwapchainKHR(_logicalDevice, _swapChain, nullptr);

			vkDestroyDevice(_logicalDevice, nullptr);

			vkDestroySurfaceKHR(_instance, _windowSurface, nullptr);

			if (enableValidationLayers) {
				PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
				DestroyDebugReportCallback(_instance, _callback, nullptr);
			}

			vkDestroyInstance(_instance, nullptr);
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

	void CreateInstance() {
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "VulkanClear";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "ClearScreenEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Get _instance extensions required by GLFW to draw to window
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

		// Initialize Vulkan _instance
		if (vkCreateInstance(&createInfo, nullptr, &_instance)) {
			std::cerr << "failed to create _instance!" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan _instance" << std::endl;
		}
	}

	void CreateWindowSurface() {
		if (glfwCreateWindowSurface(_instance, _window, NULL, &_windowSurface) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}

		std::cout << "created window surface" << std::endl;
	}

	void FindPhysicalDevice() {
		// Try to find 1 Vulkan supported device
		// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions
		uint32_t deviceCount = 0;
		if (vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
			std::cerr << "failed to get number of physical devices" << std::endl;
			exit(1);
		}

		deviceCount = 1;
		VkResult res = vkEnumeratePhysicalDevices(_instance, &deviceCount, &_physicalDevice);
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
		vkGetPhysicalDeviceProperties(_physicalDevice, &deviceProperties);
		vkGetPhysicalDeviceFeatures(_physicalDevice, &deviceFeatures);

		uint32_t supportedVersion[] = {
			VK_VERSION_MAJOR(deviceProperties.apiVersion),
			VK_VERSION_MINOR(deviceProperties.apiVersion),
			VK_VERSION_PATCH(deviceProperties.apiVersion)
		};

		std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;
	}

	void CheckSwapChainSupport() {
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "physical device doesn't support any extensions" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, deviceExtensions.data());

		for (const auto& extension : deviceExtensions) {
			if (strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				std::cout << "physical device supports swap chains" << std::endl;
				return;
			}
		}

		std::cerr << "physical device doesn't support swap chains" << std::endl;
		exit(1);
	}

	void FindQueueFamilies() {
		// Check queue families
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);

		if (queueFamilyCount == 0) {
			std::cout << "physical device has no queue families!" << std::endl;
			exit(1);
		}

		// Find queue family with graphics support
		// Note: is a transfer queue necessary to copy vertices to the gpu or can a graphics queue handle that?
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

		std::cout << "physical device has " << queueFamilyCount << " queue families" << std::endl;

		bool foundGraphicsQueueFamily = false;
		bool foundPresentQueueFamily = false;

		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, _windowSurface, &presentSupport);

			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				_graphicsQueueFamily = i;
				foundGraphicsQueueFamily = true;

				if (presentSupport) {
					_presentQueueFamily = i;
					foundPresentQueueFamily = true;
					break;
				}
			}

			if (!foundPresentQueueFamily && presentSupport) {
				_presentQueueFamily = i;
				foundPresentQueueFamily = true;
			}
		}

		if (foundGraphicsQueueFamily) {
			std::cout << "queue family #" << _graphicsQueueFamily << " supports graphics" << std::endl;

			if (foundPresentQueueFamily) {
				std::cout << "queue family #" << _presentQueueFamily << " supports presentation" << std::endl;
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

	void CreateLogicalDevice() {
		// Greate one graphics queue and optionally a separate presentation queue
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfo[2] = {};

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = _graphicsQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[0].queueFamilyIndex = _presentQueueFamily;
		queueCreateInfo[0].queueCount = 1;
		queueCreateInfo[0].pQueuePriorities = &queuePriority;

		// Create logical device from physical device
		// Note: there are separate _instance and device extensions!
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfo;

		if (_graphicsQueueFamily == _presentQueueFamily) {
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

		if (vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
			std::cerr << "failed to create logical device" << std::endl;
			exit(1);
		}

		std::cout << "created logical device" << std::endl;

		// Get graphics and presentation queues (which may be the same)
		vkGetDeviceQueue(_logicalDevice, _graphicsQueueFamily, 0, &_graphicsQueue);
		vkGetDeviceQueue(_logicalDevice, _presentQueueFamily, 0, &_presentQueue);

		std::cout << "acquired graphics and presentation queues" << std::endl;

		vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_deviceMemoryProperties);
	}

	void CreateDebugCallback() {
		if (enableValidationLayers) {
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugCallback;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

			PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugReportCallbackEXT");

			if (CreateDebugReportCallback(_instance, &createInfo, nullptr, &_callback) != VK_SUCCESS) {
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

	void CreateSemaphores() {
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_imageAvailableSemaphore) != VK_SUCCESS ||
			vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_renderingFinishedSemaphore) != VK_SUCCESS) {
			std::cerr << "failed to create semaphores" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created semaphores" << std::endl;
		}
	}

	void CreateCommandPool() {
		// Create graphics command pool
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = _graphicsQueueFamily;

		if (vkCreateCommandPool(_logicalDevice, &poolCreateInfo, nullptr, &_commandPool) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	void CreateVertexAndIndexBuffers() {

		#pragma region PreviousImplementation
		//// Create an _instance of the Importer class
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

		#pragma region SceneLoading
		_scene = GltfLoader::Load(std::filesystem::current_path().string() + R"(\models\monkey.glb)");
		auto vertexPositionsSize = Utils::GetVectorSizeInBytes(_scene._meshes[0]._vertexPositions);
		auto faceIndicesSize = Utils::GetVectorSizeInBytes(_scene._meshes[0]._faceIndices);
		#pragma endregion

		#pragma region Prep
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
		cmdBufInfo.commandPool = _commandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufInfo.commandBufferCount = 1;

		VkCommandBuffer copyCommandBuffer;
		vkAllocateCommandBuffers(_logicalDevice, &cmdBufInfo, &copyCommandBuffer);
		#pragma endregion

		#pragma region VertexPositionsToGpu
		// First copy vertices to host accessible vertex buffer memory
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = vertexPositionsSize;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		vkCreateBuffer(_logicalDevice, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer);

		VkMemoryAllocateInfo memAlloc = {};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		void* data;

		vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffers.vertices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &stagingBuffers.vertices.memory);

		vkMapMemory(_logicalDevice, stagingBuffers.vertices.memory, 0, vertexPositionsSize, 0, &data);
		memcpy(data, _scene._meshes[0]._vertexPositions.data(), vertexPositionsSize);
		vkUnmapMemory(_logicalDevice, stagingBuffers.vertices.memory);
		vkBindBufferMemory(_logicalDevice, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0);

		// Then allocate a gpu only buffer for vertices
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(_logicalDevice, &vertexBufferInfo, nullptr, &_vertexBuffer);
		vkGetBufferMemoryRequirements(_logicalDevice, _vertexBuffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &_vertexBufferMemory);
		vkBindBufferMemory(_logicalDevice, _vertexBuffer, _vertexBufferMemory, 0);

		// Next copy indices to host accessible index buffer memory
		VkBufferCreateInfo indexBufferInfo = {};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = faceIndicesSize;
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		#pragma endregion

		#pragma region FaceIndicesToGpu
		vkCreateBuffer(_logicalDevice, &indexBufferInfo, nullptr, &stagingBuffers.indices.buffer);
		vkGetBufferMemoryRequirements(_logicalDevice, stagingBuffers.indices.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &stagingBuffers.indices.memory);

		vkMapMemory(_logicalDevice, stagingBuffers.indices.memory, 0, faceIndicesSize, 0, &data);
		memcpy(data, _scene._meshes[0]._faceIndices.data(), faceIndicesSize);
		vkUnmapMemory(_logicalDevice, stagingBuffers.indices.memory);
		vkBindBufferMemory(_logicalDevice, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0);

		// And allocate another gpu only buffer for indices
		indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		vkCreateBuffer(_logicalDevice, &indexBufferInfo, nullptr, &_indexBuffer);
		vkGetBufferMemoryRequirements(_logicalDevice, _indexBuffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex);
		vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &_indexBufferMemory);
		vkBindBufferMemory(_logicalDevice, _indexBuffer, _indexBufferMemory, 0);

		// Now copy data from host visible buffer to gpu only buffer
		VkCommandBufferBeginInfo bufferBeginInfo = {};
		bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		#pragma endregion

		#pragma region CommandBufferCreationAndExecution
		vkBeginCommandBuffer(copyCommandBuffer, &bufferBeginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.size = vertexPositionsSize;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.vertices.buffer, _vertexBuffer, 1, &copyRegion);
		copyRegion.size = faceIndicesSize;
		vkCmdCopyBuffer(copyCommandBuffer, stagingBuffers.indices.buffer, _indexBuffer, 1, &copyRegion);

		vkEndCommandBuffer(copyCommandBuffer);

		// Submit to queue
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCommandBuffer;

		vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(_graphicsQueue);

		vkFreeCommandBuffers(_logicalDevice, _commandPool, 1, &copyCommandBuffer);
		#pragma endregion
		
		vkDestroyBuffer(_logicalDevice, stagingBuffers.vertices.buffer, nullptr);
		vkFreeMemory(_logicalDevice, stagingBuffers.vertices.memory, nullptr);
		vkDestroyBuffer(_logicalDevice, stagingBuffers.indices.buffer, nullptr);
		vkFreeMemory(_logicalDevice, stagingBuffers.indices.memory, nullptr);

		std::cout << "set up vertex and index buffers" << std::endl;
	}

	void CreateUniformBuffer() {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(_uniformBufferData);
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		vkCreateBuffer(_logicalDevice, &bufferInfo, nullptr, &_uniformBuffer);

		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(_logicalDevice, _uniformBuffer, &memReqs);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &allocInfo.memoryTypeIndex);

		vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &_uniformBufferMemory);
		vkBindBufferMemory(_logicalDevice, _uniformBuffer, _uniformBufferMemory, 0);

		UpdateUniformData();
	}

	void clearscreen()
	{
		HANDLE hOut;
		COORD Position;

		hOut = GetStdHandle(STD_OUTPUT_HANDLE);

		Position.X = 0;
		Position.Y = 0;
		SetConsoleCursorPosition(hOut, Position);
	}

	void UpdateUniformData() {

		/*
		// Steps:
		// 1) Get the values of yaw, pitch and roll on a frame to frame basis (the deltas of each frame)
		// 2) (Maybe) Store camera vectors in world space inside the camera class
		// 3) Modify the camera vectors thinking in a frame-by-frame way using the deltas instead of the cumulative values
		// 4) Store the accumulated rotation in a matrix

		// ****** STEP 1 ******

		// Get the mouse x and y. These values are stored cumulatively and will be used as degrees of rotation when calculating the cameraForward vector
		_yaw = Input::Instance()._mouseX * _mouseSensitivity; // According to the right hand rule, rotating left is positive along the Y axis, but going left with the mouse gives you a negative value. Because we will be using this value as degrees of rotation, we want to negate it.
		_pitch = Input::Instance()._mouseY * _mouseSensitivity; // Same as above. Around the X axis (for pitch) the positive rotation is looking upwards

		if (_input.IsKeyHeldDown("q")) {
			//std::cout << "d key is being held down\n";
			_roll -= 1.0f * 0.1f * (float)_deltaTime;
		}

		if (_input.IsKeyHeldDown("e")) {
			//std::cout << "d key is being held down\n";
			_roll += 1.0f * 0.1f * (float)_deltaTime;
		}

		_deltaYaw = _yaw - _lastYaw;
		_deltaPitch = _pitch - _lastPitch;
		_deltaRoll = _roll - _lastRoll;

		_lastYaw = _yaw;
		_lastPitch = _pitch;
		_lastRoll = _roll;

		// ****** STEP 2 (Maybe) ******
		// Blank

		// ****** STEP 3 ******

		// First apply roll rotation
		_mainCamera._cameraForward = glm::vec3(_mainCamera._rotation[0][2], _mainCamera._rotation[1][2], _mainCamera._rotation[2][2]);
		_mainCamera._rotation = glm::rotate(_mainCamera._rotation, glm::radians(_deltaRoll), _mainCamera._cameraForward);

		// Then apply yaw rotation
		_mainCamera._cameraUp = glm::vec3(_mainCamera._rotation[0][1], _mainCamera._rotation[1][1], _mainCamera._rotation[2][1]);
		_mainCamera._rotation = glm::rotate(_mainCamera._rotation, glm::radians(_deltaYaw), _mainCamera._cameraUp);

		// Then pitch
		_mainCamera._cameraRight = glm::vec3(_mainCamera._rotation[0][0], _mainCamera._rotation[1][0], _mainCamera._rotation[2][0]);
		_mainCamera._rotation = glm::rotate(_mainCamera._rotation, glm::radians(_deltaPitch), _mainCamera._cameraRight);

		_mainCamera._cameraUp = glm::vec3(_mainCamera._rotation[0][1], _mainCamera._rotation[1][1], _mainCamera._rotation[2][1]);
		_mainCamera._cameraForward = glm::vec3(_mainCamera._rotation[0][2], _mainCamera._rotation[1][2], _mainCamera._rotation[2][2]);
		_mainCamera._cameraRight = glm::vec3(_mainCamera._rotation[0][0], _mainCamera._rotation[1][0], _mainCamera._rotation[2][0]);

		//std::cout << "Camera right is (" << _mainCamera._cameraRight.x << "," << _mainCamera._cameraRight.y << "," << _mainCamera._cameraRight.z << ")" << std::endl;
		*/
		#pragma region PreviousImpl

		// Calculate the cameraForward vector based on yaw and pitch
		//glm::vec3 cameraForward;
		//cameraForward.x = sin(glm::radians(yaw)) * cos(glm::radians(pitch)); // Use yaw and pitch as degrees for calculation
		//cameraForward.y = sin(glm::radians(pitch));
		//cameraForward.z = -(cos(glm::radians(yaw)) * cos(glm::radians(pitch)));

		//// Calculate the cameraUp vector based on yaw, pitch and roll
		//glm::vec3 cameraUp;
		//cameraUp.x = sin(glm::radians(_mainCamera._roll)) * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
		//cameraUp.y = cos(glm::radians(_mainCamera._roll)) * cos(glm::radians(pitch));
		//cameraUp.z = sin(glm::radians(pitch)) * sin(glm::radians(_mainCamera._roll)) * sin(glm::radians(yaw));

		//auto cameraRight = glm::cross(cameraForward, cameraUp);

		//glm::normalize(cameraForward);
		//glm::normalize(cameraUp);

		//std::cout << "Yaw is " << yaw << std::endl;
		//std::cout << "Pitch is " << pitch << std::endl;
		//std::cout << "Roll is " << _mainCamera._roll << std::endl;

		//std::cout << "cameraForward is (" << cameraForward.x << "," << cameraForward.y << "," << cameraForward.z << ")" << std::endl;
		//std::cout << "cameraUp is (" << cameraUp.x << "," << cameraUp.y << "," << cameraUp.z << ")" << std::endl;

		//auto cross = glm::cross(cameraForward, cameraUp);
		//std::cout << "Cross product is (" << cross.x << "," << cross.y << "," << cross.z << ")" << std::endl;

		//auto dot = glm::dot(cameraForward, cameraUp);
		//std::cout << "Dot product is " << dot << std::endl;

		//clearscreen();

		//cameraUp = glm::normalize(cameraUp);

		//auto cameraPosition = _mainCamera._position;

		////std::cout << _mainCamera._roll << std::endl;
		#pragma endregion

		_mainCamera.Update();

		// Create a transformation matrix that maps the world's X to cameraRight, the world's Y to cameraUp and the world's Z to cameraForward
		// This way the vertex shader will put the vertices in the correct position by multiplying each vertex's position by the resulting matrix

		/*std::cout << _mainCamera._position.x << ", " << _mainCamera._position.y << ", " << _mainCamera._position.z << std::endl;
		std::cout << "Mouse x is " << Input::Instance()._mouseX << std::endl;
		std::cout << "Mouse y is " << Input::Instance()._mouseY << std::endl;*/

		// Generate the projection matrix. This matrix maps the position in camera space to 2D screen space.
		auto aspectRatio = std::bit_cast<float, uint32_t>(_swapChainExtent.width) / std::bit_cast<float, uint32_t>(_swapChainExtent.height);
		_mainCamera._projection.SetTransformation(glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f));
		_uniformBufferData.transformationMatrix = _mainCamera._projection.Transformation() * _mainCamera._view.Transformation() * _modelMatrix;

		// Send the uniform buffer data (which contains the combined transformation matrices) to the GPU
		void* data;
		vkMapMemory(_logicalDevice, _uniformBufferMemory, 0, sizeof(_uniformBufferData), 0, &data);
		memcpy(data, &_uniformBufferData, sizeof(_uniformBufferData));
		vkUnmapMemory(_logicalDevice, _uniformBufferMemory);
	}

	// Find device memory that is supported by the requirements (typeBits) and meets the desired properties
	VkBool32 GetMemoryType(uint32_t typeBits, VkFlags properties, uint32_t* typeIndex) {
		for (uint32_t i = 0; i < 32; i++) {
			if ((typeBits & 1) == 1) {
				if ((_deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					*typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		return false;
	}

	void CreateSwapChain() {
		// Find surface capabilities
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _windowSurface, &surfaceCapabilities) != VK_SUCCESS) {
			std::cerr << "failed to acquire presentation surface capabilities" << std::endl;
			exit(1);
		}

		// Find supported surface formats
		uint32_t formatCount;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _windowSurface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
			std::cerr << "failed to get number of supported surface formats" << std::endl;
			exit(1);
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _windowSurface, &formatCount, surfaceFormats.data()) != VK_SUCCESS) {
			std::cerr << "failed to get supported surface formats" << std::endl;
			exit(1);
		}

		// Find supported present modes
		uint32_t presentModeCount;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _windowSurface, &presentModeCount, nullptr) != VK_SUCCESS || presentModeCount == 0) {
			std::cerr << "failed to get number of supported presentation modes" << std::endl;
			exit(1);
		}

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _windowSurface, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
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
		_swapChainExtent = ChooseSwapExtent(surfaceCapabilities);

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
		createInfo.surface = _windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = _swapChainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = _oldSwapChain;

		if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapChain) != VK_SUCCESS) {
			std::cerr << "failed to create swap chain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swap chain" << std::endl;
		}

		if (_oldSwapChain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(_logicalDevice, _oldSwapChain, nullptr);
		}
		_oldSwapChain = _swapChain;

		_swapChainFormat = surfaceFormat.format;

		// Store the images used by the swap chain
		// Note: these are the images that swap chain image indices refer to
		// Note: actual number of images may differ from requested number, since it's a lower bound
		uint32_t actualImageCount = 0;
		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}

		_swapChainImages.resize(actualImageCount);

		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapChain, &actualImageCount, _swapChainImages.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swap chain images" << std::endl;
			exit(1);
		}

		std::cout << "acquired swap chain images" << std::endl;
	}

	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
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

	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes) {
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	void CreateRenderPass() {
		VkAttachmentDescription attachmentDescription = {};
		attachmentDescription.format = _swapChainFormat;
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

		if (vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
	}

	void CreateImageViews() {
		_swapChainImageViews.resize(_swapChainImages.size());

		// Create an image view for every image in the swap chain
		for (size_t i = 0; i < _swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = _swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = _swapChainFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(_logicalDevice, &createInfo, nullptr, &_swapChainImageViews[i]) != VK_SUCCESS) {
				std::cerr << "failed to create image view for swap chain image #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created image views for swap chain images" << std::endl;
	}

	void CreateFramebuffers() {
		_swapChainFramebuffers.resize(_swapChainImages.size());

		// Note: Framebuffer is basically a specific choice of attachments for a render pass
		// That means all attachments must have the same dimensions, interesting restriction
		for (size_t i = 0; i < _swapChainImages.size(); i++) {
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = _renderPass;
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = &_swapChainImageViews[i];
			createInfo.width = _swapChainExtent.width;
			createInfo.height = _swapChainExtent.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &_swapChainFramebuffers[i]) != VK_SUCCESS) {
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

			if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				std::cerr << "failed to create shader module for " << filename << std::endl;
				exit(1);
			}

			std::cout << "created shader module for " << filename << std::endl;
		}
		else { std::cout << "failed to open file " + filename << std::endl; exit(0); }

		return shaderModule;
	}

	void CreateGraphicsPipeline() {
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

		// Binding and attribute descriptions
		_vertexBindingDescription.binding = 0;
		_vertexBindingDescription.stride = sizeof(_scene._meshes[0]._vertexPositions[0]);
		_vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Describe how the shader should read vertex attributes
		_vertexAttributeDescriptions.resize(1);
		_vertexAttributeDescriptions[0].binding = 0;
		_vertexAttributeDescriptions[0].location = 0;
		_vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;

		// Describe vertex input
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &_vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = _vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = _vertexAttributeDescriptions.data();

		// Describe input assembly
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// Describe viewport and scissor
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = 640.0f; // swapchainExtent wants uint32_t as value for both width and height. Need to find a way to convert from uint32_t to float. bit::cast is not working
		viewport.height = 480.0f;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _swapChainExtent.width;
		scissor.extent.height = _swapChainExtent.height;

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
		// Note: this describes the mapping between memory and shader resources (descriptor sets), which contain the information you want to send to the shaders
		// This is for uniform buffers and samplers
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
		descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayoutCreateInfo.bindingCount = 1;
		descriptorLayoutCreateInfo.pBindings = &layoutBinding;

		if (vkCreateDescriptorSetLayout(_logicalDevice, &descriptorLayoutCreateInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor layout" << std::endl;
		}

		VkPipelineLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 1;
		layoutCreateInfo.pSetLayouts = &_descriptorSetLayout;

		if (vkCreatePipelineLayout(_logicalDevice, &layoutCreateInfo, nullptr, &_pipelineLayout) != VK_SUCCESS) {
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
		pipelineCreateInfo.layout = _pipelineLayout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline) != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		// No longer necessary
		vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
	}

	void CreateDescriptorPool() {
		// This describes how many descriptor sets we'll create from this pool for each type
		VkDescriptorPoolSize typeCount;
		typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		typeCount.descriptorCount = 1;

		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = &typeCount;
		createInfo.maxSets = 1;

		if (vkCreateDescriptorPool(_logicalDevice, &createInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor pool" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor pool" << std::endl;
		}
	}

	void CreateDescriptorSets() {
		// There needs to be one descriptor set per binding point in the shader
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &_descriptorSetLayout;

		if (vkAllocateDescriptorSets(_logicalDevice, &allocInfo, &_descriptorSet) != VK_SUCCESS) {
			std::cerr << "failed to create descriptor set" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created descriptor set" << std::endl;
		}

		// Update descriptor set with uniform binding
		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = _uniformBuffer;
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(_uniformBufferData);

		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = _descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.dstBinding = 0;

		vkUpdateDescriptorSets(_logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	void CreateCommandBuffers() {
		// Allocate graphics command buffers
		_graphicsCommandBuffers.resize(_swapChainImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)_swapChainImages.size();

		if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _graphicsCommandBuffers.data()) != VK_SUCCESS) {
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
		for (size_t i = 0; i < _swapChainImages.size(); i++) {
			vkBeginCommandBuffer(_graphicsCommandBuffers[i], &beginInfo);

			// If present queue family and graphics queue family are different, then a barrier is necessary
			// The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier presentToDrawBarrier = {};
			presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presentToDrawBarrier.srcAccessMask = 0;
			presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			if (_presentQueueFamily != _graphicsQueueFamily) {
				presentToDrawBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				presentToDrawBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}
			else {
				presentToDrawBarrier.srcQueueFamilyIndex = _presentQueueFamily;
				presentToDrawBarrier.dstQueueFamilyIndex = _graphicsQueueFamily;
			}

			presentToDrawBarrier.image = _swapChainImages[i];
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(_graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = _renderPass;
			renderPassBeginInfo.framebuffer = _swapChainFramebuffers[i];
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = _swapChainExtent;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearColor;


			#pragma region RenderPassCommandRecording
			vkCmdBeginRenderPass(_graphicsCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindDescriptorSets(_graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1, &_descriptorSet, 0, nullptr);
			vkCmdBindPipeline(_graphicsCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(_graphicsCommandBuffers[i], 0, 1, &_vertexBuffer, &offset);

			// Get the value type of vertex indices. Typically unsigned int (32-bit) but could also be 16-bit. Gltf for example uses 16-bit.
			VkIndexType indexType = VK_INDEX_TYPE_NONE_KHR;
			if (std::is_same_v<decltype(_scene._meshes[0]._faceIndices)::value_type, unsigned short>) {
				indexType = VK_INDEX_TYPE_UINT16;
			}
			if (std::is_same_v<decltype(_scene._meshes[0]._faceIndices)::value_type, unsigned int>) {
				indexType = VK_INDEX_TYPE_UINT32;
			}
			vkCmdBindIndexBuffer(_graphicsCommandBuffers[i], _indexBuffer, 0, indexType);

			vkCmdDrawIndexed(_graphicsCommandBuffers[i], _scene._meshes[0]._faceIndices.size(), 1, 0, 0, 0);
			vkCmdEndRenderPass(_graphicsCommandBuffers[i]);
			#pragma endregion


			// If present and graphics queue families differ, then another barrier is required
			if (_presentQueueFamily != _graphicsQueueFamily) {
				VkImageMemoryBarrier drawToPresentBarrier = {};
				drawToPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				drawToPresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				drawToPresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
				drawToPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				drawToPresentBarrier.srcQueueFamilyIndex = _graphicsQueueFamily;
				drawToPresentBarrier.dstQueueFamilyIndex = _presentQueueFamily;
				drawToPresentBarrier.image = _swapChainImages[i];
				drawToPresentBarrier.subresourceRange = subResourceRange;

				vkCmdPipelineBarrier(_graphicsCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &drawToPresentBarrier);
			}

			if (vkEndCommandBuffer(_graphicsCommandBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
			}
		}

		std::cout << "recorded command buffers" << std::endl;

		// No longer needed
		vkDestroyPipelineLayout(_logicalDevice, _pipelineLayout, nullptr);
	}

	void Draw() {
		// Acquire image
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(_logicalDevice, _swapChain, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

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
		submitInfo.pWaitSemaphores = &_imageAvailableSemaphore;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &_renderingFinishedSemaphore;

		// This is the stage where the queue should wait on the semaphore
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &_graphicsCommandBuffers[imageIndex];

		if (auto res = vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
			std::cerr << "failed to submit draw command buffer" << std::endl;
			exit(1);
		}

		// Present drawn image
		// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &_renderingFinishedSemaphore;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &_swapChain;
		presentInfo.pImageIndices = &imageIndex;

		res = vkQueuePresentKHR(_presentQueue, &presentInfo);

		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
			OnWindowSizeChanged();
		}
		else if (res != VK_SUCCESS) {
			std::cerr << "failed to submit present command buffer" << std::endl;
			exit(1);
		}
	}
};

int main() {
	VulkanApplication app;
	app.Run();

	return 0;
}
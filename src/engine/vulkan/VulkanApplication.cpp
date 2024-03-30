#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <filesystem>
#include <map>
#include <bitset>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tinygltf/tiny_gltf.h>
#include "LocalIncludes.hpp"

namespace Engine::Vulkan
{
	bool windowResized = false;
	bool windowMinimized = false;

	void VulkanApplication::Run()
	{
		InitializeWindow();
		_input = Input::KeyboardMouse(_pWindow);
		SetupVulkan();
		MainLoop();
		Cleanup(true);
	}

	VkBool32 VulkanApplication::DebugCallback(VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t srcObject,
		size_t location,
		int32_t msgCode,
		const char* pLayerPrefix,
		const char* pMsg,
		void* pUserData)
	{
		if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
			Utils::Logger::Log("ERROR: [" + std::string(pLayerPrefix) + "] Code " + std::to_string(msgCode) + " : " + pMsg);
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
			Utils::Logger::Log("WARNING: [" + std::string(pLayerPrefix) + "] Code " + std::to_string(msgCode) + " : " + pMsg);
		}

		return VK_FALSE;
	}

	void VulkanApplication::InitializeWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		_pWindow = glfwCreateWindow(_settings._windowWidth, _settings._windowHeight, "Hold The Line!", nullptr, nullptr);
		glfwSetWindowSizeCallback(_pWindow, VulkanApplication::OnWindowResized);
	}

	void VulkanApplication::SetupVulkan()
	{
		_instance = CreateInstance();
		CreateDebugCallback(_instance);
		_windowSurface = CreateWindowSurface(_instance);
		_physicalDevice = CreatePhysicalDevice(_instance);

		auto flags = (VkQueueFlagBits)(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
		_queueFamilyIndex = FindQueueFamilyIndex(_physicalDevice, flags);
		if (PhysicalDevice::SupportsSurface(_physicalDevice, _queueFamilyIndex, _windowSurface) == false) {
			std::exit(1);
		}

		VkDeviceQueueCreateInfo graphicsQueueInfo{};
		float queuePriority = 1.0f;
		graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphicsQueueInfo.queueFamilyIndex = _queueFamilyIndex;
		graphicsQueueInfo.queueCount = 1;
		graphicsQueueInfo.pQueuePriorities = &queuePriority;

		_logicalDevice = CreateLogicalDevice(_physicalDevice, { graphicsQueueInfo });
		vkGetDeviceQueue(_logicalDevice, _queueFamilyIndex, 0, &_queue);
		_commandPool = CreateCommandPool(_logicalDevice, _queueFamilyIndex);

		LoadScene();
		LoadEnvironmentMap();
		CreateSwapchain();
		CreateRenderPass();
		CreateFramebuffers();
		CreatePipelineLayout();
		CreateGraphicsPipeline();
		AllocateDrawCommandBuffers();
		RecordDrawCommands();
		CreateSemaphores();
	}

	glm::vec3 RotateVector(glm::vec3 vectorToRotate, glm::vec3 axis, float angleDegrees) {
		auto angleRadians = glm::radians(angleDegrees);
		auto cosine = cos(angleRadians / 2.0f);
		auto sine = sin(angleRadians / 2.0f);
		glm::quat rotation(cosine, axis.x * sine, axis.y * sine, axis.z * sine);
		return rotation * vectorToRotate;
	}

	void VulkanApplication::MainLoop()
	{
		while (!glfwWindowShouldClose(_pWindow)) {
			Update();
			Draw();
			glfwPollEvents();
		}
	}

	void VulkanApplication::Update()
	{
		Time::Instance().Update();
		_input.Update();
		_mainCamera.Update();
		_scene.Update();
	}

	void VulkanApplication::OnWindowResized(GLFWwindow* window, int width, int height)
	{
		windowResized = true;

		if (width == 0 && height == 0) {
			windowMinimized = true;
			return;
		}

		windowMinimized = false;
		Settings::GlobalSettings::Instance()._windowWidth = width;
		Settings::GlobalSettings::Instance()._windowHeight = height;
	}

	void VulkanApplication::WindowSizeChanged()
	{
		windowResized = false;

		// Only recreate objects that are affected by framebuffer size changes.
		Cleanup(false);
		CreateSwapchain();
		CreateRenderPass();
		CreateFramebuffers();
		CreateGraphicsPipeline();
		AllocateDrawCommandBuffers();
		RecordDrawCommands();
		//vkDestroyPipelineLayout(_logicalDevice, _graphicsPipeline._shaderResources._pipelineLayout, nullptr);
	}

	void VulkanApplication::DestroyRenderPass()
	{
		//_renderPass._depthImage.Destroy();
		/*for (auto image : _renderPass.colorImages) {
			image.Destroy();
		}*/

		vkDestroyRenderPass(_logicalDevice, _renderPass._handle, nullptr);
	}

	void VulkanApplication::DestroySwapchain()
	{
		for (size_t i = 0; i < _swapchain._frameBuffers.size(); i++) {
			vkDestroyFramebuffer(_logicalDevice, _swapchain._frameBuffers[i], nullptr);
		}
	}

	void VulkanApplication::Cleanup(bool fullClean)
	{
		//vkDeviceWaitIdle(_logicalDevice);
		//vkFreeCommandBuffers(_logicalDevice, _commandPool, (uint32_t)_drawCommandBuffers.size(), _drawCommandBuffers.data());
		//vkDestroyPipeline(_logicalDevice, _graphicsPipeline._handle, nullptr);
		//DestroyRenderPass();
		//DestroySwapchain();

		//vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._cameraDataSet._layout, nullptr);
		////vkDestroyDescriptorSetLayout(_logicalDevice, _graphicsPipeline._shaderResources._samplerSet._layout, nullptr);

		//if (fullClean) {
		//	vkDestroySemaphore(_logicalDevice, _imageAvailableSemaphore, nullptr);
		//	vkDestroySemaphore(_logicalDevice, _renderingFinishedSemaphore, nullptr);
		//	DestroyRenderPass();
		//	vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);

		//	// Clean up uniform buffer related objects.
		//	vkDestroyDescriptorPool(_logicalDevice, _graphicsPipeline._shaderResources._cameraDataPool._handle, nullptr);
		//	_graphicsPipeline._shaderResources._cameraDataBuffer.Destroy();

		//	// Buffers must be destroyed after no command buffers are referring to them anymore.
		//	/*_graphicsPipeline._vertexBuffer.Destroy();
		//	_graphicsPipeline._indexBuffer.Destroy();*/

		//	// Note: implicitly destroys images (in fact, we're not allowed to do that explicitly).
		//	DestroySwapchain();

		//	vkDestroyDevice(_logicalDevice, nullptr);

		//	vkDestroySurfaceKHR(_instance, _windowSurface, nullptr);

		//	if (_settings._enableValidationLayers) {
		//		PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugReportCallbackEXT");
		//		DestroyDebugReportCallback(_instance, _callback, nullptr);
		//	}

		//	vkDestroyInstance(_instance, nullptr);
		//}
	}

	bool VulkanApplication::ValidationLayersSupported(const std::vector<const char*>& validationLayers)
	{
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

	VkInstance VulkanApplication::CreateInstance()
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hold The Line!";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Celeritas Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Get instance extensions required by GLFW to draw to the window.
		unsigned int glfwExtensionCount;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions;
		for (size_t i = 0; i < glfwExtensionCount; i++) {
			extensions.push_back(glfwExtensions[i]);
		}

		if (_settings._enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		uint32_t extensionCount = 0;
		CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

		if (extensionCount == 0) {
			Utils::Exit(1, "no extensions supported");
		}

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		CheckResult(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()));

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (_settings._enableValidationLayers) {
			if (ValidationLayersSupported(_settings._pValidationLayers)) {
				createInfo.enabledLayerCount = 1;
				createInfo.ppEnabledLayerNames = _settings._pValidationLayers.data();
			}
		}

		VkInstance outInstance;
		CheckResult(vkCreateInstance(&createInfo, nullptr, &outInstance));
		return outInstance;
	}

	VkSurfaceKHR VulkanApplication::CreateWindowSurface(VkInstance& instance)
	{
		VkSurfaceKHR outSurface;
		CheckResult(glfwCreateWindowSurface(instance, _pWindow, NULL, &outSurface));
		return outSurface;
	}

	VkPhysicalDevice VulkanApplication::CreatePhysicalDevice(VkInstance instance)
	{
		// Try to find 1 Vulkan supported device.
		// Note: perhaps refactor to loop through devices and find first one that supports all required features and extensions.
		uint32_t deviceCount = 0;
		CheckResult(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

		if (deviceCount <= 0) {
			Utils::Exit(1, "device count was zero");
		}

		deviceCount = 1;
		std::vector<VkPhysicalDevice> outDevice(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, outDevice.data());

		if (deviceCount <= 0) {
			Utils::Exit(1, "device count was zero");
		}

		// Check device features
		// Note: will apiVersion >= appInfo.apiVersion? Probably yes, but spec is unclear.
		/*VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(_handle, &deviceProperties);
		vkGetPhysicalDeviceFeatures(_handle, &deviceFeatures);

		uint32_t supportedVersion[] = {
			VK_VERSION_MAJOR(deviceProperties.apiVersion),
			VK_VERSION_MINOR(deviceProperties.apiVersion),
			VK_VERSION_PATCH(deviceProperties.apiVersion)
		};

		std::cout << "physical device supports version " << supportedVersion[0] << "." << supportedVersion[1] << "." << supportedVersion[2] << std::endl;*/

		return outDevice[0];
	}

	VkDevice VulkanApplication::CreateLogicalDevice(VkPhysicalDevice& physicalDevice, const std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos)
	{
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

		// Devices features to enable.
		VkPhysicalDeviceFeatures enabledFeatures = {};
		enabledFeatures.samplerAnisotropy = VK_TRUE;
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.shaderCullDistance = VK_TRUE;

		const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

		if (_settings._enableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = 1;
			deviceCreateInfo.ppEnabledLayerNames = _settings._pValidationLayers.data();
		}

		VkDevice outDevice;
		vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &outDevice);
		return outDevice;
	}

	void VulkanApplication::CreateDebugCallback(VkInstance& instance)
	{
		if (_settings._enableValidationLayers) {
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

			PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

			CheckResult(createDebugReportCallback(instance, &createInfo, nullptr, &_callback));
		}
	}

	void VulkanApplication::CreateSemaphores()
	{
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		CheckResult(vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_imageAvailableSemaphore));
		CheckResult(vkCreateSemaphore(_logicalDevice, &createInfo, nullptr, &_renderingFinishedSemaphore));
	}

	void LoadMaterials(VkDevice& logicalDevice, tinygltf::Model& gltfScene)
	{
		for (int i = 0; i < gltfScene.materials.size(); ++i) {
			Scenes::Material m;
			auto& baseColorTextureIndex = gltfScene.materials[i].pbrMetallicRoughness.baseColorTexture.index;
			m._name = gltfScene.materials[i].name;

			if (baseColorTextureIndex >= 0) {
				auto& baseColorImageIndex = gltfScene.textures[baseColorTextureIndex].source;
				auto& baseColorImageData = gltfScene.images[baseColorImageIndex].image;
				unsigned char* copiedImageData = new unsigned char[baseColorImageData.size()];
				memcpy(copiedImageData, baseColorImageData.data(), baseColorImageData.size());
				auto size = VkExtent2D{ (uint32_t)gltfScene.images[baseColorImageIndex].width, (uint32_t)gltfScene.images[baseColorImageIndex].height };

				auto& imageCreateInfo = m._albedo._createInfo;
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.extent = { (uint32_t)size.width, (uint32_t)size.height, 1 };
				imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				CheckResult(vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &m._albedo._image));

				// Allocate memory on the GPU for the image.
				VkMemoryRequirements reqs;
				vkGetImageMemoryRequirements(_logicalDevice, m._albedo._image, &reqs);
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = reqs.size;
				allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(_physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				VkDeviceMemory mem;
				CheckResult(vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &mem));
				CheckResult(vkBindImageMemory(_logicalDevice, m._albedo._image, mem, 0));

				auto& imageViewCreateInfo = m._albedo._viewCreateInfo;
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
				imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				imageViewCreateInfo.image = m._albedo._image;
				imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
				imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
				imageViewCreateInfo.subresourceRange.layerCount = 1;
				imageViewCreateInfo.subresourceRange.levelCount = 1;
				CheckResult(vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &m._albedo._view));

				auto& samplerCreateInfo = m._albedo._samplerCreateInfo;
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
				samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &m._albedo._sampler);

				m._albedo._pData = copiedImageData;
				m._albedo._sizeBytes = baseColorImageData.size();

				_scene._materials.push_back(m);
			}
		}
	}

	void VulkanApplication::LoadScene()
	{
		_scene = Scenes::Scene(_logicalDevice, _physicalDevice);
		_scene._pointLights.push_back(Scenes::PointLight("DefaultLight"));

		tinygltf::Model gltfScene;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		//auto scenePath = Settings::Paths::ModelsPath() /= "MaterialSphere.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "cubes.glb";
		auto scenePath = Settings::Paths::ModelsPath() /= "directions.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "mp5k.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "Cube.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "stanford_dragon_pbr.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "SampleMap.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "monster.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "free_1972_datsun_4k_textures.glb";
		bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, scenePath.string());
		std::cout << warn << std::endl;
		std::cout << err << std::endl;

		std::map<unsigned int, unsigned int> loadedMaterialCache; // Gltf material index, index of the materials in scene._materials.

		LoadMaterials(_logicalDevice, gltfScene);

		for (int i = 0; i < gltfScene.nodes.size(); ++i) {
			if (gltfScene.nodes[i].mesh < 0) {
				continue;
			}

			auto& gltfMesh = gltfScene.meshes[gltfScene.nodes[i].mesh];

			for (auto& gltfPrimitive : gltfMesh.primitives) {
				auto gameObject = Scenes::GameObject(gltfScene.nodes[i].name, &_scene);
				auto& position = gltfScene.nodes[i].translation;
				auto& scale = gltfScene.nodes[i].scale;

				if (position.size() == 3) {
					gameObject._transform.SetPosition(glm::vec3{ position[0], position[1], position[2] });
				}

				/*if (scale.size() == 3) {
					gameObject._transform.SetScale(glm::vec3{ scale[0], scale[1], scale[2] });
				}*/

				auto faceIndicesAccessorIndex = gltfPrimitive.indices;
				auto vertexPositionsAccessorIndex = gltfPrimitive.attributes["POSITION"];
				auto vertexNormalsAccessorIndex = gltfPrimitive.attributes["NORMAL"];
				auto uvCoords0AccessorIndex = gltfPrimitive.attributes["TEXCOORD_0"];
				//auto uvCoords1AccessorIndex = gltfPrimitive.attributes["TEXCOORD_1"];
				//auto uvCoords2AccessorIndex = gltfPrimitive.attributes["TEXCOORD_2"];

				auto faceIndicesAccessor = gltfScene.accessors[faceIndicesAccessorIndex];
				auto positionsAccessor = gltfScene.accessors[vertexPositionsAccessorIndex];
				auto normalsAccessor = gltfScene.accessors[vertexNormalsAccessorIndex];
				auto uvCoords0Accessor = gltfScene.accessors[uvCoords0AccessorIndex];
				//auto uvCoords1Accessor = gltfScene.accessors[uvCoords1AccessorIndex];
				//auto uvCoords2Accessor = gltfScene.accessors[uvCoords2AccessorIndex];

				// Load face indices.
				auto faceIndicesCount = faceIndicesAccessor.count;
				auto faceIndicesBufferIndex = gltfScene.bufferViews[faceIndicesAccessor.bufferView].buffer;
				auto faceIndicesBufferOffset = gltfScene.bufferViews[faceIndicesAccessor.bufferView].byteOffset;
				auto faceIndicesBufferStride = faceIndicesAccessor.ByteStride(gltfScene.bufferViews[faceIndicesAccessor.bufferView]);
				auto faceIndicesBufferSizeBytes = faceIndicesCount * faceIndicesBufferStride;
				std::vector<unsigned int> faceIndices(faceIndicesCount);
				if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					for (int i = 0; i < faceIndicesCount; ++i) {
						unsigned short index = 0;
						memcpy(&index, gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset + i * faceIndicesBufferStride, faceIndicesBufferStride);
						faceIndices[i] = index;
					}
				}
				else if (faceIndicesAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					memcpy(faceIndices.data(), gltfScene.buffers[faceIndicesBufferIndex].data.data() + faceIndicesBufferOffset, faceIndicesBufferSizeBytes);
				}

				// Load vertex Positions.
				auto vertexPositionsCount = positionsAccessor.count;
				auto vertexPositionsBufferIndex = gltfScene.bufferViews[positionsAccessor.bufferView].buffer;
				auto vertexPositionsBufferOffset = gltfScene.bufferViews[positionsAccessor.bufferView].byteOffset;
				auto vertexPositionsBufferStride = positionsAccessor.ByteStride(gltfScene.bufferViews[positionsAccessor.bufferView]);
				auto vertexPositionsBufferSizeBytes = vertexPositionsCount * vertexPositionsBufferStride;
				std::vector<glm::vec3> vertexPositions(vertexPositionsCount);
				memcpy(vertexPositions.data(), gltfScene.buffers[vertexPositionsBufferIndex].data.data() + vertexPositionsBufferOffset, vertexPositionsBufferSizeBytes);

				// Load vertex normals.
				auto vertexNormalsCount = normalsAccessor.count;
				auto vertexNormalsBufferIndex = gltfScene.bufferViews[normalsAccessor.bufferView].buffer;
				auto vertexNormalsBufferOffset = gltfScene.bufferViews[normalsAccessor.bufferView].byteOffset;
				auto vertexNormalsBufferStride = normalsAccessor.ByteStride(gltfScene.bufferViews[normalsAccessor.bufferView]);
				auto vertexNormalsBufferSizeBytes = vertexNormalsCount * vertexNormalsBufferStride;
				std::vector<glm::vec3> vertexNormals(vertexNormalsCount);
				memcpy(vertexNormals.data(), gltfScene.buffers[vertexNormalsBufferIndex].data.data() + vertexNormalsBufferOffset, vertexNormalsBufferSizeBytes);

				// Load UV coordinates for UV slot 0.
				auto uvCoords0Count = uvCoords0Accessor.count;
				auto uvCoords0BufferIndex = gltfScene.bufferViews[uvCoords0Accessor.bufferView].buffer;
				auto uvCoords0BufferOffset = gltfScene.bufferViews[uvCoords0Accessor.bufferView].byteOffset;
				auto uvCoords0BufferStride = uvCoords0Accessor.ByteStride(gltfScene.bufferViews[uvCoords0Accessor.bufferView]);
				auto uvCoords0BufferSize = uvCoords0Count * uvCoords0BufferStride;
				std::vector<glm::vec2> uvCoords0(uvCoords0Count);
				memcpy(uvCoords0.data(), gltfScene.buffers[uvCoords0BufferIndex].data.data() + uvCoords0BufferOffset, uvCoords0BufferSize);

				gameObject._pMesh = new Scenes::Mesh{ &_scene };
				auto& mesh = gameObject._pMesh;

				// Gather vertices and face indices.
				std::vector<Scenes::Vertex> vertices;
				vertices.resize(vertexPositions.size());
				for (int i = 0; i < vertexPositions.size(); ++i) {
					Scenes::Vertex v;
					v._position = vertexPositions[i];
					v._normal = vertexNormals[i];
					v._uvCoord = uvCoords0[i];
					vertices[i] = v;
				}

				// Copy vertices to the GPU.
				mesh->CreateVertexBuffer(_physicalDevice, _logicalDevice, _commandPool, _queue, vertices);

				// Copy face indices to the GPU.
				mesh->CreateIndexBuffer(_physicalDevice, _logicalDevice, _commandPool, _queue, faceIndices);

				_scene._gameObjects.push_back(gameObject);
				mesh->_gameObjectIndex = (unsigned int)(_scene._gameObjects.size() - 1);
			}
		}
	}

	void VulkanApplication::LoadEnvironmentMap()
	{
		_scene._environmentMap = Engine::Scenes::CubicalEnvironmentMap::CubicalEnvironmentMap(_physicalDevice, _logicalDevice);
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Waterfall.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Debug.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ModernBuilding.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.png");
		//_environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.hdr");
		_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "garden.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ItalianFlag.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "TestPng.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "EnvMap.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "texture.jpg");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Test1.png");

		_scene._environmentMap.CreateImage(_logicalDevice, _physicalDevice, _commandPool, _queue);
	}

	VkPresentModeKHR VulkanApplication::ChoosePresentMode(const std::vector<VkPresentModeKHR> presentModes)
	{
		for (const auto& presentMode : presentModes) {
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return presentMode;
			}
		}

		// If mailbox is unavailable, fall back to FIFO (guaranteed to be available)
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkSurfaceFormatKHR VulkanApplication::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
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

	VkExtent2D VulkanApplication::ChooseFramebufferSize(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
	{
		auto& settings = Settings::GlobalSettings::Instance();

		if (surfaceCapabilities.currentExtent.width == -1) {
			VkExtent2D swapChainExtent = {};

			swapChainExtent.width = std::min(std::max(settings._windowWidth, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
			swapChainExtent.height = std::min(std::max(settings._windowHeight, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);

			return swapChainExtent;
		}
		else {
			return surfaceCapabilities.currentExtent;
		}
	}

	void VulkanApplication::CreateFramebuffers()
	{
		_swapchain._frameBuffers.resize(_renderPass._colorImages.size());

		for (size_t i = 0; i < _renderPass._colorImages.size(); i++) {

			// We will render to the same depth image for each frame. 
			// We can just keep clearing and reusing the same depth image for every frame.
			VkImageView colorAndDepthImages[2] = { _renderPass._colorImages[i]._view, _renderPass._depthImage._view };
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = _renderPass._handle;
			createInfo.attachmentCount = 2;
			createInfo.pAttachments = &colorAndDepthImages[0];
			createInfo.width = _swapchain._framebufferSize.width;
			createInfo.height = _swapchain._framebufferSize.height;
			createInfo.layers = 1;

			CheckResult(vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &_swapchain._frameBuffers[i]));
		}
	}

	void VulkanApplication::CreateRenderPass()
	{
		auto imageFormat = _swapchain._surfaceFormat.format;

		// Store the images used by the swap chain.
		// Note: these are the images that swap chain image indices refer to.
		// Note: actual number of images may differ from requested number, since it's a lower bound.
		uint32_t actualImageCount = 0;
		CheckResult(vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, nullptr));

		_renderPass._colorImages.resize(actualImageCount);

		std::vector<VkImage> images;
		images.resize(actualImageCount);
		CheckResult(vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, images.data()));

		// Create the color attachments.
		for (uint32_t i = 0; i < actualImageCount; ++i) {
			_renderPass._colorImages[i]._image = images[i];
			auto& createInfo = _renderPass._colorImages[i]._viewCreateInfo;
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			vkCreateImageView(_logicalDevice, &createInfo, nullptr, &_renderPass._colorImages[i]._view);
		}

		// Create the image.
		auto& imageCreateInfo = _renderPass._depthImage._createInfo;
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.extent = { _swapchain._framebufferSize.width, _swapchain._framebufferSize.height, 1 };
		imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		vkCreateImage(_logicalDevice, &imageCreateInfo, nullptr, &_renderPass._depthImage._image);

		// Allocate memory on the GPU for the image.
		VkMemoryRequirements reqs;
		vkGetImageMemoryRequirements(_logicalDevice, _renderPass._depthImage._image, &reqs);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = reqs.size;
		allocInfo.memoryTypeIndex = PhysicalDevice::GetMemoryTypeIndex(_physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkDeviceMemory mem;
		vkAllocateMemory(_logicalDevice, &allocInfo, nullptr, &mem);
		vkBindImageMemory(_logicalDevice, _renderPass._depthImage._image, mem, 0);

		auto& imageViewCreateInfo = _renderPass._depthImage._viewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.components = { {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY}, {VK_COMPONENT_SWIZZLE_IDENTITY} };
		imageViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
		imageViewCreateInfo.image = _renderPass._depthImage._image;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		vkCreateImageView(_logicalDevice, &imageViewCreateInfo, nullptr, &_renderPass._depthImage._view);

		auto& samplerCreateInfo = _renderPass._depthImage._samplerCreateInfo;
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.anisotropyEnable = false;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		vkCreateSampler(_logicalDevice, &samplerCreateInfo, nullptr, &_renderPass._depthImage._sampler);

		// Describes how the render pass is going to use the main color attachment. An attachment is a fancy word for "image used for a render pass".
		VkAttachmentDescription colorAttachmentDescription = {};
		colorAttachmentDescription.format = VK_FORMAT_R8G8B8A8_UNORM;
		colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Note: hardware will automatically transition attachment to the specified layout
		// Note: index refers to attachment descriptions array.
		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Describes how the render pass is going to use the depth attachment.
		VkAttachmentDescription depthAttachmentDescription = {};
		depthAttachmentDescription.flags = 0;
		depthAttachmentDescription.format = VK_FORMAT_D32_SFLOAT;
		depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference = {};
		depthAttachmentReference.attachment = 1;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Note: this is a description of how the attachments of the render pass will be used in this sub pass
		// e.g. if they will be read in shaders and/or drawn to.
		VkSubpassDescription subPassDescription = {};
		subPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subPassDescription.colorAttachmentCount = 1;
		subPassDescription.pColorAttachments = &colorAttachmentReference;
		subPassDescription.pDepthStencilAttachment = &depthAttachmentReference;

		// Now we have to adjust the renderpass synchronization. Previously, it was possible that multiple frames 
		// were rendered simultaneously by the GPU. This is a problem when using depth buffers, because one frame 
		// could overwrite the depth buffer while a previous frame is still rendering to it.
		// We keep the subpass dependency for the color attachment we were already using.
		VkSubpassDependency colorDependency = {};
		colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		colorDependency.dstSubpass = 0;
		colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.srcAccessMask = 0;
		colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before 
		// previous subpasses have finished using it.
		VkSubpassDependency depthDependency = {};
		depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depthDependency.dstSubpass = 0;
		depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.srcAccessMask = 0;
		depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// Create the render pass. We pass in the main image attachment (color) and the depth image attachment, so the GPU knows how to treat
		// the images.
		VkAttachmentDescription attachmentDescriptions[2] = { colorAttachmentDescription, depthAttachmentDescription };
		VkSubpassDependency subpassDependencies[2] = { colorDependency, depthDependency };
		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 2;
		createInfo.pAttachments = &attachmentDescriptions[0];
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subPassDescription;
		createInfo.dependencyCount = 2;
		createInfo.pDependencies = &subpassDependencies[0];

		CheckResult(vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass._handle));
	}

	void VulkanApplication::CreateSwapchain()
	{
		// Get physical device capabilities for the window surface.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = PhysicalDevice::GetSurfaceCapabilities(_physicalDevice, _windowSurface);
		std::vector<VkSurfaceFormatKHR> surfaceFormats = PhysicalDevice::GetSupportedFormatsForSurface(_physicalDevice, _windowSurface);
		std::vector<VkPresentModeKHR> presentModes = PhysicalDevice::GetSupportedPresentModesForSurface(_physicalDevice, _windowSurface);

		// Determine number of images for swapchain.
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
		_swapchain._framebufferSize = ChooseFramebufferSize(surfaceCapabilities);

		// Determine transformation to use (preferring no transform).
		VkSurfaceTransformFlagBitsKHR surfaceTransform;
		if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			surfaceTransform = surfaceCapabilities.currentTransform;
		}

		// Choose presentation mode (preferring MAILBOX ~= triple buffering).
		VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

		// Finally, create the swap chain.
		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = _windowSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = _swapchain._framebufferSize;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = _swapchain._oldSwapchainHandle;

		CheckResult(vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapchain._handle));

		if (_swapchain._oldSwapchainHandle != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(_logicalDevice, _swapchain._oldSwapchainHandle, nullptr);
		}

		_swapchain._oldSwapchainHandle = _swapchain._handle;
	}

	VkShaderModule VulkanApplication::CreateShaderModule(const std::filesystem::path& absolutePath)
	{
		std::ifstream file(absolutePath.c_str(), std::ios::ate | std::ios::binary);
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

			CheckResult(vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule));
		}
		else {
			auto msg = "failed to open file " + absolutePath.string();
			Utils::Exit(1, msg.c_str());
			exit(0);
		}

		return shaderModule;
	}

	void VulkanApplication::CreateGraphicsPipeline()
	{
		using Vertex = Scenes::Vertex;

		VkShaderModule vertexShaderModule = CreateShaderModule(Settings::Paths::VertexShaderPath());
		VkShaderModule fragmentShaderModule = CreateShaderModule(Settings::Paths::FragmentShaderPath());

		// Set up shader stage info.
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

		// Vertex attribute binding - gives the vertex shader more info about a particular vertex buffer, denoted by the binding number. See binding for more info.
		_graphicsPipeline._vertexBindingDescription.binding = 0;
		_graphicsPipeline._vertexBindingDescription.stride = sizeof(Vertex);
		_graphicsPipeline._vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Describe how the shader should read vertex attributes when getting a vertex from the vertex buffer.
		// Object-space positions.
		_graphicsPipeline._vertexAttributeDescriptions.resize(3);
		_graphicsPipeline._vertexAttributeDescriptions[0].location = 0;
		_graphicsPipeline._vertexAttributeDescriptions[0].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[0].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Position);

		// Normals.
		_graphicsPipeline._vertexAttributeDescriptions[1].location = 1;
		_graphicsPipeline._vertexAttributeDescriptions[1].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[1].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::Normal);

		// UV coordinates.
		_graphicsPipeline._vertexAttributeDescriptions[2].location = 2;
		_graphicsPipeline._vertexAttributeDescriptions[2].binding = 0;
		_graphicsPipeline._vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		_graphicsPipeline._vertexAttributeDescriptions[2].offset = (uint32_t)Vertex::OffsetOf(Vertex::AttributeType::UV);

		// Describe vertex input.
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &_graphicsPipeline._vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = (uint32_t)_graphicsPipeline._vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = _graphicsPipeline._vertexAttributeDescriptions.data();

		// Describe input assembly - this allows Vulkan to know how many indices make up a face for the vkCmdDrawIndexed function.
		// The input assembly is the very first stage of the graphics pipeline, where vertices and indices are loaded from VRAM and assembled,
		// to then be passed to the shaders.
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		// Describe viewport and scissor
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = Utils::Converter::Convert<uint32_t, float>(_swapchain._framebufferSize.width);
		viewport.height = Utils::Converter::Convert<uint32_t, float>(_swapchain._framebufferSize.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = _swapchain._framebufferSize.width;
		scissor.extent.height = _swapchain._framebufferSize.height;

		// Note: scissor test is always enabled (although dynamic scissor is possible).
		// Number of viewports must match number of scissors.
		VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
		viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportCreateInfo.viewportCount = 1;
		viewportCreateInfo.pViewports = &viewport;
		viewportCreateInfo.scissorCount = 1;
		viewportCreateInfo.pScissors = &scissor;

		// Describe rasterization - this tells Vulkan what settings to use when at the fragment shader stage of the pipeline, a.k.a. when
		// rendering pixels.
		// Note: depth bias and using polygon modes other than fill require changes to logical device creation (device features).
		VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
		rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationCreateInfo.depthBiasClamp = 0.0f;
		rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
		rasterizationCreateInfo.lineWidth = 1.0f;

		// Configure depth testing.
		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.pNext = nullptr;
		depthStencilCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilCreateInfo.minDepthBounds = 0.0f;
		depthStencilCreateInfo.maxDepthBounds = 1.0f;

		// Describe multisampling
		// Note: using multisampling also requires turning on device features.
		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.minSampleShading = 1.0f;
		multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

		// Describing color blending.
		// Note: all paramaters except blendEnable and colorWriteMask are irrelevant here.
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
		colorBlendAttachmentState.blendEnable = VK_FALSE;
		colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		// Note: all attachments must have the same values unless a device feature is enabled.
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

		// Create the graphics pipeline.
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = 2;
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
		pipelineCreateInfo.layout = _graphicsPipeline._layout;
		pipelineCreateInfo.renderPass = _renderPass._handle;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		CheckResult(vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline._handle));

		// No longer necessary as it has all been put into the _graphicsPipeline object.
		vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
	}

	std::vector<Vulkan::DescriptorSetLayout> VulkanApplication::CreateDescriptorSetLayouts()
	{
		VkDescriptorSetLayout cameraLayout;
		VkDescriptorSetLayout gameObjectLayout;
		VkDescriptorSetLayout meshLayout;
		VkDescriptorSetLayout lightLayout;
		VkDescriptorSetLayout environmentMapLayout;

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &cameraLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &gameObjectLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &lightLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[3];
			bindings[0] = { VkDescriptorSetLayoutBinding {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._albedo._sampler} };
			bindings[1] = { VkDescriptorSetLayoutBinding {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._roughness._sampler} };
			bindings[2] = { VkDescriptorSetLayoutBinding {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._materials[0]._metalness._sampler} };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 3;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &meshLayout);
		}

		{
			VkDescriptorSetLayoutBinding bindings[1] = { VkDescriptorSetLayoutBinding { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_scene._environmentMap._cubeMapImage._sampler } };
			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCreateInfo.bindingCount = 1;
			layoutCreateInfo.pBindings = bindings;
			vkCreateDescriptorSetLayout(_logicalDevice, &layoutCreateInfo, nullptr, &environmentMapLayout);
		}

		return {
			Vulkan::DescriptorSetLayout{ "cameraLayout", 0, cameraLayout },
			Vulkan::DescriptorSetLayout{ "gameObjectLayout", 1, gameObjectLayout },
			Vulkan::DescriptorSetLayout{ "lightLayout", 2, lightLayout },
			Vulkan::DescriptorSetLayout{ "meshLayout", 3, meshLayout },
			Vulkan::DescriptorSetLayout{ "environmentMapLayout", 4, environmentMapLayout }
		};
	}

	void VulkanApplication::CreatePipelineLayout()
	{
		auto layouts = CreateDescriptorSetLayouts();

		auto& shaderResources = _graphicsPipeline._shaderResources;
		auto cameraResources = _mainCamera.CreateDescriptorSets(_physicalDevice, _logicalDevice, _commandPool, _queue, layouts);
		shaderResources.MergeResources(cameraResources);
		_mainCamera.UpdateShaderResources();

		auto sceneResources = _scene.CreateDescriptorSets(_physicalDevice, _logicalDevice, _commandPool, _queue, layouts);
		shaderResources.MergeResources(sceneResources);
		_scene.UpdateShaderResources();

		// Select the layout from each descriptor set to create a layout-only vector.
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
		//std::transform(shaderResources._data.begin(), shaderResources._data.end(), std::back_inserter(layouts), [](const std::pair<PipelineLayout, VkDescriptorSet>& res) { return res.first._layout; });

		std::transform(shaderResources._data.begin(), shaderResources._data.end(), std::back_inserter(descriptorSetLayouts),
			[](const std::pair<DescriptorSetLayout, std::vector<VkDescriptorSet>>& pair) {
				return pair.first._layout;
			});

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		CheckResult(vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &_graphicsPipeline._layout));
	}

	void VulkanApplication::AllocateDrawCommandBuffers()
	{
		// Allocate graphics command buffers
		_drawCommandBuffers.resize(_renderPass._colorImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = _commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)_renderPass._colorImages.size();

		CheckResult(vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _drawCommandBuffers.data()));
	}

	void VulkanApplication::RecordDrawCommands()
	{
		// Prepare data for recording command buffers.
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkImageSubresourceRange subResourceRange = {};
		subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResourceRange.baseMipLevel = 0;
		subResourceRange.levelCount = 1;
		subResourceRange.baseArrayLayer = 0;
		subResourceRange.layerCount = 1;

		// Record command buffer for each swapchain image.
		for (size_t i = 0; i < _renderPass._colorImages.size(); i++) {
			vkBeginCommandBuffer(_drawCommandBuffers[i], &beginInfo);

			// If present queue family and graphics queue family are different, then a barrier is necessary
			// The barrier is also needed initially to transition the image to the present layout
			VkImageMemoryBarrier presentToDrawBarrier = {};
			presentToDrawBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presentToDrawBarrier.srcAccessMask = 0;
			presentToDrawBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presentToDrawBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			presentToDrawBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			presentToDrawBarrier.image = _renderPass._colorImages[i]._image;
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(_drawCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

			VkClearValue clearColor = {
			{ 0.1f, 0.1f, 0.1f, 1.0f } // R, G, B, A.
			};

			VkClearValue depthClear;
			depthClear.depthStencil.depth = 1.0f;
			VkClearValue clearValues[] = { clearColor, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = _renderPass._handle;
			renderPassBeginInfo.framebuffer = _swapchain._frameBuffers[i];
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent = _swapchain._framebufferSize;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = &clearValues[0];

			vkCmdBeginRenderPass(_drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._handle);

			auto& shaderResources = _graphicsPipeline._shaderResources;
			VkDescriptorSet sets[4] = { shaderResources[0][0], shaderResources[1][0], shaderResources[2][0], shaderResources[4][0] };
			vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 0, 3, sets, 0, nullptr);
			vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 4, 1, &shaderResources[4][0], 0, nullptr);

			for (auto& gameObject : _scene._gameObjects) {
				gameObject._pMesh->Draw(_graphicsPipeline._layout, _drawCommandBuffers[i]);
			}

			vkCmdEndRenderPass(_drawCommandBuffers[i]);

			CheckResult(vkEndCommandBuffer(_drawCommandBuffers[i]));
		}
	}

	void VulkanApplication::Draw()
	{
		if (!windowMinimized) {

			// Acquire image.
			uint32_t imageIndex;
			VkResult res = vkAcquireNextImageKHR(_logicalDevice, _swapchain._handle, UINT64_MAX, _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			// Unless surface is out of date right now, defer swap chain recreation until end of this frame.
			if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || windowResized) {
				WindowSizeChanged();
				return;
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to acquire image" << std::endl;
				exit(1);
			}

			// Wait for image to be available and draw.
			// This is the stage where the queue should wait on the semaphore.
			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &_imageAvailableSemaphore;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &_renderingFinishedSemaphore;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_drawCommandBuffers[imageIndex];

			if (auto res = vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
				std::cerr << "failed to submit draw command buffer" << std::endl;
				exit(1);
			}

			// Present drawn image.
			// Note: semaphore here is not strictly necessary, because commands are processed in submission order within a single queue.
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &_renderingFinishedSemaphore;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &_swapchain._handle;
			presentInfo.pImageIndices = &imageIndex;

			res = vkQueuePresentKHR(_queue, &presentInfo);

			if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
				WindowSizeChanged();
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to submit present command buffer" << std::endl;
				exit(1);
			}
		}
	}
}
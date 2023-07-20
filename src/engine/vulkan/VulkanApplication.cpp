#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

// STL.
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

// Math.
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Scene loading.
#include <tinygltf/tiny_gltf.h>

// Project local classes.
#include "utils/Json.h"
#include "structural/IUpdatable.hpp"
#include "structural/Singleton.hpp"
#include "settings/GlobalSettings.hpp"
#include "settings/Paths.hpp"
#include "engine/Time.hpp"
#include "engine/input/Input.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/PhysicalDevice.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "utils/Utils.hpp"
#include "engine/math/Transform.hpp"
#include "engine/vulkan/Queue.hpp"
#include "engine/vulkan/Image.hpp"
#include "engine/scenes/Material.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include "engine/scenes/Vertex.hpp"
#include "engine/structural/IPipelineable.hpp"
#include "engine/structural/Drawable.hpp"
#include "engine/scenes/PointLight.hpp"
#include "engine/scenes/CubicalEnvironmentMap.hpp"
#include "engine/scenes/Scene.hpp"
#include "engine/scenes/GameObject.hpp"
#include "engine/scenes/Camera.hpp"
#include "engine/scenes/Mesh.hpp"
#include "engine/vulkan/VulkanApplication.hpp"

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
			std::cerr << "\nERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
			std::cerr << "\nWARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
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
		CreateInstance();
		CreateDebugCallback();
		CreateWindowSurface();
		_physicalDevice = PhysicalDevice(_instance);
		FindQueueFamilyIndex();
		CreateLogicalDeviceAndQueue();
		CreateSemaphores();
		CreateCommandPool();
		LoadScene();
		LoadEnvironmentMap();
		CreateSwapchain();
		CreateRenderPass();
		CreateFramebuffers();
		CreatePipelineLayout();
		CreateGraphicsPipeline();
		AllocateDrawCommandBuffers();
		RecordDrawCommands();
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
		_renderPass._depthImage.Destroy();
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
	} // Cleanup

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

	void VulkanApplication::CreateInstance()
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
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) {
			std::cerr << "no extensions supported!" << std::endl;
			exit(1);
		}

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		std::cout << "supported instance extensions:" << std::endl;

		for (const auto& extension : availableExtensions) {
			std::cout << "\t" << extension.extensionName << std::endl;
		}

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

		if (vkCreateInstance(&createInfo, nullptr, &_instance)) {
			std::cerr << "failed to create instance" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan instance" << std::endl;
		}
	}

	void VulkanApplication::CreateWindowSurface()
	{
		if (glfwCreateWindowSurface(_instance, _pWindow, NULL, &_windowSurface) != VK_SUCCESS) {
			std::cerr << "failed to create window surface!" << std::endl;
			exit(1);
		}

		std::cout << "created window surface" << std::endl;
	}

	void VulkanApplication::FindQueueFamilyIndex()
	{
		auto queueFamilyProperties = _physicalDevice.GetAllQueueFamilyProperties();

		for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
			if (_physicalDevice.SupportsSurface(i, _windowSurface) &&
				queueFamilyProperties[i].queueCount > 0 &&
				queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

				_queue._familyIndex = i;
			}
		}
	}

	void VulkanApplication::CreateLogicalDeviceAndQueue()
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = _queue._familyIndex;
		queueCreateInfo.queueCount = 1;
		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;

		// Necessary for shader (for some reason)
		VkPhysicalDeviceFeatures enabledFeatures = {};
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

		if (vkCreateDevice(_physicalDevice._handle, &deviceCreateInfo, nullptr, &_logicalDevice) != VK_SUCCESS) {
			std::cerr << "failed to create logical device" << std::endl;
			exit(1);
		}

		std::cout << "created logical device" << std::endl;

		vkGetDeviceQueue(_logicalDevice, _queue._familyIndex, 0, &_queue._handle);
	}

	void VulkanApplication::CreateDebugCallback()
	{
		if (_settings._enableValidationLayers) {
			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)DebugCallback;
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

	void VulkanApplication::CreateSemaphores()
	{
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

	void VulkanApplication::CreateCommandPool()
	{
		// Create graphics command pool
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = _queue._familyIndex;

		if (vkCreateCommandPool(_logicalDevice, &poolCreateInfo, nullptr, &_commandPool) != VK_SUCCESS) {
			std::cerr << "failed to create command queue for graphics queue family" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created command pool for graphics queue family" << std::endl;
		}
	}

	void VulkanApplication::LoadScene()
	{
		_scene._pointLights.push_back(Scenes::PointLight("DefaultLight"));

		tinygltf::Model gltfScene;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		//auto scenePath = Settings::Paths::ModelsPath() /= "MaterialSphere.glb";
		auto scenePath = Settings::Paths::ModelsPath() /= "mp5k.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "Cube.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "stanford_dragon_pbr.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "SampleMap.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "monster.glb";
		//auto scenePath = Settings::Paths::ModelsPath() /= "free_1972_datsun_4k_textures.glb";
		bool ret = loader.LoadBinaryFromFile(&gltfScene, &err, &warn, scenePath.string());
		std::cout << warn << std::endl;
		std::cout << err << std::endl;

		std::map<unsigned int, unsigned int> loadedMaterialCache; // Gltf material index, index of the materials in scene._materials.

		for (int i = 0; i < gltfScene.materials.size(); ++i) {
			Scenes::Material m;
			auto& baseColorTextureIndex = gltfScene.materials[i].pbrMetallicRoughness.baseColorTexture.index;
			m._name = gltfScene.materials[i].name;

			if (baseColorTextureIndex >= 0) {
				auto& baseColorImageIndex = gltfScene.textures[baseColorTextureIndex].source;
				auto& baseColorImageData = gltfScene.images[baseColorImageIndex].image;
				auto size = VkExtent2D{ (uint32_t)gltfScene.images[baseColorImageIndex].width, (uint32_t)gltfScene.images[baseColorImageIndex].height };

				m._baseColor = Image(_logicalDevice,
					_physicalDevice,
					VK_FORMAT_R8G8B8A8_SRGB,
					VkExtent3D{ size.width, size.height, 1 },
					baseColorImageData.data(),
					(VkImageUsageFlagBits)(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				m._baseColor.SendToGPU(_commandPool, _queue);
				_scene._materials.push_back(m);
				loadedMaterialCache.emplace(i, (unsigned int)_scene._materials.size() - 1);
			}
		}

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

				if (scale.size() == 3) {
					gameObject._transform.SetScale(glm::vec3{ scale[0], scale[1], scale[2] });
				}

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

				// Load material.
				if (loadedMaterialCache.count(gltfPrimitive.material) > 0) {
					mesh->_materialIndex = loadedMaterialCache[gltfPrimitive.material];
				}

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

		std::cout << "scene " << scenePath.string() << " loaded" << std::endl;
	}

	void VulkanApplication::LoadEnvironmentMap()
	{
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Waterfall.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Debug.png");
		_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ModernBuilding.hdr");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Workshop.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "ItalianFlag.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "TestPng.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "EnvMap.png");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "texture.jpg");
		//_scene._environmentMap.LoadFromSphericalHDRI(Settings::Paths::TexturesPath() /= "Test1.png");
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
			VkImageView colorAndDepthImages[2] = { _renderPass._colorImages[i]._viewHandle, _renderPass._depthImage._viewHandle };
			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = _renderPass._handle;
			createInfo.attachmentCount = 2;
			createInfo.pAttachments = &colorAndDepthImages[0];
			createInfo.width = _swapchain._framebufferSize.width;
			createInfo.height = _swapchain._framebufferSize.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(_logicalDevice, &createInfo, nullptr, &_swapchain._frameBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to create framebuffer for swap chain image view #" << i << std::endl;
				exit(1);
			}
		}

		std::cout << "created framebuffers for swap chain image views" << std::endl;
	}

	void VulkanApplication::CreateRenderPass()
	{
		auto imageFormat = _swapchain._surfaceFormat.format;

		// Store the images used by the swap chain.
		// Note: these are the images that swap chain image indices refer to.
		// Note: actual number of images may differ from requested number, since it's a lower bound.
		uint32_t actualImageCount = 0;
		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, nullptr) != VK_SUCCESS || actualImageCount == 0) {
			std::cerr << "failed to acquire number of swap chain images" << std::endl;
			exit(1);
		}

		_renderPass._colorImages.resize(actualImageCount);

		std::vector<VkImage> images;
		images.resize(actualImageCount);
		if (vkGetSwapchainImagesKHR(_logicalDevice, _swapchain._handle, &actualImageCount, images.data()) != VK_SUCCESS) {
			std::cerr << "failed to acquire swapchain images" << std::endl;
			exit(1);
		}

		std::cout << "acquired swap chain images" << std::endl;

		// Create the color attachments.
		for (uint32_t i = 0; i < actualImageCount; ++i) {
			_renderPass._colorImages[i] = Image(_logicalDevice, images[i], VK_FORMAT_R8G8B8A8_UNORM);
		}

		// Create the depth attachment.
		auto& settings = Settings::GlobalSettings::Instance();
		_renderPass._depthImage = Image(_logicalDevice,
			_physicalDevice,
			VK_FORMAT_D32_SFLOAT,
			VkExtent3D{ _swapchain._framebufferSize.width, _swapchain._framebufferSize.height, 1 },
			nullptr,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		std::cout << "created image views for swap chain images" << std::endl;

		// Describes how the render pass is going to use the main color attachment. An attachment is a fancy word for image.
		VkAttachmentDescription colorAttachmentDescription = {};
		colorAttachmentDescription.format = _renderPass._colorImages.size() > 0 ? _renderPass._colorImages[0]._format : VK_FORMAT_UNDEFINED;
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
		depthAttachmentDescription.format = _renderPass._depthImage._format;
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
		// previous renderpasses have finished using it.
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

		if (vkCreateRenderPass(_logicalDevice, &createInfo, nullptr, &_renderPass._handle) != VK_SUCCESS) {
			std::cerr << "failed to create render pass" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created render pass" << std::endl;
		}
	}

	void VulkanApplication::CreateSwapchain()
	{
		// Get physical device capabilities for the window surface.
		VkSurfaceCapabilitiesKHR surfaceCapabilities = _physicalDevice.GetSurfaceCapabilities(_windowSurface);
		std::vector<VkSurfaceFormatKHR> surfaceFormats = _physicalDevice.GetSupportedFormatsForSurface(_windowSurface);
		std::vector<VkPresentModeKHR> presentModes = _physicalDevice.GetSupportedPresentModesForSurface(_windowSurface);

		// Determine number of images for swapchain.
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount != 0 && imageCount > surfaceCapabilities.maxImageCount) {
			imageCount = surfaceCapabilities.maxImageCount;
		}

		std::cout << "using " << imageCount << " images for swap chain" << std::endl;

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

		if (vkCreateSwapchainKHR(_logicalDevice, &createInfo, nullptr, &_swapchain._handle) != VK_SUCCESS) {
			std::cerr << "failed to create swapchain" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created swapchain" << std::endl;
		}

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

			if (vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				std::wcerr << "failed to create shader module for " << absolutePath.c_str() << std::endl;
				exit(1);
			}

			std::wcout << "created shader module for " << absolutePath.c_str() << std::endl;
		}
		else {
			std::wcout << "failed to open file " << absolutePath.c_str() << std::endl;
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

		if (vkCreateGraphicsPipelines(_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &_graphicsPipeline._handle) != VK_SUCCESS) {
			std::cerr << "failed to create graphics pipeline" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created graphics pipeline" << std::endl;
		}

		// No longer necessary as it has all been put into the _graphicsPipeline object.
		vkDestroyShaderModule(_logicalDevice, vertexShaderModule, nullptr);
		vkDestroyShaderModule(_logicalDevice, fragmentShaderModule, nullptr);
	}

	void VulkanApplication::CreatePipelineLayout()
	{
		_mainCamera.CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
		_mainCamera.UpdateShaderResources();

		VkDescriptorSetLayout* gameObjectLayout = nullptr;
		VkDescriptorSetLayout* meshLayout = nullptr;
		VkDescriptorSetLayout* lightLayout = nullptr;
		VkDescriptorSetLayout* sceneLayout = nullptr;

		for (auto& gameObject : _scene._gameObjects) {
			if (gameObject._pMesh != nullptr) {
				gameObject.CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
				gameObject._pMesh->CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
				gameObject.UpdateShaderResources();
				gameObject._pMesh->UpdateShaderResources();

				if (gameObjectLayout == nullptr) {
					gameObjectLayout = &gameObject._sets[0]._layout;
				}

				if (meshLayout == nullptr) {
					meshLayout = &gameObject._pMesh->_sets[0]._layout;
				}
			}
		}

		for (auto& light : _scene._pointLights) {
			light.CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
			light.UpdateShaderResources();

			if (lightLayout == nullptr) {
				lightLayout = &light._sets[0]._layout;
			}
		}

		_scene.CreateShaderResources(_physicalDevice, _logicalDevice, _commandPool, _queue);
		_scene.UpdateShaderResources();
		sceneLayout = &_scene._sets[0]._layout;

		std::vector<VkDescriptorSetLayout> layouts = { _mainCamera._sets[0]._layout, *gameObjectLayout, *lightLayout, *meshLayout, *sceneLayout };
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
		pipelineLayoutCreateInfo.pSetLayouts = layouts.data();

		if (vkCreatePipelineLayout(_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &_graphicsPipeline._layout) != VK_SUCCESS) {
			std::cerr << "failed to create pipeline layout" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created pipeline layout" << std::endl;
		}
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

		if (vkAllocateCommandBuffers(_logicalDevice, &allocInfo, _drawCommandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "failed to allocate draw command buffers" << std::endl;
			exit(1);
		}
		else {
			std::cout << "allocated draw command buffers" << std::endl;
		}
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
			presentToDrawBarrier.srcQueueFamilyIndex = _queue._familyIndex;
			presentToDrawBarrier.dstQueueFamilyIndex = _queue._familyIndex;
			presentToDrawBarrier.image = _renderPass._colorImages[i]._imageHandle;
			presentToDrawBarrier.subresourceRange = subResourceRange;

			vkCmdPipelineBarrier(_drawCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToDrawBarrier);

#pragma region RenderPassCommandRecording
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

			for (auto& gameObject : _scene._gameObjects) {
				VkDescriptorSet sets[5] = { _mainCamera._sets[0]._handle, gameObject._sets[0]._handle, _scene._pointLights[0]._sets[0]._handle, gameObject._pMesh->_sets[0]._handle, _scene._sets[0]._handle };
				vkCmdBindDescriptorSets(_drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline._layout, 0, 5, sets, 0, nullptr);
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(_drawCommandBuffers[i], 0, 1, &gameObject._pMesh->_vertices._vertexBuffer._handle, &offset);
				vkCmdBindIndexBuffer(_drawCommandBuffers[i], gameObject._pMesh->_faceIndices._indexBuffer._handle, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(_drawCommandBuffers[i], (uint32_t)gameObject._pMesh->_faceIndices._indexData.size(), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(_drawCommandBuffers[i]);

			if (vkEndCommandBuffer(_drawCommandBuffers[i]) != VK_SUCCESS) {
				std::cerr << "failed to record command buffer" << std::endl;
				exit(1);
#pragma endregion
			}

			std::cout << "recorded draw commands in draw command buffer" << std::endl;
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

			if (auto res = vkQueueSubmit(_queue._handle, 1, &submitInfo, VK_NULL_HANDLE); res != VK_SUCCESS) {
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

			res = vkQueuePresentKHR(_queue._handle, &presentInfo);

			if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || windowResized) {
				WindowSizeChanged();
			}
			else if (res != VK_SUCCESS) {
				std::cerr << "failed to submit present command buffer" << std::endl;
				exit(1);
			}
		}
	}

	VkFormat VulkanApplication::ChooseImageFormat(const std::filesystem::path& absolutePathToImage)
	{
		auto extension = absolutePathToImage.extension();

		if (extension == L".jpg") {
			return VK_FORMAT_R8G8B8_SRGB;
		}
		else if (extension == L".png") {
			return VK_FORMAT_R8G8B8A8_SRGB;
		}

		return VK_FORMAT_UNDEFINED;
	}
}
#include <vector>
#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "../src/engine/core/app_config.h"
#include "instance.h"


namespace Engine::Core::VulkanEntities {
	void Instance::CreateDebugCallback(AppConfig& app_config) {
		if (app_config.debugInfo_.enable_debugging) {
			VkDebugReportCallbackCreateInfoEXT create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			create_info.pfnCallback = (PFN_vkDebugReportCallbackEXT)app_config.DebugCallback;
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

	void Instance::CreateInstance(AppConfig& app_config) {

		//Add meta information to the Vulkan application
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_config.appInfo_.name;
		app_info.applicationVersion = VK_MAKE_VERSION(app_config.appInfo_.version_major, app_config.appInfo_.version_minor, app_config.appInfo_.version_patch);
		app_info.pEngineName = "Celeritas Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		//Get instance extensions required by GLFW to draw to the window. Extensions are just features (pieces of code)
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
		if (app_config.debugInfo_.enable_debugging) {
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
		if (app_config.debugInfo_.enable_debugging) {
			create_info.enabledLayerCount = 1;
			create_info.ppEnabledLayerNames = &app_config.debugInfo_.debug_layer;
		}
		if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
			std::cerr << "failed to create instance!" << std::endl;
			exit(1);
		}
		else {
			std::cout << "created vulkan instance" << std::endl;
		}
	}

	VkInstance Instance::GetVulkanInstance() {
		return instance_;
	}
}
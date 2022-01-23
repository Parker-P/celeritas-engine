#pragma once
#include <string>
#include <vulkan/vulkan.h>

namespace Engine::Core {
	
	//Configuration variables for vulkan applications
	class AppConfig {
	public:

		//Metadata of your application
		static struct {
			const char* name; //Name of the application
			short version_major; //The x in x.0.0
			short version_minor; //The x in 0.x.0
			short version_patch; //The x in 0.0.x
		} appInfo_;

		static struct {
			unsigned int width_; //The width of the window
			unsigned int height_; //The height of the window
		} settings_;

		//Contains information for application debugging
		static struct {
			bool enable_debugging = false; //Enable debugging?
			const char* debug_layer; //Debug layer constant
		} debugInfo_;

		//Absolute path to the shaders folder
		static std::string kShaderPath_ = "C:\\Users\\Paolo Parker\\source\\repos\\Celeritas Engine\\src\\engine\\shaders\\"; //Path to the folder where the shader files are
	
		AppConfig& operator=(AppConfig& other) {
			this->appInfo_ = other.appInfo_;
			this->settings_ = other.settings_;
			this->debugInfo_ = other.debugInfo_;
			this->kShaderPath_ = other.kShaderPath_;
		}

		//Callback used by Vulkan when a validation layer reports an error
		static VkBool32 DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char* pLayerPrefix, const char* pMsg, void* pUserData) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				std::cerr << "ERROR: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				std::cerr << "WARNING: [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;
			}
			return VK_FALSE;
		}
	};
}

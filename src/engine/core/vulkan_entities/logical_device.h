#pragma once

namespace Engine::Core::VulkanEntities {
	//A logical device is an instance of a physical device
	class LogicalDevice {
		VkDevice logical_device_;
	public:
		void CreateLogicalDevice(PhysicalDevice& physical_device, AppConfig& app_config);
		VkDevice GetLogicalDevice();
	};
}

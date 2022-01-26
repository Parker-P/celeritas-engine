#pragma once

namespace Engine::Core::VulkanEntities {
	class LogicalDevice {
		VkDevice logical_device_;
	public:
		void CreateLogicalDevice(PhysicalDevice& physical_device, AppConfig& app_config);
		VkDevice GetLogicalDevice();
	};
}

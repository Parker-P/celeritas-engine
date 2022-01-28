#pragma once

namespace Engine::Core::VulkanEntities {
	class GraphicsPipeline {
		VkPipeline graphics_pipeline_;
		VkShaderModule CreateShaderModule(LogicalDevice& logical_device, const std::string& file_name);
	public:

		void CreateGraphicsPipeline(LogicalDevice& logical_device, SwapChain& swap_chain, AppConfig& app_config);
	};
}
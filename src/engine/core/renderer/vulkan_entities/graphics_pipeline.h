#pragma once

namespace Engine::Core::Renderer::VulkanEntities {
	class GraphicsPipeline {
		VkPipeline graphics_pipeline_; //Holds info about the entire graphics pipeline
	public:
		void CreateGraphicsPipeline(LogicalDevice& logical_device, SwapChain& swap_chain, AppConfig& app_config);
	};
}
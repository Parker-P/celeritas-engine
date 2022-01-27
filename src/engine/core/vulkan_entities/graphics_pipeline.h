#pragma once

namespace Engine::Core::VulkanEntities {
	class GraphicsPipeline {
		VkShaderModule CreateShaderModule(const std::string& filename);
	public:

		void CreateGraphicsPipeline();
	};
}
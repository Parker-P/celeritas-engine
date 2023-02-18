#pragma once
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/scenes/Mesh.hpp"
#include "engine/vulkan/Buffer.hpp"
#include "engine/vulkan/ShaderResources.hpp"
#include <engine/vulkan/PhysicalDevice.hpp>

namespace Engine::Structural
{
	/**
	 * @brief Used by deriving classes to be able to bind shader resources (descriptor sets and push constants) to a pipeline.
	 * Information can be sent from application (run by the CPU) accessible memory to shader (run by the GPU) accessible memory, in order for it to be used in 
	 * certain ways in the shader programs. This data structure encapsulates all Vulkan calls needed to enable that. There are 2 ways that data can be sento to 
	 * the shaders: using push constants, or using descriptors.
	 * The most common and flexible way is using descriptors.
	 */
	class Pipelineable
	{

	public:

		/**
		 * @brief Array of pairs; each pair represents a block of data meant to be sent to shaders.
		 */
		std::vector<Vulkan::Buffer> _data;

		std::vector<Vulkan::Descriptor> _descriptors;

		std::vector<Vulkan::DescriptorSet> _sets;

		Vulkan::DescriptorPool _pool;

		/**
		 * @brief Function meant for implementing classes to assemble and return their pipeline layouts; this means descriptor sets and push constants.
		 * For more information, see ShaderResources.cpp and _graphicsPipeline in VulkanApplication.hpp.
		 * @return A pair of GPU-allocated descriptor set layouts and push constants.
		 */
		virtual std::pair<std::vector<VkDescriptorSetLayout>, std::vector<VkPushConstantRange>> CreateShaderResources() = 0;
	};
}

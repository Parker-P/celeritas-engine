#pragma once

namespace Engine::Structural
{
	/**
	 * @brief Used by deriving classes to be able to bind shader resources (descriptor sets and push constants) to GPU-visible memory, to then be used by a pipeline.
	 * Information can be sent from application (run by the CPU) accessible memory to shader (run by the GPU) accessible memory, in order for it to be used in
	 * certain ways in the shader programs. This data structure encapsulates all Vulkan calls needed to enable that. There are 2 ways that data can be sent to
	 * the shaders: using push constants, or using descriptors.
	 * The most common and flexible way is using descriptors, but push constant are fastest.
	 */
	class IPipelineable
	{

	public:

		/**
		 * @brief Buffers to be used in descriptors for shader resources.
		 */
		std::vector<Vulkan::Buffer> _buffers;

		/**
		 * @brief Images to be used in descriptors for shader resources.
		 */
		std::vector<Vulkan::Image> _images;

		/**
		 * @brief See ShaderResources.
		 */
		Vulkan::ShaderResources _shaderResources;

		/**
		 * @brief Function that is meant for deriving classes to create shader resources and send them to GPU-visible memory (could be either RAM or VRAM).
		 * Shader resources can either be push constants or descriptors. See ShaderResources.cpp.
		 * @param physicalDevice Intended to be used to gather GPU information when allocating buffers or images.
		 * @param logicalDevice Intended to be used for binding created buffers, images, descriptors, descriptor sets etc. to the GPU.
		 */
		virtual Vulkan::ShaderResources CreateDescriptorSets(VkPhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, VkQueue& queue, std::vector<Vulkan::DescriptorSetLayout>& layouts) = 0;

		/**
		 * @brief Function that is meant for deriving classes to update the shader resources that have been created with CreateDescriptorSets.
		 */
		virtual void UpdateShaderResources() = 0;
	};
}

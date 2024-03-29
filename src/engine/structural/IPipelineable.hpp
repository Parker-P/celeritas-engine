#pragma once

#include <array>
#include <iostream>

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
		 * @brief Buffers to be used in descriptors that go in _descriptors.
		 */
		std::vector<Vulkan::Buffer> _buffers;

		/**
		 * @brief Images to be used in descriptors that go in _descriptors.
		 */
		std::vector<Vulkan::Image> _images;

		/**
		 * @brief Descriptors to be used in _sets.
		 */
		std::vector<Vulkan::Descriptor> _descriptors;

		/**
		 * @brief Descriptor sets to be allocated by _pool.
		 */
		std::vector<Vulkan::DescriptorSet> _sets;

		/**
		 * @brief GPU-memory allocator for _sets.
		 */
		Engine::Vulkan::DescriptorPool _pool;

		/**
		 * @brief Function that is meant for deriving classes to create shader resources and send them to GPU-visible memory (could be either RAM or VRAM).
		 * Shader resources can either be push constants or descriptors. See ShaderResources.cpp.
		 * @param physicalDevice Intended to be used to gather GPU information when allocating buffers or images.
		 * @param logicalDevice Intended to be used for binding created buffers, images, descriptors, descriptor sets etc. to the GPU.
		 */
		virtual void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice, VkCommandPool& commandPool, Vulkan::Queue& graphicsQueue) = 0;

		/**
		 * @brief Function that is meant for deriving classes to update the shader resources that have been created with CreateShaderResources.
		 */
		virtual void UpdateShaderResources() = 0;
	};
}

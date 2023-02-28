#pragma once

namespace Engine::Vulkan
{
	class Descriptor;
	class DescriptorSet;
	class DescriptorPool;
}

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
		 * @brief
		 */
		std::vector<Vulkan::Buffer> _data;

		/**
		 * @brief .
		 */
		std::vector<Vulkan::Descriptor> _descriptors;

		/**
		 * @brief .
		 */
		std::vector<Vulkan::DescriptorSet> _sets;

		/**
		 * @brief .
		 */
		Vulkan::DescriptorPool _pool;
	};
}

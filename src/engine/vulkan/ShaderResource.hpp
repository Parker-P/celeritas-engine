#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief Represents a descriptor set, which is a group of descriptors.
	 * Each descriptor in the set is an entry in the shader's input variables, and can be either a buffer or an image.
	 */
	class DescriptorSet
	{
	public:
		/**
		 * @brief ID used to make the pipeline layout and the order the descriptor sets are bound to the pipeline (via vkBindDescriptorSets) match.
		 */
		int _id;

		/**
		 * @brief Handle.
		 */
		VkDescriptorSet _set;

		/**
		 * @brief Layout handle.
		 */
		VkDescriptorSetLayout _layout;
	};
}
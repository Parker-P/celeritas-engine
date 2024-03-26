#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief Describes the structure that a single descriptor set to provide context on how the shader should treat the descriptor set.
	 * To make an analogy: if descriptor sets were cars, the blueprints used to fabricate them would be the pipeline layout, and the
	 * people inside the car would be the descriptors (the data the sets contain).
	 * A descriptor set is a group of descriptors. Each descriptor in the set is an entry in the shader's input variables, and can be either
	 * a buffer or an image.
	 */
	class PipelineLayout
	{
	public:
		/**
		 * @brief ID used to make the pipeline layout and the order the descriptor sets are bound to the pipeline (via vkBindDescriptorSets) match.
		 * In shaders, this corresponds to the "set" decorator when defining an input variable, like in this line of GLSL:
		 * layout(set = 3, binding = 0) uniform sampler2D albedoMap;
		 */
		int _id;

		/**
		 * @brief Info with which the pipeline layout was created.
		 */
		VkDescriptorSetLayoutCreateInfo _createInfo;

		/**
		 * @brief Layout handle.
		 */
		VkDescriptorSetLayout _layout;

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator<(const PipelineLayout& other) const
		{
			return _id < other._id;
		}

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator==(const PipelineLayout& other) const
		{
			return _id == other._id;
		}
	};

	/**
	 * @brief Represents a description of how data from CPU-side memory is bound to input variables in the shaders.
	 */
	class ShaderResources
	{
	public:
		std::map<PipelineLayout, std::vector<VkDescriptorSet>> _data;

		/**
		 * @brief Operator overload to index into _data.
		 */
		std::vector<VkDescriptorSet>& operator[](const int& index)
		{
			auto it = std::find_if(_data.begin(), _data.end(), [index](const auto& pair) { return pair.first._id == index; });

			if (it == _data.end()) {
				throw std::out_of_range("No resources found for the given index");
			}

			return it->second;
		}
	};
}
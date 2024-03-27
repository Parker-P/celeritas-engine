#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief Describes the structure of a single descriptor set to provide context on how the shader should treat the descriptor set.
	 * To make an analogy: if descriptor sets were cars, the blueprint used to fabricate them would be the descriptor set layout, and the
	 * people inside the car would be the descriptors (the data the sets contain).
	 * A descriptor set is a group of descriptors. Each descriptor in the set is an entry in the shader's input variables, and can be either
	 * a buffer or an image.
	 */
	class DescriptorSetLayout
	{
	public:
		/**
		 * @brief ID used to make the pipeline layout and the order the descriptor sets are bound to the pipeline (via vkBindDescriptorSets) match.
		 * In shaders, this corresponds to the "set" decorator when defining an input variable, like in this line of GLSL:
		 * layout(set = 3, binding = 0) uniform sampler2D albedoMap;
		 */
		int _id;

		/**
		 * @brief Layout handle.
		 */
		VkDescriptorSetLayout _layout;

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator<(const DescriptorSetLayout& other) const
		{
			return _id < other._id;
		}

		/**
		 * @brief Used for ordering pipeline layouts in map structures.
		 */
		bool operator==(const DescriptorSetLayout& other) const
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
		
		std::map<DescriptorSetLayout, std::vector<VkDescriptorSet>> _data;

		/**
		 * @brief Merges this shader resources instance with another.
		 */
		void MergeResources(ShaderResources& other) {
			for (const auto& pair : other._data) {
				auto it = _data.find(pair.first);
				if (it != _data.end()) {
					// Here, you can decide how to merge the values.
					// For simplicity, I'm replacing the value in map1 with the value from map2.
					it->second = pair.second;
				}
				else {
					// If the key doesn't exist in map1, simply insert the key-value pair
					_data.insert(pair);
				}
			}
		}

		/**
		 * @brief Operator overload to index into _data.
		 */
		std::vector<VkDescriptorSet>& operator[](const int& index)
		{
			auto it = std::find_if(_data.begin(), _data.end(), [index](const auto& pair) { return pair.first._id == index; });

			if (it == _data.end()) {
				exit(1);
			}

			return it->second;
		}
	};
}
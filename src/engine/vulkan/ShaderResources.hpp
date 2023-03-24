#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief A descriptor is a block of data, similar to a buffer, with the difference being that a descriptor is bound to metadata
	 * that Vulkan uses to enable their use in the shaders of a pipeline. This allows us to exchange data between a program run by the CPU's cores with a shader run
	 * by the GPU's cores.
	 * A descriptor is accessed by a shader by using an index and a binding number, similar to vertex attributes, which are accessed
	 * by the vertex shader using a binding and a location number.
	 * For example, in a shader, a descriptor is declared as follows:
	 * 
	 * #version 450
	 *
	 * // Descriptor declaration. The shader will access the descriptor with binding number 1 at descriptor set with index 2.
	 * layout(set = 2, binding = 1) uniform UniformBuffer
	 * {
	 *     ... data in the uniform buffer ...
	 * } uniformBuffer;
	 *
	 * void main()
	 * {
	 *		... shader code ...
	 * }
	 * 
	 * See the _bindingNumber member, and the _indexNumber member in the DescriptorSet class.
	 */
	class Descriptor
	{

	public:

		/**
		 * @brief Wrapper that describes a buffer.
		 */
		VkDescriptorBufferInfo _bufferInfo;

		/**
		 * @brief Wrapper that describes an image.
		 */
		VkDescriptorImageInfo _imageInfo;

		/**
		 * @brief Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a
		 * structure that contains a pointer to an image and some metadata that tells the GPU how to read it.
		 */
		VkDescriptorType _type;

		/**
		 * @brief Binding number used by the shaders to know which descriptor to access within the descriptor set this descriptor belongs to.
		 */
		uint32_t _bindingNumber;

		/**
		 * @brief Default constructor.
		 */
		Descriptor() = default;

		/**
		 * @brief Constructs a descriptor given a descriptor type and a buffer and/or image.
		 * @param type Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a
		 * structure that contains an image and some metadata that tells the GPU how to read it.
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 * @param pBuffer Optional pointer to a buffer.
		 * @param pBuffer Optional pointer to an image.
		 */
		Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, Buffer* pBuffer = nullptr, Image* pImage = nullptr);

	};

	/**
	 * @brief A descriptor set has bindings to descriptors, and is used to cluster descriptors. See description for Descriptor.
	 */
	class DescriptorSet
	{
		/**
		 * @brief Used for Vulkan calls.
		 */
		VkDevice _logicalDevice;

	public:

		/**
		 * @brief Identifier for Vulkan.
		 */
		VkDescriptorSet _handle;

		/**
		 * @brief A descriptor set layout object is defined by an array of zero or more descriptor bindings. Each individual descriptor binding is specified
		 * by a descriptor type, a count (array size) of the number of descriptors in the binding, a set of shader stages that can access the binding, and
		 * (if using immutable samplers) an array of sampler descriptors.
		 */
		VkDescriptorSetLayout _layout;

		/**
		 * @brief *NOT ACTUALLY USED ANYWHERE* Set index number used by the shaders to identify the descriptor set to access. When a pipeline is created,
		 * an object of VkPipelineLayout is required to create it. The VkPipelineLayout object contains an array of VkDescriptorSetLayout handles. This
		 * _indexNumber represents the index in that array where the _layout of this descriptor set is used. This is how in the shaders Vulkan knows which
		 * descriptor set you are linking. When you see (set = 2, binding = 3) it means that this descriptor set's layout was placed at index 3 in the array
		 * of VkDescriptorSetLayout handles when creating the pipeline currently in use.
		 */
		short _indexNumber;

		/**
		 * @brief Descriptors this set contains.
		 */
		std::vector<Descriptor*> _descriptors;

		/**
		 * @brief Writes the data contained in its descriptors to the correct GPU-visible allocated portion of memory.
		 */
		void SendToGPU();

		/**
		 * @brief Default constructor.
		 */
		DescriptorSet() = default;

		/**
		 * @brief Constructs a descriptor set.
		 * @param logicalDevice Logical device used to call Vulkan functions.
		 * @param shaderFlags Flags to define which shader/s can access this descriptor set.
		 * @param descriptors Descriptors. Must all be of the same type and compatible with the type of data they contain (image or buffer).
		 */
		DescriptorSet(VkDevice& logicalDevice, const VkShaderStageFlagBits& shaderStageFlags, std::vector<Descriptor*> pDescriptors);
	};

	/**
	 * @brief A descriptor pool acts as a facility to allocate memory for descriptor sets.
	 */
	class DescriptorPool
	{
		/**
		 * @brief Used to make Vulkan creation calls.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Descriptor sets the pool contains.
		 */
		std::vector<DescriptorSet*> _pDescriptorSets;

		/**
		 * @brief .
		 */
		void AllocateDescriptorSets();

	public:

		/**
		 * @brief Identifier for Vulkan.
		 */
		VkDescriptorPool _handle;

		/**
		 * @brief Default constructor.
		 */
		DescriptorPool() = default;

		/**
		 * @brief Constructs a descriptor pool and allocates GPU memory for the given descriptor sets.
		 * @param logicalDevice Used for the Vulkan call to create the descriptor pool and to allocate memory for the given descriptor sets.
		 * @param descriptorSets Descriptor sets to allocate memory for.
		 */
		DescriptorPool(VkDevice& logicalDevice, std::vector<DescriptorSet*> pDescriptorSets);

		/**
		 * @brief Updates a descriptor identified by its memory address with the data contained in the given buffer.
		 * @param data The buffer whose data will be sent to the allocated descriptor set.
		 */
		void UpdateDescriptor(Descriptor& d, Buffer& data);

		/**
		 * @brief Updates a descriptor identified by its memory address with the data contained in the given image.
		 * @param data The buffer whose data will be sent to the allocated descriptor set.
		 */
		void UpdateDescriptor(Descriptor& d, Image& data);
	};
}
#pragma once

#include <array>
#include <iostream>

namespace Engine::Vulkan
{
	class Descriptor;
	class DescriptorSet;
	class DescriptorPool;
}

namespace Engine::Structural
{
	/**
	 * @brief Represents a fixed-size array. Meant to be used as a class member as it has a default constructor and can be
	 * defined without knowing the size beforehand and can therefore be allocated in constructors or functions.
	 */
	template <typename T>
	class Array
	{
	public:

		/**
		 * @brief Size of the array.
		 */
		size_t _size;

		/**
		 * @brief Data contained in the array.
		 */
		T* _data;

		/**
		 * @brief Default constructor.
		 */
		Array() = default;

		/**
		 * @brief Allocates "size" instances of the array's underlying type.
		 */
		Array(size_t size)
		{
			_size = size;
			_data = new T[size];
		}

		/**
		 * @brief Operator overload so you can use the array with indexing.
		 * @param i
		 * @return 
		 */
		T& operator[](unsigned int i)
		{
			if (i > _size) {
				std::cout << "Index out of bounds" << std::endl;
				exit(-1);
			}

			return _data[i];
		}
	};

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
		 * @brief Buffers.
		 */
		Array<Vulkan::Buffer> _buffers;

		/**
		 * @brief Images.
		 */
		Array<Vulkan::Image> _images;

		/**
		 * @brief .
		 */
		Array<Vulkan::Descriptor> _descriptors;

		/**
		 * @brief .
		 */
		Array<Vulkan::DescriptorSet> _sets;

		/**
		 * @brief .
		 */
		Vulkan::DescriptorPool _pool;

		/**
		 * @brief Function that is meant for deriving classes to create shader resources and send them to GPU memory.
		 * @param physicalDevice Intended to be used to gather GPU information when allocating buffers or images.
		 * @param logicalDevice Intended to be used for binding created buffers, images, descriptors, descriptor sets etc. to the GPU.
		 */
		virtual void CreateShaderResources(Vulkan::PhysicalDevice& physicalDevice, VkDevice& logicalDevice) = 0;
	};
}

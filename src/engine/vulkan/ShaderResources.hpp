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
	 *		... shader code that uses uniformBuffer...
	 * }
	 *
	 * See the _bindingNumber member, and the _indexNumber member in the DescriptorSet class.
	 */
	class Descriptor
	{

	public:

		/**
		 * @brief The buffer the descriptor represents.
		 */
		std::optional<Buffer> _buffer = std::nullopt;

		/**
		 * @brief The image the descriptor represents.
		 */
		std::optional<Image> _image = std::nullopt;

		/**
		 * @brief Descriptor type: could be, for example, a uniform buffer (general data) or a texture sampler. A texture sampler is a
		 * structure that contains a pointer to an image and some metadata that tells the GPU how to read it.
		 */
		VkDescriptorType _type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

		/**
		 * @brief Binding number used by the shaders to know which descriptor to access within the descriptor set this descriptor belongs to.
		 */
		uint32_t _bindingNumber = 0;

		/**
		 * @brief Wrapper that adds some metadata for the buffer that the shaders need.
		 */
		std::optional<VkDescriptorBufferInfo> _bufferInfo;

		/**
		 * @brief Wraps all the information that the shaders need in order to fully use the image.
		 */
		std::optional<VkDescriptorImageInfo> _imageInfo;

		/**
		 * @brief Default constructor.
		 */
		Descriptor() = default;

		/**
		 * @brief Constructs a descriptor given a descriptor type and a buffer and image.
		 * @param type Descriptor type could be, for example, a uniform buffer (general data) or a combined image sampler. A combined image
		 * sampler is a flag that indicates that the descriptor contains both (hence the combine word) an image and some metadata that tells 
		 * the GPU how to read it.
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 * @param buffer Buffer.
		 * @param image Image.
		 */
		Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, const Buffer& buffer, const Image& image);

		/**
		 * @brief Constructs a descriptor given a descriptor type and a buffer.
		 * @param type Descriptor type could be, for example, a uniform buffer (general data) or a combined image sampler. A combined image
		 * sampler is a flag that indicates that the descriptor contains both (hence the combine word) an image and some metadata that tells 
		 * the GPU how to read it.
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 * @param buffer Buffer.
		 */
		Descriptor(const VkDescriptorType& type, const uint32_t& bindingNumber, const Buffer& buffer);

		/**
		 * @brief Constructs a descriptor given a descriptor type and an image.
		 * 
		 * @param type Descriptor type could be, for example, a uniform buffer (general data) or a combined image sampler. A combined image
		 * sampler is a flag that indicates that the descriptor contains both (hence the combine word) an image and some metadata that tells
		 * the GPU how to read it.
		 * 
		 * @param bindingNumber Binding number used by a shader to know which descriptor to access within a descriptor set.
		 * 
		 * @param image Image.
		 * 
		 * @param filteringMode Texture filtering is a parameter used by the shader when given the instruction to read the color of a texture at a specific UV coordinate. 
         * Positions on a texture are identified by integer values (pixel coordinates) whereas the texture() function in a shader takes float values. 
         * Say we have a 2x2 pixel image: if we sample from UV (0.35, 0.35) with origin at the bottom left corner, the texture() function will have to 
         * give back the color of the bottom left pixel, because UV coordinates (0.35, 0.35) fall in the bottom left pixel of our 2x2 image. However, 
         * provided that the image has only 1 sample per pixel, the exact center of that pixel is represented by UV coordinates (0.25, 0.25). UV coordinates 
         * (0.35, 0.35) are skewed towards the upper right of our bottom left pixel in our 2x2 image, so the texture() function uses the filtering parameter 
         * to determine how to calculate the color it gives back. In the case of linear filtering, the color the texture() function gives back will be a blend 
         * of the 4 closest pixels, weighted by how close the input coordinate (0.35, 0.35) is to each pixel, represented in this case by VK_FILTER_LINEAR.
         * 
         * @param addressMode This indicates how the sampler is going to behave when it receives coordinates that are out of the 0-1 UV range. For example,
         * VK_SAMPLER_ADDRESS_MODE_REPEAT will cause the sampler to give back the color of the texture at UV coordinates (0.25, 0.25) when given coordinates (1.25, 1.25), 
         * which mimics the textures being placed side by side, hence the REPEAT suffix.
         * 
         * @param anisotropyLevel Anisotropy is another, more advanced filtering technique that is most effective when the surface onto which the texture being sampled is at
         * a steep angle. It's aimed at preserving sharp features of textures, and maintaining visual fidelity when sampling textures at steep angles.
         * The word anisotropy comes from Greek and literally means "not the same angle".
         * The algorithm for anisotropic filtering looks like the following:
         * 
         * 1) We first need a way to know the angle of the texture. This could be easily achieved by passing the normal vector on from the vertex shader to
         * the fragment shader, but in most cases the derivatives of the texture are used. Texture derivatives represent the rate of change of texture
         * coordinates relative to the position of the pixel being rendered on-screen. To understand this, imagine you are rendering a plane that is
         * very steeply angled from your view, so steeply angled that you can only see 2 pixels, so it looks more like a line (but is infact the plane
         * being rendered almost from the side). If we take the texture coordinate of the 2 neighbouring pixels (on the shorter side) we will get the 2
         * extremes of the UV space. By comparing the rate of change of the on-screen pixel coordinates and the resulting texture coordinates, the shaders
         * can calculate the derivative that will be needed to sample around the main sampling position. In our extreme case, a 1 pixel change in position on screen
         * results in the 2 extremes of the UV space in the corresponding texture coordinates. This means that the magnitude of the derivative is at the
         * maximum it can be for one of the axes.
         * 
         * 2) Now that we have the magnitude of the derivative (the rate of change of the texture coordinates relative to the on-screen pixel coordinates)
         * we can start sampling around the color at the UV coordinate. In anisotropic filtering, the sampling is typically done with an ellyptical pattern
         * or with a cylindrical pattern. In the ellyptical pattern, the samples are taken around the UV coordinate following the line created by an
         * imaginary ellyptical circumference around the sample point. The number of samples will increase if the texture is on a very steeply-angled
         * surface. That's why we needed the angle of the texture, to guide the amount of samples being taken around the main sampling position.
         * 
         * 3) At this point we can calculate a weighted average of all colors that have been sampled around the main sampling position.
         * 
         * @param minLod Represents the minimum LOD (or mipmap). The fractional part of the floating-point values represents interpolation between adjacent 
         * mipmap levels. For example, if you set minLod to 1.5, the sampler may perform texture sampling by interpolating between LOD 1 and LOD 2, blending 
         * the textures from these two levels to get an intermediate level of detail.
         * 
         * @param maxLod Represents the minimum LOD (or mipmap).
         * 
         * @param mipMapMode VK_SAMPLER_MIPMAP_MODE_NEAREST: This mode specifies that the nearest mipmap level should be selected for sampling. 
         * When using this mode, the sampler will not perform any interpolation between mipmaps. Instead, it will directly use the texel data from the nearest 
         * mipmap level to the desired LOD (Level of Detail). This mode is useful when you want a sharp, blocky appearance for textures at different viewing 
         * distances or when using pixel art textures.
         * 
         * VK_SAMPLER_MIPMAP_MODE_LINEAR: This mode specifies that linear interpolation should be performed between mipmaps when sampling. The sampler will 
         * use a weighted average of texel data from two adjacent mipmap levels to obtain the final sampled color. Linear mipmap filtering provides smoother 
         * transitions between LODs and is commonly used to improve visual quality when textures are viewed at varying distances.
		 */
		Descriptor(const VkDescriptorType& type, 
			const uint32_t& bindingNumber, 
			const Image& image, 
			const VkFilter& filteringMode = VK_FILTER_NEAREST,
			const VkSamplerAddressMode& addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
			const float& anisotropyLevel = 0.0f,
			const float& minLod = 0.0f,
			const float& maxLod = 0.0f,
			const VkSamplerMipmapMode& mipMapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST);

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
		 * descriptor set you are linking. When you see (set = 2, binding = 3) it means that this descriptor set's layout was placed at index 2 (starting from 0,
		 * so in the third position) in the array of VkDescriptorSetLayout handles when creating the pipeline currently in use.
		 */
		short _indexNumber;

		/**
		 * @brief Descriptors this set contains.
		 */
		std::vector<Descriptor> _descriptors;

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
		DescriptorSet(VkDevice& logicalDevice, const VkShaderStageFlagBits& shaderStageFlags, std::vector<Descriptor> descriptors);
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
		std::vector<DescriptorSet>* _descriptorSets;

		/**
		 * @brief Allocates memory for the descriptor sets it points to.
		 */
		void AllocateDescriptorSets();

		/**
		 * @brief Sends each descriptor of each descriptor set to GPU-visible memory.
		 */
		void SendDescriptorSetDataToGPU();

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
		DescriptorPool(VkDevice& logicalDevice, std::vector<DescriptorSet>& descriptorSets);
	};
}
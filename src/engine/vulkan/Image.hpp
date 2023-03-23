#pragma once

namespace Engine::Vulkan
{
	/**
	 * @brief Represents a Vulkan image. A Vulkan image uses 2 structures, 1 for storing the data the image contain (VkImage),
	 * and 1 for decorating that data with metadata (VkImageView) that Vulkan can use in the graphics pipeline to know how to treat it.
	 */
	class Image
	{
		/**
		 * @brief See Vulkan specification.
		 */
		VkDevice _logicalDevice;

		/**
		 * @brief Physical device.
		 */
		PhysicalDevice _physicalDevice;

		/**
		 * @brief Gets the size of a pixel of the image using its format.
		 * @param format How the image is stored in memory.
		 * @return The size of a pixel in bytes if format is valid, 0 otherwise.
		 */
		size_t GetPixelSizeBytes(const VkFormat& format);


	public:

		/**
		 * @brief Handle that identifies a structure that contains the raw image data.
		 */
		VkImage _imageHandle;

		/**
		 * @brief Handle that identifies a structure that contains image metadata, as well as a pointer to the
		 * VkImage. See @see VkImage.
		 */
		VkImageView _imageViewHandle;

		/**
		 * @brief The way the data for each pixel of the image is stored in memory.
		 */
		VkFormat _format;

		/**
		 * @brief Width and height of the image in pixels.
		 */
		VkExtent2D _sizePixels;

		/**
		 * @brief Image size in bytes.
		 */
		size_t _sizeBytes;

		/**
		 * @brief Flags that indicate what type of data this image contains.
		 */
		VkImageAspectFlagBits _typeFlags;

		/**
		 * @brief Describes how this image is going to be read by the physical GPU texture samplers, which feed textures to shaders.
		 */
		VkSampler _sampler;

		/**
		 * @brief Raw data of the image.
		 */
		void* _pData;

		Image() = default;

		/**
		 * @brief Constructs a Vulkan image.
		 * @param logicalDevice
		 * @param physicalDevice
		 * @param imageFormat
		 * @param sizePixels Width and height of the image in pixels.
		 * @param data Pointer to where in memory the data for the image is stored. Use nullptr if the image is intended to be a renderpass attachment.
		 * @param usageFlags Gives Vulkan information about how it's going to be used as a render pass attachment. For example an image could be used to store depth information, denoted by using VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT.
		 * @param aspectFlags Indicates how it's meant to be used and what type of data it contains.
		 * @param memoryPropertiesFlags This can be any of the following values:
		 * 1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: this means GPU memory, so VRAM. If this is not set, then regular RAM is assumed.
		 * 2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: this means that the CPU will be able to read and write from the allocated memory IF YOU CALL vkMapMemory() FIRST.
		 * 3) VK_MEMORY_PROPERTY_HOST_CACHED_BIT: this means that the memory will be cached so that when the CPU writes to this buffer, if the data is small enough to fit in its
		 * cache (which is much faster to access) it will do that instead. Only problem is that this way, if your GPU needs to access that data, it won't be able to unless it's
		 * also marked as HOST_COHERENT.
		 * 4) VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: this means that anything that the CPU writes to the buffer will be able to be read by the GPU as well, effectively
		 * granting the GPU access to the CPU's cache (if the buffer is also marked as HOST_CACHED). COHERENT stands for consistency across memories, so it basically means
		 * that the CPU, GPU or any other device will see the same memory if trying to access the buffer. If you don't have this flag set, and you try to access the
		 * buffer from the GPU while the buffer is marked HOST_CACHED, you may not be able to get the data or even worse, you may end up reading the wrong chunk of memory.
		 * Further read: https://asawicki.info/news_1740_vulkan_memory_types_on_pc_and_how_to_use_them
		 */
		Image(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const VkFormat& imageFormat, const VkExtent2D& sizePixels, void* pData, const VkImageUsageFlagBits& usageFlags, const VkImageAspectFlagBits& typeFlags, const VkMemoryPropertyFlagBits& memoryPropertiesFlags);

		/**
		 * @brief Constructs an image using a pre-existing image handle.
		 * @param image The pre-existing image handle.
		 * @param imageFormat
		 * @param size
		 */
		Image(VkDevice& logicalDevice, VkImage& image, const VkFormat& imageFormat);

		/**
		 * @brief Sends the image to the GPU.
		 * @param commandPool Command pool used to allocate a command buffer that will contain the commands to send the image to VRAM.
		 * @param queue Queue that will contain the allocated transfer command buffer.
		 */
		void SendToGPU(VkCommandPool& commandPool, Queue& queue);

		/**
		 * @brief Generates an image descriptor. An image descriptor is bound to a sampler, which tells Vulkan how to instruct
		 * the actual GPU hardware samplers on how to read and sample the particular texture.
		 */
		VkDescriptorImageInfo GenerateDescriptor();

		/**
		 * @brief Creates a 1x1 pixels image with the one pixel being of the color specified with the red, blue, green and alpha values provided.
		 * @param logicalDevice Needed to create the image with Vulkan calls.
		 * @param physicalDevice Needed to create the image with Vulkan calls.
		 * @param red Red channel value, domain is 0-255.
		 * @param green Green channel value, domain is 0-255.
		 * @param blue Blue channel value, domain is 0-255.
		 * @param alpha Alpha channel value, domain is 0-255.
		 * @return 
		 */
		static Image* SolidColor(VkDevice& logicalDevice, PhysicalDevice& physicalDevice, const unsigned char& red, const unsigned char& green, const unsigned char& blue, const unsigned char& alpha);

		/**
		 * @brief Uses Vulkan calls to deallocate and remove the contents of the image from memory.
		 */
		void Destroy();
	};
}
